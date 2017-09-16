/*
 * Copyright (C) 2008 Till Harbaum <till@harbaum.org>.
 *
 * This file is part of OSM2Go.
 *
 * OSM2Go is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OSM2Go is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OSM2Go.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file diff.cpp generate and restore changes on the current data set
 */

#include "diff.h"

#include "appdata.h"
#include "misc.h"
#include "osm.h"
#include "project.h"
#include "statusbar.h"

#include <algorithm>
#include <cstring>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <unistd.h>

#include <osm2go_cpp.h>

#if !defined(LIBXML_TREE_ENABLED) || !defined(LIBXML_OUTPUT_ENABLED)
#error "libxml doesn't support required tree or output"
#endif

static std::string diff_filename(const project_t *project) {
  std::string ret = project->path;
  ret += project->name;
  ret += ".diff";
  return ret;
}

struct diff_save_tags_functor {
  xmlNodePtr const node;
  explicit diff_save_tags_functor(xmlNodePtr n) : node(n) {}
  void operator()(const tag_t &tag) {
    xmlNodePtr tag_node = xmlNewChild(node, O2G_NULLPTR,
                                      BAD_CAST "tag", O2G_NULLPTR);
    xmlNewProp(tag_node, BAD_CAST "k", BAD_CAST tag.key);
    xmlNewProp(tag_node, BAD_CAST "v", BAD_CAST tag.value);
  }
};

static void diff_save_tags(const base_object_t *obj, xmlNodePtr node) {
  diff_save_tags_functor fc(node);
  obj->tags.for_each(fc);
}

struct diff_save_objects {
  xmlNodePtr const root_node;
  explicit diff_save_objects(xmlNodePtr r) : root_node(r) {}
  /**
   * @brief save the common OSM object information
   * @param obj the object to save
   * @param tname the name of the XML node
   * @return XML node object
   */
  xmlNodePtr diff_save_state_n_id(const base_object_t *obj, const char *tname);
};

xmlNodePtr diff_save_objects::diff_save_state_n_id(const base_object_t *obj,
                                                   const char *tname) {
  xmlNodePtr node = xmlNewChild(root_node, O2G_NULLPTR, BAD_CAST tname, O2G_NULLPTR);

  if(obj->flags & OSM_FLAG_DELETED)
    xmlNewProp(node, BAD_CAST "state", BAD_CAST "deleted");
  else if(obj->isNew())
    xmlNewProp(node, BAD_CAST "state", BAD_CAST "new");

  /* all items need an id */
  xmlNewProp(node, BAD_CAST "id", BAD_CAST obj->id_string().c_str());

  return node;
}

struct diff_save_nodes : diff_save_objects {
  explicit diff_save_nodes(xmlNodePtr r) : diff_save_objects(r) { }
  void operator()(const std::pair<item_id_t, node_t *> pair);
};

void diff_save_nodes::operator()(const std::pair<item_id_t, node_t *> pair)
{
  const node_t * const node = pair.second;
  if(!node->flags)
    return;

  xmlNodePtr node_node = diff_save_state_n_id(node, node_t::api_string());

  if(node->flags & OSM_FLAG_DELETED)
    return;

  /* additional info is only required if the node hasn't been deleted */
  xml_set_prop_pos(node_node, &node->pos);

  diff_save_tags(node, node_node);
}

struct diff_save_ways : diff_save_objects {
  explicit diff_save_ways(xmlNodePtr r) : diff_save_objects(r) { }
  void operator()(const std::pair<item_id_t, way_t *> pair);
};

void diff_save_ways::operator()(const std::pair<item_id_t, way_t *> pair)
{
  const way_t * const way = pair.second;
  if(!way->flags)
    return;

  xmlNodePtr node_way = diff_save_state_n_id(way, way_t::api_string());

  if(way->flags & OSM_FLAG_HIDDEN)
    xmlNewProp(node_way, BAD_CAST "hidden", BAD_CAST "true");

  /* additional info is only required if the way hasn't been deleted */
  /* and of the dirty or new flags are set. (otherwise e.g. only */
  /* the hidden flag may be set) */
  if((!(way->flags & OSM_FLAG_DELETED)) &&
     (way->flags & OSM_FLAG_DIRTY)) {
    way->write_node_chain(node_way);
    diff_save_tags(way, node_way);
  }
}

struct diff_save_relations : diff_save_objects {
  explicit diff_save_relations(xmlNodePtr r) : diff_save_objects(r) { }
  void operator()(const std::pair<item_id_t, relation_t *> pair);
};

