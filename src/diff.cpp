/*
 * SPDX-FileCopyrightText: 2008 Till Harbaum <till@harbaum.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file diff.cpp generate and restore changes on the current data set
 */

#include "diff.h"

#include "fdguard.h"
#include "misc.h"
#include "notifications.h"
#include "osm.h"
#include "osm_objects.h"
#include "project.h"
#include "uicontrol.h"

#include <algorithm>
#include <cassert>
#include <cerrno>
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

namespace {

std::string
diff_filename(const project_t *project)
{
  return project->name + ".diff";
}

std::string
project_diff_name(const project_t *project)
{
  struct stat st;

  /* first try to open a backup which is only present if saving the */
  /* actual diff didn't succeed */
  const char *backupfn = "backup.diff";
  std::string diff_name;
  if(unlikely(fstatat(project->dirfd, backupfn, &st, 0) == 0 && S_ISREG(st.st_mode))) {
    diff_name = backupfn;
  } else {
    diff_name = diff_filename(project);

    if(fstatat(project->dirfd, diff_name.c_str(), &st, 0) != 0 || !S_ISREG(st.st_mode))
      diff_name.clear();
  }

  return diff_name;
}

struct diff_save_tags_functor {
  xmlNode * const node;
  explicit inline diff_save_tags_functor(xmlNodePtr n) : node(n) {}
  void operator()(const tag_t &tag) {
    xmlNodePtr tag_node = xmlNewChild(node, nullptr,
                                      BAD_CAST "tag", nullptr);
    xmlNewProp(tag_node, BAD_CAST "k", BAD_CAST tag.key);
    xmlNewProp(tag_node, BAD_CAST "v", BAD_CAST tag.value);
  }
};

void
diff_save_tags(const base_object_t *obj, xmlNodePtr node)
{
  diff_save_tags_functor fc(node);
  obj->tags.for_each(fc);
}

/**
  * @brief save the common OSM object information
  * @param root_node the XML node to append to
  * @param obj the object to save
  * @param tname the name of the XML node
  * @return XML node object
  */
xmlNodePtr
diff_save_state_n_id(xmlNodePtr root_node, const base_object_t *obj, const char *tname)
{
  xmlNodePtr node = xmlNewChild(root_node, nullptr, BAD_CAST tname, nullptr);

  if(obj->isDeleted())
    xmlNewProp(node, BAD_CAST "state", BAD_CAST "deleted");
  else if(obj->isNew())
    xmlNewProp(node, BAD_CAST "state", BAD_CAST "new");

  /* all items need an id */
  xmlNewProp(node, BAD_CAST "id", BAD_CAST obj->id_string().c_str());

  return node;
}

template<typename T ENABLE_IF_CONVERTIBLE(T *, base_object_t *)>
class diff_save_objects {
  xmlNode * const root_node;
  inline void save_additional_info(const T *m, xmlNodePtr xmlnode) const;
public:
  explicit inline diff_save_objects(xmlNodePtr r) : root_node(r) {}
  void operator()(const std::pair<item_id_t, T *> &pair) {
    if(!pair.second->isDirty())
      return;

    xmlNodePtr xmlnode = diff_save_state_n_id(root_node, pair.second, T::api_string());

    if(pair.second->isDeleted())
      return;

    /* additional info is only required if the object hasn't been deleted */
    save_additional_info(pair.second, xmlnode);

    diff_save_tags(pair.second, xmlnode);
  }
};

template<>
void diff_save_objects<node_t>::save_additional_info(const node_t *m, xmlNodePtr xmlnode) const
{
  m->pos.toXmlProperties(xmlnode);
}

struct diff_save_ways {
  xmlNode * const root_node;
  osm_t::ref osm;
  explicit inline diff_save_ways(xmlNodePtr r, osm_t::ref o) : root_node(r), osm(o) { }
  void operator()(const std::pair<item_id_t, way_t *> &pair);
};

void diff_save_ways::operator()(const std::pair<item_id_t, way_t *> &pair)
{
  const way_t * const way = pair.second;
  bool hidden = osm->wayIsHidden(way);
  if(!way->isDirty() && !hidden)
    return;

  xmlNodePtr node_way = diff_save_state_n_id(root_node, way, way_t::api_string());

  if(way->isDeleted())
    return;

  if(hidden)
    xmlNewProp(node_way, BAD_CAST "hidden", BAD_CAST "true");

  /* additional info is only required if the way hasn't been deleted */
  /* and if the dirty flags is set. (otherwise e.g. only the hidden
   * flag may be set) */
  if(way->flags & OSM_FLAG_DIRTY) {
    way->write_node_chain(node_way);
    diff_save_tags(way, node_way);
  }
}

template<>
void diff_save_objects<relation_t>::save_additional_info(const relation_t *m, xmlNodePtr xmlnode) const
{
  m->generate_member_xml(xmlnode);
}

} // namespace

