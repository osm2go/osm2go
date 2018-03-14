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
 * along with OSM2Go.  If not, see <https://www.gnu.org/licenses/>.
 */

/**
 * @file diff.cpp generate and restore changes on the current data set
 */

#include "diff.h"

#include "appdata.h"
#include "fdguard.h"
#include "misc.h"
#include "osm.h"
#include "project.h"
#include "uicontrol.h"
#include "xml_helpers.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <fcntl.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <sys/stat.h>
#include <unistd.h>

#include "osm2go_annotations.h"
#include <osm2go_cpp.h>
#include <osm2go_i18n.h>

#if !defined(LIBXML_TREE_ENABLED) || !defined(LIBXML_OUTPUT_ENABLED)
#error "libxml doesn't support required tree or output"
#endif

static std::string diff_filename(const project_t *project) {
  return project->name + ".diff";
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
  node->pos.toXmlProperties(node_node);

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
  if(!(way->flags & OSM_FLAG_DELETED) && (way->flags & OSM_FLAG_DIRTY)) {
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

void diff_save(const project_t *project) {
  if(unlikely(project->osm == O2G_NULLPTR))
    return;

  const std::string &diff_name = diff_filename(project);

  osm_t *osm = project->osm;
  if(osm->is_clean(true)) {
    printf("data set is clean, removing diff if present\n");
    unlinkat(project->dirfd, diff_name.c_str(), 0);
    return;
  }

  printf("data set is dirty, generating diff\n");

  /* write the diff to a new file so the original one needs intact until
   * saving is completed */
  const std::string ndiff = project->path + "save.diff";

  std::unique_ptr<xmlDoc, xmlDocDelete> doc(xmlNewDoc(BAD_CAST "1.0"));
  xmlNodePtr root_node = xmlNewNode(O2G_NULLPTR, BAD_CAST "diff");
  xmlNewProp(root_node, BAD_CAST "name", BAD_CAST project->name.c_str());
  xmlDocSetRootElement(doc.get(), root_node);

  std::for_each(osm->nodes.begin(), osm->nodes.end(), diff_save_nodes(root_node));
  std::for_each(osm->ways.begin(), osm->ways.end(), diff_save_ways(root_node));
  std::for_each(osm->relations.begin(), osm->relations.end(), diff_save_relations(root_node));

  xmlSaveFormatFileEnc(ndiff.c_str(), doc.get(), "UTF-8", 1);

  /* if we reach this point writing the new file worked and we */
  /* can move it over the real file */
  renameat(-1, ndiff.c_str(), project->dirfd, diff_name.c_str());
}

static item_id_t xml_get_prop_int(xmlNode *node, const char *prop, item_id_t def) {
  xmlString str(xmlGetProp(node, BAD_CAST prop));

  if(str)
    return strtoll(reinterpret_cast<char*>(str.get()), O2G_NULLPTR, 10);
  else
    return def;
}

static int xml_get_prop_state(xmlNode *node) {
  xmlString str(xmlGetProp(node, BAD_CAST "state"));

  if(!str)
    return OSM_FLAG_DIRTY;
  else if(strcmp(reinterpret_cast<char *>(str.get()), "new") == 0)
    return OSM_FLAG_DIRTY;
  else if(likely(strcmp(reinterpret_cast<char *>(str.get()), "deleted") == 0))
    return OSM_FLAG_DELETED;

  assert_unreachable();
}

static osm_t::TagMap xml_scan_tags(xmlNodePtr node) {
  /* scan for tags */
  osm_t::TagMap  ret;

  for(; node != O2G_NULLPTR; node = node->next) {
    if(node->type == XML_ELEMENT_NODE) {
      if(likely(strcmp(reinterpret_cast<const char *>(node->name), "tag") == 0))
        osm_t::parse_tag(node, ret);
    }
  }
  return ret;
}

static void diff_restore_node(xmlNodePtr node_node, osm_t *osm) {
  printf("Restoring node");

  /* read properties */
  item_id_t id = xml_get_prop_int(node_node, "id", ID_ILLEGAL);
  if(unlikely(id == ID_ILLEGAL)) {
    printf("\n  Node entry missing id\n");
    return;
  }

  printf(" " ITEM_ID_FORMAT "\n", id);

  int state = xml_get_prop_state(node_node);
  pos_t pos = pos_t::fromXmlProperties(node_node);
  bool pos_diff = pos.valid();

  if(unlikely(!(state & OSM_FLAG_DELETED) && !pos_diff)) {
    printf("  Node not deleted, but no valid position\n");
    return;
  }

  /* evaluate properties */
  node_t *node;

  switch(state) {
  case OSM_FLAG_DELETED:
    printf("  Restoring DELETE flag\n");

    node = osm->node_by_id(id);
    if(likely(node != O2G_NULLPTR))
      node->flags |= OSM_FLAG_DELETED;
    else
      printf("  WARNING: no node with that id found\n");
    return;

  case OSM_FLAG_DIRTY:
    if(id < 0) {
      printf("  Restoring NEW node\n");

      node = new node_t(0, lpos_t(), pos, id);

      osm->nodes[id] = node;
      break;
    } else {
      printf("  Valid id/position (DIRTY)\n");

      node = osm->node_by_id(id);
      if(likely(node != O2G_NULLPTR)) {
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
    node->lpos = node->pos.toLpos(osm->bounds);
}

static void diff_restore_way(xmlNodePtr node_way, osm_t *osm) {
  printf("Restoring way");

  item_id_t id = xml_get_prop_int(node_way, "id", ID_ILLEGAL);
  if(unlikely(id == ID_ILLEGAL)) {
    printf("\n  entry missing id\n");
    return;
  }

  printf(" " ITEM_ID_FORMAT "\n", id);

  int state = xml_get_prop_state(node_way);

  /* evaluate properties */
  way_t *way;
  switch(state) {

  case OSM_FLAG_DELETED:
    printf("  Restoring DELETE flag\n");

    way = osm->way_by_id(id);
    if(likely(way != O2G_NULLPTR))
      way->flags |= OSM_FLAG_DELETED;
    else
      printf("  WARNING: no way with that id found\n");
    return;

  case OSM_FLAG_DIRTY:
    if(id < 0) {
      printf("  Restoring NEW way\n");

      way = new way_t(0, id);

      osm->ways[id] = way;
      break;
    } else {
      printf("  Valid id (DIRTY)\n");

      way = osm->way_by_id(id);
      if(likely(way != O2G_NULLPTR)) {
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

  /* handle hidden flag */
  if(xml_get_prop_bool(node_way, "hidden"))
    way->flags |= OSM_FLAG_HIDDEN;

  /* update node_chain */
  /* scan for nodes */
  node_chain_t new_chain;
  xmlNode *nd_node = O2G_NULLPTR;
  for(nd_node = node_way->children; nd_node; nd_node = nd_node->next) {
    if(nd_node->type == XML_ELEMENT_NODE) {
      if(likely(strcmp(reinterpret_cast<const char *>(nd_node->name), "nd") == 0)) {
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
  if(unlikely(id == ID_ILLEGAL)) {
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

    relation = osm->relation_by_id(id);
    if(likely(relation != O2G_NULLPTR))
      relation->flags |= OSM_FLAG_DELETED;
    else
      printf("  WARNING: no relation with that id found\n");
    return;

  case OSM_FLAG_DIRTY:
    if(id < 0) {
      printf("  Restoring NEW relation\n");

      relation = new relation_t(0, id);

      osm->relations[id] = relation;
      break;
    } else {
      printf("  Valid id (DIRTY)\n");

      relation = osm->relation_by_id(id);
      if(likely(relation != O2G_NULLPTR)) {
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
      if(likely(strcmp(reinterpret_cast<const char *>(member_node->name), "member") == 0)) {
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

unsigned int diff_restore_file(const project_t *project) {
  struct stat st;
  osm_t * const osm = project->osm;

  /* first try to open a backup which is only present if saving the */
  /* actual diff didn't succeed */
  const char *backupfn = "backup.diff";
  std::string diff_name;
  if(unlikely(fstatat(project->dirfd, backupfn, &st, 0) == 0 && S_ISREG(st.st_mode))) {
    printf("diff backup present, loading it instead of real diff ...\n");
    diff_name = backupfn;
  } else {
    diff_name = diff_filename(project);

    if(fstatat(project->dirfd, diff_name.c_str(), &st, 0) != 0 || !S_ISREG(st.st_mode)) {
      printf("no diff present!\n");
      return DIFF_NONE_PRESENT;
    }
    printf("diff found, applying ...\n");
  }

  xmlNode *root_element = O2G_NULLPTR;

  fdguard difffd(project->dirfd, diff_name.c_str(), O_RDONLY);

  /* parse the file and get the DOM */
  std::unique_ptr<xmlDoc, xmlDocDelete> doc(xmlReadFd(difffd, O2G_NULLPTR, O2G_NULLPTR, XML_PARSE_NONET));
  if(unlikely(!doc)) {
    errorf(O2G_NULLPTR, _("Error: could not parse file %s\n"), diff_name.c_str());
    return DIFF_INVALID;
  }

  unsigned int res = DIFF_RESTORED;

  /* Get the root element node */
  root_element = xmlDocGetRootElement(doc.get());

  xmlNode *cur_node = O2G_NULLPTR;
  for (cur_node = root_element; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if(strcmp(reinterpret_cast<const char *>(cur_node->name), "diff") == 0) {
        xmlString str(xmlGetProp(cur_node, BAD_CAST "name"));
	if(str) {
          const char *cstr = reinterpret_cast<const char *>(str.get());
	  printf("diff for project %s\n", cstr);
	  if(unlikely(project->name != cstr)) {
            warningf(O2G_NULLPTR, _("Diff name (%s) does not match project name (%s)"),
		     cstr, project->name.c_str());
            res |= DIFF_PROJECT_MISMATCH;
	  }
	}

        for(xmlNodePtr node_node = cur_node->children; node_node != O2G_NULLPTR;
            node_node = node_node->next) {
	  if(node_node->type == XML_ELEMENT_NODE) {

            if(strcmp(reinterpret_cast<const char *>(node_node->name), node_t::api_string()) == 0)
	      diff_restore_node(node_node, osm);

            else if(strcmp(reinterpret_cast<const char *>(node_node->name), way_t::api_string()) == 0)
	      diff_restore_way(node_node, osm);

            else if(likely(strcmp(reinterpret_cast<const char *>(node_node->name), relation_t::api_string()) == 0))
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

  /* check for hidden ways and update menu accordingly */
  if(osm->ways.end() != std::find_if(osm->ways.begin(), osm->ways.end(),
                                     osm_t::find_object_by_flags(OSM_FLAG_HIDDEN)))
    res |= DIFF_HAS_HIDDEN;

  return res;
}

void diff_restore(appdata_t &appdata) {
  if(unlikely(appdata.project->osm == O2G_NULLPTR))
    return;

  unsigned int flags = diff_restore_file(appdata.project);
  if(flags & DIFF_HAS_HIDDEN) {
    printf("hidden flags have been restored, enable show_add menu\n");

    appdata.uicontrol->showNotification(_("Some objects are hidden"), MainUi::Highlight);
    appdata.uicontrol->setActionEnable(MainUi::MENU_ITEM_MAP_SHOW_ALL, true);
  }
}

bool diff_present(const project_t *project) {
  struct stat st;
  return fstatat(project->dirfd, diff_filename(project).c_str(), &st, 0) == 0 && S_ISREG(st.st_mode);
}

void diff_remove(const project_t *project) {
  unlinkat(project->dirfd, diff_filename(project).c_str(), 0);
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