/* store all modfied relations */
void diff_save_relations::operator()(const std::pair<item_id_t, relation_t *> pair)
{
  const relation_t * const relation = pair.second;
  if(!relation->flags)
    return;

  xmlNodePtr node_rel = diff_save_state_n_id(relation, relation_t::api_string());

  if(relation->flags & OSM_FLAG_DELETED)
    return;

  /* additional info is only required if the relation */
  /* hasn't been deleted */
  relation->generate_member_xml(node_rel);

  diff_save_tags(relation, node_rel);
}

struct find_object_by_flags {
  int flagmask;
  explicit find_object_by_flags(int f = ~0) : flagmask(f) {}
  bool operator()(std::pair<item_id_t, base_object_t *> pair) {
    return pair.second->flags & flagmask;
  }
};

/* return true if no diff needs to be saved */
bool diff_is_clean(const osm_t *osm, bool honor_hidden_flags) {
  /* check if a diff is necessary */
  std::map<item_id_t, node_t *>::const_iterator nit =
    std::find_if(osm->nodes.begin(), osm->nodes.end(), find_object_by_flags());
  if(nit != osm->nodes.end())
    return false;

  int flagmask = honor_hidden_flags ? ~0 : ~OSM_FLAG_HIDDEN;
  std::map<item_id_t, way_t *>::const_iterator wit =
    std::find_if(osm->ways.begin(), osm->ways.end(), find_object_by_flags(flagmask));
  if(wit != osm->ways.end())
    return false;

  std::map<item_id_t, relation_t *>::const_iterator it =
    std::find_if(osm->relations.begin(), osm->relations.end(), find_object_by_flags());
  return it == osm->relations.end();
}

void diff_save(const project_t *project, const osm_t *osm) {
  const std::string &diff_name = diff_filename(project);

  if(diff_is_clean(osm, true)) {
    printf("data set is clean, removing diff if present\n");
    unlink(diff_name.c_str());
    return;
  }

  printf("data set is dirty, generating diff\n");

  /* write the diff to a new file so the original one needs intact until
   * saving is completed */
  const std::string ndiff = project->path + "save.diff";

  xmlDocPtr doc = xmlNewDoc(BAD_CAST "1.0");
  xmlNodePtr root_node = xmlNewNode(O2G_NULLPTR, BAD_CAST "diff");
  xmlNewProp(root_node, BAD_CAST "name", BAD_CAST project->name.c_str());
  xmlDocSetRootElement(doc, root_node);

  std::for_each(osm->nodes.begin(), osm->nodes.end(), diff_save_nodes(root_node));
  std::for_each(osm->ways.begin(), osm->ways.end(), diff_save_ways(root_node));
  std::for_each(osm->relations.begin(), osm->relations.end(), diff_save_relations(root_node));

  xmlSaveFormatFileEnc(ndiff.c_str(), doc, "UTF-8", 1);
  xmlFreeDoc(doc);

  /* if we reach this point writing the new file worked and we */
  /* can delete the backup */
  rename(ndiff.c_str(), diff_name.c_str());
}

static item_id_t xml_get_prop_int(xmlNode *node, const char *prop, item_id_t def) {
  xmlChar *str = xmlGetProp(node, BAD_CAST prop);
  item_id_t value = def;

  if(str) {
    value = strtoll(reinterpret_cast<char*>(str), O2G_NULLPTR, 10);
    xmlFree(str);
  }

  return value;
}

static int xml_get_prop_state(xmlNode *node) {
  xmlChar *str = xmlGetProp(node, BAD_CAST "state");

  if(str) {
    if(strcmp(reinterpret_cast<char *>(str), "new") == 0) {
      xmlFree(str);
      return OSM_FLAG_DIRTY;
    } else if(G_LIKELY(strcmp(reinterpret_cast<char *>(str), "deleted") == 0)) {
      xmlFree(str);
      return OSM_FLAG_DELETED;
    } else {
      xmlFree(str);
    }

    g_assert_not_reached();
  }

  return OSM_FLAG_DIRTY;
}

static osm_t::TagMap xml_scan_tags(xmlNodePtr node) {
  /* scan for tags */
  osm_t::TagMap  ret;

  while(node) {
    if(node->type == XML_ELEMENT_NODE) {
      if(G_LIKELY(strcmp(reinterpret_cast<const char *>(node->name), "tag") == 0))
        osm_t::parse_tag(node, ret);
    }
    node = node->next;
  }
  return ret;
}