void project_t::diff_save() const {
  if(unlikely(!osm))
    return;

  if(osm->is_clean(true)) {
    printf("data set is clean, removing diff if present\n");
    diff_remove_file();
    return;
  }

  const std::string &diff_name = diff_filename(this);

  printf("data set is dirty, generating diff\n");

  /* write the diff to a new file so the original one needs intact until
   * saving is completed */
  const std::string ndiff = path + "save.diff";

  xmlDocGuard doc(xmlNewDoc(BAD_CAST "1.0"));
  xmlNodePtr root_node = xmlNewNode(nullptr, BAD_CAST "diff");
  xmlNewProp(root_node, BAD_CAST "name", BAD_CAST name.c_str());
  xmlDocSetRootElement(doc.get(), root_node);

  std::for_each(osm->nodes.begin(), osm->nodes.end(), diff_save_objects<node_t>(root_node));
  std::for_each(osm->ways.begin(), osm->ways.end(), diff_save_ways(root_node, osm));
  std::for_each(osm->relations.begin(), osm->relations.end(), diff_save_objects<relation_t>(root_node));

  xmlSaveFormatFileEnc(ndiff.c_str(), doc.get(), "UTF-8", 1);

  /* if we reach this point writing the new file worked and we */
  /* can move it over the real file */
  if(renameat(-1, ndiff.c_str(), dirfd, diff_name.c_str()) != 0)
    fprintf(stderr, "error %i when moving '%s' to '%s'\n", errno, ndiff.c_str(), diff_name.c_str());
}