static void diff_restore_node(xmlNodePtr node_node, osm_t *osm) {
  printf("Restoring node");

  /* read properties */
  item_id_t id = xml_get_prop_int(node_node, "id", ID_ILLEGAL);
  if(G_UNLIKELY(id == ID_ILLEGAL)) {
    printf("\n  Node entry missing id\n");
    return;
  }

  printf(" " ITEM_ID_FORMAT "\n", id);

  int state = xml_get_prop_state(node_node);
  pos_t pos = xml_get_prop_pos(node_node);
  bool pos_diff = pos.valid();

  if(G_UNLIKELY(!(state & OSM_FLAG_DELETED) && !pos_diff)) {
    printf("  Node not deleted, but no valid position\n");
    return;
  }

  /* evaluate properties */
  node_t *node = O2G_NULLPTR;

  switch(state) {
  case OSM_FLAG_DELETED:
    printf("  Restoring DELETE flag\n");

    if(G_LIKELY((node = osm->node_by_id(id)) != O2G_NULLPTR)) {
      node->flags |= OSM_FLAG_DELETED;
      return;
    } else {
      printf("  WARNING: no node with that id found\n");
      return;
    }

  case OSM_FLAG_DIRTY:
    if(id < 0) {
      printf("  Restoring NEW node\n");

      node = new node_t(0, lpos_t(), pos, id);

      osm->nodes[id] = node;
      break;
    } else {
      printf("  Valid id/position (DIRTY)\n");

      if(G_LIKELY((node = osm->node_by_id(id)) != O2G_NULLPTR)) {
        node->flags |= OSM_FLAG_DIRTY;
        if (node->pos == pos)
          pos_diff = false;
        else
          node->pos = pos;
        break;
      } else {
        printf("  WARNING: no node with that id found\n");
         return;
      }
    }

  default:
    printf("  Illegal state entry %u\n", state);
    return;
  }

  g_assert_nonnull(node);

  osm_t::TagMap ntags = xml_scan_tags(node_node->children);
  /* check if the same changes have been done upstream */
  if(state == OSM_FLAG_DIRTY && !pos_diff && node->tags == ntags) {
    printf("node " ITEM_ID_FORMAT " has the same values and position as upstream, discarding diff\n", id);
    node->flags &= ~OSM_FLAG_DIRTY;
    return;
  }

  node->tags.replace(ntags);

  /* update screen position, the absolute position has already been changed */
  if(pos_diff)
    node->lpos = node->pos.toLpos(*(osm->bounds));
}

static void diff_restore_way(xmlNodePtr node_way, osm_t *osm) {
  printf("Restoring way");

  item_id_t id = xml_get_prop_int(node_way, "id", ID_ILLEGAL);
  if(G_UNLIKELY(id == ID_ILLEGAL)) {
    printf("\n  entry missing id\n");
    return;
  }

  printf(" " ITEM_ID_FORMAT "\n", id);

  int state = xml_get_prop_state(node_way);

  /* evaluate properties */
  way_t *way = O2G_NULLPTR;
  switch(state) {

  case OSM_FLAG_DELETED:
    printf("  Restoring DELETE flag\n");

    if(G_LIKELY((way = osm->way_by_id(id)) != O2G_NULLPTR)) {
      way->flags |= OSM_FLAG_DELETED;
      return;
    } else {
      printf("  WARNING: no way with that id found\n");
      return;
    }

  case OSM_FLAG_DIRTY:
    if(id < 0) {
      printf("  Restoring NEW way\n");

      way = new way_t(0, id);

      osm->ways[id] = way;
      break;
    } else {
      printf("  Valid id (DIRTY)\n");

      if(G_LIKELY((way = osm->way_by_id(id)) != O2G_NULLPTR)) {
        way->flags |= OSM_FLAG_DIRTY;
        break;
      } else {
        printf("  WARNING: no way with that id found\n");
        return;
      }
    }

  default:
    printf("  Illegal state entry %u\n", state);
    return;
  }

  g_assert_nonnull(way);

  /* handle hidden flag */
  if(xml_get_prop_is(node_way, "hidden", "true"))
    way->flags |= OSM_FLAG_HIDDEN;

  /* update node_chain */
  /* scan for nodes */
  node_chain_t new_chain;
  xmlNode *nd_node = O2G_NULLPTR;
  for(nd_node = node_way->children; nd_node; nd_node = nd_node->next) {
    if(nd_node->type == XML_ELEMENT_NODE) {
      if(G_LIKELY(strcmp(reinterpret_cast<const char *>(nd_node->name), "nd") == 0)) {
	/* attach node to node_chain */
	node_t *tmp = osm->parse_way_nd(nd_node);
	if(tmp)
	  new_chain.push_back(tmp);
      }
    }
  }

  /* only replace the original nodes if new nodes have actually been */
  /* found. */
  if(!new_chain.empty()) {
    bool sameChain = (way->node_chain == new_chain);
    // it doesn't matter which chain is kept if they are the same, so just
    // always swap as that keeps the code simpler
    way->node_chain.swap(new_chain);
    osm_node_chain_free(new_chain);
    new_chain.clear();

    osm_t::TagMap ntags = xml_scan_tags(node_way->children);
    if (way->tags != ntags) {
      way->tags.replace(ntags);
    } else if (!ntags.empty()) {
      if (sameChain) {
        printf("way " ITEM_ID_FORMAT " has the same nodes and tags as upstream, discarding diff\n", id);
        way->flags &= ~OSM_FLAG_DIRTY;
      }
    }
  } else {
    /* only replace tags if nodes have been found before. if no nodes */
    /* were found this wasn't a dirty entry but e.g. only the hidden */
    /* flag had been set */
    printf("  no nodes restored, way isn't dirty!\n");
    way->flags &= ~OSM_FLAG_DIRTY;
  }
}

static void diff_restore_relation(xmlNodePtr node_rel, osm_t *osm) {
  printf("Restoring relation");

  item_id_t id = xml_get_prop_int(node_rel, "id", ID_ILLEGAL);
  if(G_UNLIKELY(id == ID_ILLEGAL)) {
    printf("\n  entry missing id\n");
    return;
  }

  printf(" " ITEM_ID_FORMAT "\n", id);

  int state = xml_get_prop_state(node_rel);

  /* evaluate properties */
  relation_t *relation = O2G_NULLPTR;
  switch(state) {
  case OSM_FLAG_DELETED:
    printf("  Restoring DELETE flag\n");

    if(G_LIKELY((relation = osm->relation_by_id(id)) != O2G_NULLPTR)) {
      relation->flags |= OSM_FLAG_DELETED;
      return;
    } else {
      printf("  WARNING: no relation with that id found\n");
      return;
    }

  case OSM_FLAG_DIRTY:
    if(id < 0) {
      printf("  Restoring NEW relation\n");

      relation = new relation_t(0, id);

      osm->relations[id] = relation;
      break;
    } else {
      printf("  Valid id (DIRTY)\n");

      if(G_LIKELY((relation = osm->relation_by_id(id)) != O2G_NULLPTR)) {
        relation->flags |= OSM_FLAG_DIRTY;
        break;
      } else {
        printf("  WARNING: no relation with that id found\n");
        return;
      }
    }

  default:
    printf("  Illegal state entry %u\n", state);
    return;
  }

  g_assert_nonnull(relation);

  bool was_changed = false;
  osm_t::TagMap ntags = xml_scan_tags(node_rel->children);
  if(relation->tags != ntags) {
    relation->tags.replace(ntags);
    was_changed = true;
  }

  /* update members */

  /* scan for members */
  std::vector<member_t> members;
  xmlNode *member_node = O2G_NULLPTR;
  for(member_node = node_rel->children; member_node;
      member_node = member_node->next) {
    if(member_node->type == XML_ELEMENT_NODE) {
      if(G_LIKELY(strcmp(reinterpret_cast<const char *>(member_node->name), "member") == 0)) {
	/* attach member to member_chain */
        osm->parse_relation_member(member_node, members);
      }
    }
  }

  if(relation->members != members) {
    /* this may be an existing relation, so remove members to */
    /* make space for new ones */
    relation->members.swap(members);
    was_changed = true;
  }
  osm_members_free(members);

  if(!was_changed && (relation->flags & OSM_FLAG_DIRTY)) {
    printf("relation " ITEM_ID_FORMAT " has the same members and tags as upstream, discarding diff\n", id);
    relation->flags &= ~OSM_FLAG_DIRTY;
  }
}