namespace {

item_id_t
xml_get_prop_int(xmlNode *node, const char *prop, item_id_t def)
{
  xmlString str(xmlGetProp(node, BAD_CAST prop));

  if(str)
    return strtoll(str, nullptr, 10);
  else
    return def;
}

int
xml_get_prop_state(xmlNode *node)
{
  xmlString str(xmlGetProp(node, BAD_CAST "state"));

  if(!str)
    return OSM_FLAG_DIRTY;
  else if(strcmp(str, "new") == 0)
    return OSM_FLAG_DIRTY;
  else if(likely(strcmp(str, "deleted") == 0))
    return OSM_FLAG_DELETED;

  printf("  Illegal state entry '%s'\n", static_cast<const char *>(str));

  return -1;
}

osm_t::TagMap
xml_scan_tags(xmlNodePtr node)
{
  /* scan for tags */
  osm_t::TagMap  ret;

  for(; node != nullptr; node = node->next) {
    if(node->type == XML_ELEMENT_NODE) {
      if(likely(strcmp(reinterpret_cast<const char *>(node->name), "tag") == 0))
        osm_t::parse_tag(node, ret);
    }
  }
  return ret;
}

void deleteDiffObject(osm_t::ref osm, node_t *n)
{
  // don't touch the reference count here: if this node was part of a way is needs to be
  // removed from it in the diff already.
  osm->node_delete(n, osm_t::NodeDeleteKeepRefs);
}

/**
 * @brief decrement the reference count of nodes
 *
 * Don't do any other processing here, this has to be part of the diff already.
 */
void diffWaySimpleUnref(node_t *node)
{
  assert_cmpnum_op(node->ways, >, 0);
  node->ways--;
}

void deleteDiffObject(osm_t::ref osm, way_t *w)
{
  // the diff is loaded before the map is drawn, so no map pointer is needed at this point
  osm->way_delete(w, nullptr, diffWaySimpleUnref);
}

void deleteDiffObject(osm_t::ref osm, relation_t *r)
{
  osm->relation_delete(r);
}

template<typename T ENABLE_IF_CONVERTIBLE(T *, base_object_t *)>
T *restore_object(xmlNodePtr xml_node, osm_t::ref osm)
{
  printf("Restoring %s", T::api_string());

  /* read properties */
  item_id_t id = xml_get_prop_int(xml_node, "id", ID_ILLEGAL);
  if(unlikely(id == ID_ILLEGAL)) {
    printf("\n  %s entry missing id\n", T::api_string());
    return nullptr;
  }

  printf(" " ITEM_ID_FORMAT "\n", id);

  /* evaluate properties */
  T *ret;

  switch(xml_get_prop_state(xml_node)) {
  case OSM_FLAG_DELETED:
    printf("  Restoring DELETE flag\n");

    ret = osm->object_by_id<T>(id);
    if(likely(ret != nullptr))
      deleteDiffObject(osm, ret);
    else
      printf("  WARNING: no object with that id found\n");
    return nullptr;

  case OSM_FLAG_DIRTY:
    if(id < 0) {
      printf("  Restoring NEW object\n");

      ret = new T(base_attributes(id));

      osm->insert(ret);
      break;
    } else {
      printf("  Valid id/position (DIRTY)\n");

      ret = osm->object_by_id<T>(id);
      if(likely(ret != nullptr)) {
        osm->mark_dirty(ret);
        break;
      } else {
        printf("  WARNING: no object with that id found\n");
        return nullptr;
      }
    }

  default:
    // xml_get_prop_state() already warned about this
    return nullptr;
  }

  return ret;
}

void
diff_restore_node(xmlNodePtr node_node, osm_t::ref osm)
{
  node_t *node = restore_object<node_t>(node_node, osm);
  if (node == nullptr)
    return;

  pos_t pos = pos_t::fromXmlProperties(node_node);
  if(unlikely(!pos.valid())) {
    printf("  Node not deleted, but no valid position\n");
    return;
  }
  bool pos_diff = node->isNew() || node->pos != pos;
  if (pos_diff) {
    node->pos = pos;
    node->lpos = node->pos.toLpos(osm->bounds);
  }

  osm_t::TagMap ntags = xml_scan_tags(node_node->children);
  /* check if the same changes have been done upstream */
  if(node->flags & OSM_FLAG_DIRTY && !pos_diff && node->tags == ntags) {
    printf("node " ITEM_ID_FORMAT " has the same values and position as upstream, discarding diff\n", node->id);
    osm->unmark_dirty(node);
    return;
  }

  node->tags.replace(ntags);
}

void
diff_restore_way(xmlNodePtr node_way, osm_t::ref osm)
{
  way_t *way = restore_object<way_t>(node_way, osm);
  if (way == nullptr)
    return;

  /* handle hidden flag */
  if(xml_get_prop_bool(node_way, "hidden"))
    osm->waySetHidden(way);

  /* update node_chain */
  /* scan for nodes */
  node_chain_t new_chain;
  for(xmlNode *nd_node = node_way->children; nd_node != nullptr; nd_node = nd_node->next) {
    if(nd_node->type == XML_ELEMENT_NODE) {
      if(likely(strcmp(reinterpret_cast<const char *>(nd_node->name), "nd") == 0)) {
        /* attach node to node_chain */
        node_t *tmp = osm->parse_way_nd(nd_node);
        if(likely(tmp != nullptr))
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
    osm_node_chain_unref(new_chain);
    new_chain.clear();

    osm_t::TagMap ntags = xml_scan_tags(node_way->children);
    if (way->tags != ntags) {
      way->tags.replace(ntags);
    } else if (!ntags.empty()) {
      if (sameChain) {
        printf("way " ITEM_ID_FORMAT " has the same nodes and tags as upstream, discarding diff\n", way->id);
        osm->unmark_dirty(way);
      }
    }
  } else {
    /* only replace tags if nodes have been found before. if no nodes */
    /* were found this wasn't a dirty entry but e.g. only the hidden */
    /* flag had been set */
    printf("  no nodes restored, way isn't dirty!\n");
    osm->unmark_dirty(way);
  }
}

void
diff_restore_relation(xmlNodePtr node_rel, osm_t::ref osm)
{
  relation_t *relation = restore_object<relation_t>(node_rel, osm);
  if (relation == nullptr)
    return;

  bool was_changed = false;
  osm_t::TagMap ntags = xml_scan_tags(node_rel->children);
  if(relation->tags != ntags) {
    relation->tags.replace(ntags);
    was_changed = true;
  }

  /* update members */

  /* scan for members */
  std::vector<member_t> members;
  for(xmlNode *member_node = node_rel->children; member_node != nullptr;
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

  if(!was_changed && (relation->flags & OSM_FLAG_DIRTY)) {
    printf("relation " ITEM_ID_FORMAT " has the same members and tags as upstream, discarding diff\n", relation->id);
    osm->unmark_dirty(relation);
  }
}

} // namespace

unsigned int project_t::diff_restore()
{
  const std::string &diff_name = project_diff_name(this);
  if(diff_name.empty()) {
    printf("no diff present!\n");
    return DIFF_NONE_PRESENT;
  }

  fdguard difffd(dirfd, diff_name.c_str(), O_RDONLY);

  /* parse the file and get the DOM */
  xmlDocGuard doc(xmlReadFd(difffd, nullptr, nullptr, XML_PARSE_NONET));
  if(unlikely(!doc)) {
    error_dlg(trstring("Error: could not parse file %1\n").arg(diff_name));
    return DIFF_INVALID;
  }

  printf("diff %s found, applying ...\n", diff_name.c_str());

  unsigned int res = DIFF_RESTORED;

  for (xmlNode *cur_node = xmlDocGetRootElement(doc.get()); cur_node != nullptr;
       cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if(strcmp(reinterpret_cast<const char *>(cur_node->name), "diff") == 0) {
        xmlString str(xmlGetProp(cur_node, BAD_CAST "name"));
        if(!str.empty()) {
          const char *cstr = str;
          printf("diff for project %s\n", cstr);
          if(unlikely(name != cstr)) {
            warning_dlg(trstring("Diff name (%1) does not match project name (%2)").arg(cstr).arg(name));
            res |= DIFF_PROJECT_MISMATCH;
          }
        }

        for(xmlNodePtr node_node = cur_node->children; node_node != nullptr;
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
  if(osm->hasHiddenWays())
    res |= DIFF_HAS_HIDDEN;

  return res;
}

void diff_restore(project_t::ref project, MainUi *uicontrol) {
  assert(project->osm);
  unsigned int flags = project->diff_restore();
  if(flags & DIFF_HAS_HIDDEN) {
    printf("hidden flags have been restored, enable show_add menu\n");

    uicontrol->showNotification(_("Some objects are hidden"), MainUi::Highlight);
    uicontrol->setActionEnable(MainUi::MENU_ITEM_MAP_SHOW_ALL, true);
  }
}

bool diff_rename(project_t::ref oldproj, project_t *nproj)
{
  const std::string &diff_name = project_diff_name(oldproj.get());
  if(unlikely(diff_name.empty()))
    return false;

  fdguard difffd(oldproj->dirfd, diff_name.c_str(), O_RDONLY);

  /* parse the file and get the DOM */
  xmlDocGuard doc(xmlReadFd(difffd, nullptr, nullptr, XML_PARSE_NONET));
  if(unlikely(!doc)) {
    error_dlg(trstring("Error: could not parse file %1\n").arg(diff_name));
    return false;
  }

  for (xmlNode *cur_node = xmlDocGetRootElement(doc.get()); cur_node != nullptr;
       cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if(likely(strcmp(reinterpret_cast<const char *>(cur_node->name), "diff") == 0)) {
        xmlSetProp(cur_node, BAD_CAST "name", BAD_CAST nproj->name.c_str());
        break;
      }
    }
  }

  xmlSaveFormatFileEnc((nproj->path + diff_filename(nproj)).c_str(), doc.get(), "UTF-8", 1);

  return true;
}

bool project_t::diff_file_present() const
{
  const std::string &dn = project_diff_name(this);
  if(dn.empty())
    return false;

  struct stat st;
  return fstatat(dirfd, dn.c_str(), &st, 0) == 0 && S_ISREG(st.st_mode);
}

void project_t::diff_remove_file() const {
  unlinkat(dirfd, diff_filename(this).c_str(), 0);
}

xmlDocPtr osmchange_init()
{
  xmlDocPtr doc = xmlNewDoc(BAD_CAST "1.0");
  xmlNodePtr root_node = xmlNewNode(nullptr, BAD_CAST "osmChange");
  xmlNewProp(root_node, BAD_CAST "generator", BAD_CAST "OSM2go v" VERSION);
  xmlDocSetRootElement(doc, root_node);

  return doc;
}

namespace {

struct osmchange_delete_functor {
  xmlNode * const xml_node; ///< <delete> node
  const char * const changeset; ///< changeset string
  inline osmchange_delete_functor(xmlNodePtr delnode, const char *cs)
    : xml_node(delnode), changeset(cs) {}
  void operator()(const base_object_t *obj) {
    obj->osmchange_delete(xml_node, changeset);
  }
};

} // namespace

void osmchange_delete(const osm_t::dirty_t &dirty, xmlNodePtr xml_node, const char *changeset)
{
  xmlNodePtr del_node = xmlNewChild(xml_node, nullptr, BAD_CAST "delete", nullptr);
  osmchange_delete_functor fc(del_node, changeset);

  std::for_each(dirty.relations.deleted.begin(), dirty.relations.deleted.end(), fc);
  std::for_each(dirty.ways.deleted.begin(), dirty.ways.deleted.end(), fc);
  std::for_each(dirty.nodes.deleted.begin(), dirty.nodes.deleted.end(), fc);
}