unsigned int diff_restore_file(GtkWidget *window, const project_t *project, osm_t *osm) {
  /* first try to open a backup which is only present if saving the */
  /* actual diff didn't succeed */
  std::string diff_name = project->path;
  diff_name += "backup.diff";
  if(G_UNLIKELY(g_file_test(diff_name.c_str(), G_FILE_TEST_IS_REGULAR) == TRUE)) {
    printf("diff backup present, loading it instead of real diff ...\n");
  } else {
    diff_name = diff_filename(project);

    if(g_file_test(diff_name.c_str(), G_FILE_TEST_IS_REGULAR) != TRUE) {
      printf("no diff present!\n");
      return DIFF_NONE_PRESENT;
    }
    printf("diff found, applying ...\n");
  }

  xmlDoc *doc = O2G_NULLPTR;
  xmlNode *root_element = O2G_NULLPTR;

  /* parse the file and get the DOM */
  if(G_UNLIKELY((doc = xmlReadFile(diff_name.c_str(), O2G_NULLPTR, 0)) == O2G_NULLPTR)) {
    errorf(window, _("Error: could not parse file %s\n"), diff_name.c_str());
    return DIFF_INVALID;
  }

  unsigned int res = DIFF_RESTORED;

  /* Get the root element node */
  root_element = xmlDocGetRootElement(doc);

  xmlNode *cur_node = O2G_NULLPTR;
  for (cur_node = root_element; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if(strcmp(reinterpret_cast<const char *>(cur_node->name), "diff") == 0) {
	xmlChar *str = xmlGetProp(cur_node, BAD_CAST "name");
	if(str) {
          const char *cstr = reinterpret_cast<const char *>(str);
	  printf("diff for project %s\n", cstr);
	  if(G_UNLIKELY(project->name != cstr)) {
            warningf(window, _("Diff name (%s) does not match project name (%s)"),
		     cstr, project->name.c_str());
            res |= DIFF_PROJECT_MISMATCH;
	  }
	  xmlFree(str);
	}

        for(xmlNodePtr node_node = cur_node->children; node_node != O2G_NULLPTR;
            node_node = node_node->next) {
	  if(node_node->type == XML_ELEMENT_NODE) {

            if(strcmp(reinterpret_cast<const char *>(node_node->name), node_t::api_string()) == 0)
	      diff_restore_node(node_node, osm);

            else if(strcmp(reinterpret_cast<const char *>(node_node->name), way_t::api_string()) == 0)
	      diff_restore_way(node_node, osm);

            else if(G_LIKELY(strcmp(reinterpret_cast<const char *>(node_node->name), relation_t::api_string()) == 0))
	      diff_restore_relation(node_node, osm);

            else {
	      printf("WARNING: item %s not restored\n", node_node->name);
              res |= DIFF_ELEMENTS_IGNORED;
            }
	  }
	}
      }
    }
  }

  xmlFreeDoc(doc);

  /* check for hidden ways and update menu accordingly */
  const std::map<item_id_t, way_t *>::const_iterator it =
      std::find_if(osm->ways.begin(), osm->ways.end(), find_object_by_flags(OSM_FLAG_HIDDEN));

  if(it != osm->ways.end())
    res |= DIFF_HAS_HIDDEN;

  return res;
}

void diff_restore(appdata_t &appdata) {
  if(G_UNLIKELY(!appdata.osm))
    return;

  unsigned int flags = diff_restore_file(GTK_WIDGET(appdata.window), appdata.project, appdata.osm);
  if(flags & DIFF_HAS_HIDDEN) {
    printf("hidden flags have been restored, enable show_add menu\n");

    appdata.statusbar->set(_("Some objects are hidden"), true);
    gtk_widget_set_sensitive(appdata.menuitems[MENU_ITEM_MAP_SHOW_ALL], TRUE);
  }
}

bool diff_present(const project_t *project) {
  const std::string &diff_name = diff_filename(project);

  return g_file_test(diff_name.c_str(), G_FILE_TEST_IS_REGULAR) == TRUE;
}

void diff_remove(const project_t *project) {
  const std::string &diff_name = diff_filename(project);
  unlink(diff_name.c_str());
}

xmlDocPtr osmchange_init()
{
  xmlDocPtr doc = xmlNewDoc(BAD_CAST "1.0");
  xmlNodePtr root_node = xmlNewNode(O2G_NULLPTR, BAD_CAST "osmChange");
  xmlNewProp(root_node, BAD_CAST "generator", BAD_CAST "OSM2go v" VERSION);
  xmlDocSetRootElement(doc, root_node);

  return doc;
}

struct osmchange_delete_functor {
  xmlNodePtr const xml_node; ///< <delete> node
  const char * const changeset; ///< changeset string
  osmchange_delete_functor(xmlNodePtr delnode, const char *cs) : xml_node(delnode), changeset(cs) {}
  void operator()(const base_object_t *obj) {
    obj->osmchange_delete(xml_node, changeset);
  }
};

void osmchange_delete(const osm_t::dirty_t &dirty, xmlNodePtr xml_node, const char *changeset)
{
  xmlNodePtr del_node = xmlNewChild(xml_node, O2G_NULLPTR, BAD_CAST "delete", O2G_NULLPTR);
  osmchange_delete_functor fc(del_node, changeset);

  std::for_each(dirty.relations.deleted.begin(), dirty.relations.deleted.end(), fc);
  std::for_each(dirty.ways.deleted.begin(), dirty.ways.deleted.end(), fc);
  std::for_each(dirty.nodes.deleted.begin(), dirty.nodes.deleted.end(), fc);
}
