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

/*
 * diff.c - generate and restore changes on the current data set
 */

#include "diff.h"

#include "appdata.h"
#include "misc.h"
#include "statusbar.h"

#include <libxml/parser.h>
#include <libxml/tree.h>

#if !defined(LIBXML_TREE_ENABLED) || !defined(LIBXML_OUTPUT_ENABLED)
#error "libxml doesn't support required tree or output"
#endif

static gchar *diff_filename(const project_t *project) {
  return g_strconcat(project->path, project->name, ".diff", NULL);
}

static void diff_save_tags(const tag_t *tag, xmlNodePtr node) {
  while(tag) {
    xmlNodePtr tag_node = xmlNewChild(node, NULL,
				      BAD_CAST "tag", NULL);
    xmlNewProp(tag_node, BAD_CAST "k", BAD_CAST tag->key);
    xmlNewProp(tag_node, BAD_CAST "v", BAD_CAST tag->value);
    tag = tag->next;
  }
}

static void diff_save_state_n_id(int flags, xmlNodePtr node, item_id_t id) {
  if(flags & OSM_FLAG_DELETED)
    xmlNewProp(node, BAD_CAST "state", BAD_CAST "deleted");
  else if(flags & OSM_FLAG_NEW)
    xmlNewProp(node, BAD_CAST "state", BAD_CAST "new");

  /* all items need an id */
  gchar id_str[G_ASCII_DTOSTR_BUF_SIZE];
  g_snprintf(id_str, sizeof(id_str), ITEM_ID_FORMAT, id);
  xmlNewProp(node, BAD_CAST "id", BAD_CAST id_str);
}

static void diff_save_nodes(const node_t *node, xmlNodePtr root_node) {
  /* store all modfied nodes */
  while(node) {
    if(OSM_FLAGS(node)) {
      xmlNodePtr node_node = xmlNewChild(root_node, NULL,
					 BAD_CAST "node", NULL);

      diff_save_state_n_id(OSM_FLAGS(node), node_node, OSM_ID(node));

      if(!(OSM_FLAGS(node) & OSM_FLAG_DELETED)) {
	char str[32];

	/* additional info is only required if the node hasn't been deleted */
	g_ascii_formatd(str, sizeof(str), LL_FORMAT, node->pos.lat);
	xmlNewProp(node_node, BAD_CAST "lat", BAD_CAST str);
	g_ascii_formatd(str, sizeof(str), LL_FORMAT, node->pos.lon);
	xmlNewProp(node_node, BAD_CAST "lon", BAD_CAST str);
	snprintf(str, sizeof(str), "%lu", OSM_TIME(node));
	xmlNewProp(node_node, BAD_CAST "time", BAD_CAST str);

	diff_save_tags(OSM_TAG(node), node_node);
      }
    }
     node = node->next;
  }
}

static void diff_save_ways(const way_t *way, xmlNodePtr root_node) {

  /* store all modfied ways */
  while(way) {
    if(OSM_FLAGS(way)) {
      xmlNodePtr node_way = xmlNewChild(root_node, NULL,
					 BAD_CAST "way", NULL);

      diff_save_state_n_id(OSM_FLAGS(way), node_way, OSM_ID(way));

      if(OSM_FLAGS(way) & OSM_FLAG_HIDDEN)
	xmlNewProp(node_way, BAD_CAST "hidden", BAD_CAST "true");

      /* additional info is only required if the way hasn't been deleted */
      /* and of the dirty or new flags are set. (otherwise e.g. only */
      /* the hidden flag may be set) */
      if((!(OSM_FLAGS(way) & OSM_FLAG_DELETED)) &&
	 (OSM_FLAGS(way) & (OSM_FLAG_DIRTY | OSM_FLAG_NEW))) {
	node_chain_t *node_chain = way->node_chain;
	while(node_chain) {
	  xmlNodePtr node_node = xmlNewChild(node_way, NULL,
					     BAD_CAST "nd", NULL);
	  gchar id[G_ASCII_DTOSTR_BUF_SIZE];
	  g_snprintf(id, sizeof(id), ITEM_ID_FORMAT, OSM_ID(node_chain->node));
	  xmlNewProp(node_node, BAD_CAST "ref", BAD_CAST id);
	  node_chain = node_chain->next;
	}
	diff_save_tags(OSM_TAG(way), node_way);
      }
    }
    way = way->next;
  }
}

static void diff_save_relations(const relation_t *relation, xmlNodePtr root_node) {

  /* store all modfied relations */
  while(relation) {
    if(OSM_FLAGS(relation)) {
      xmlNodePtr node_rel = xmlNewChild(root_node, NULL,
					 BAD_CAST "relation", NULL);

      diff_save_state_n_id(OSM_FLAGS(relation), node_rel, OSM_ID(relation));

      if(!(OSM_FLAGS(relation) & OSM_FLAG_DELETED)) {
	/* additional info is only required if the relation */
	/* hasn't been deleted */
	member_t *member = relation->member;
	while(member) {
	  xmlNodePtr node_member = xmlNewChild(node_rel, NULL,
					     BAD_CAST "member", NULL);

	  gchar ref[G_ASCII_DTOSTR_BUF_SIZE];
	  switch(member->object.type) {
	  case NODE:
	    xmlNewProp(node_member, BAD_CAST "type", BAD_CAST "node");
	    g_snprintf(ref, sizeof(ref), ITEM_ID_FORMAT, OBJECT_ID(member->object));
	    break;
	  case WAY:
	    xmlNewProp(node_member, BAD_CAST "type", BAD_CAST "way");
	    g_snprintf(ref, sizeof(ref), ITEM_ID_FORMAT, OBJECT_ID(member->object));
	    break;
	  case RELATION:
	    xmlNewProp(node_member, BAD_CAST "type", BAD_CAST "relation");
	    g_snprintf(ref, sizeof(ref), ITEM_ID_FORMAT, OBJECT_ID(member->object));
	    break;

	    /* XXX_ID's are used if this is a reference to an item not */
	    /* stored in this xml data set */
	  case NODE_ID:
	    xmlNewProp(node_member, BAD_CAST "type", BAD_CAST "node");
	    g_snprintf(ref, sizeof(ref), ITEM_ID_FORMAT, member->object.id);
	    break;
	  case WAY_ID:
	    xmlNewProp(node_member, BAD_CAST "type", BAD_CAST "way");
	    g_snprintf(ref, sizeof(ref), ITEM_ID_FORMAT, member->object.id);
	    break;
	  case RELATION_ID:
	    xmlNewProp(node_member, BAD_CAST "type", BAD_CAST "relation");
	    g_snprintf(ref, sizeof(ref), ITEM_ID_FORMAT, member->object.id);
	    break;

	  default:
	    printf("unexpected member type %d\n", member->object.type);
	    g_assert_not_reached();
	    break;
	  }

	  xmlNewProp(node_member, BAD_CAST "ref", BAD_CAST ref);

	  if(member->role)
	    xmlNewProp(node_member, BAD_CAST "role", BAD_CAST member->role);

	  member = member->next;
	}
	diff_save_tags(OSM_TAG(relation), node_rel);
      }
    }
    relation = relation->next;
  }
}


/* return true if no diff needs to be saved */
gboolean diff_is_clean(const osm_t *osm, gboolean honor_hidden_flags) {
  gboolean clean = TRUE;

  /* check if a diff is necessary */
  node_t *node = osm->node;
  while(node && clean) {
    if(OSM_FLAGS(node)) clean = FALSE;
    node = node->next;
  }

  if(clean) {
    int flagmask = honor_hidden_flags ? ~0 : ~OSM_FLAG_HIDDEN;
    way_t *way;
    for(way = osm->way; way && clean; way = way->next) {
      if(OSM_FLAGS(way) & flagmask)
        clean = FALSE;
    }
  }

  relation_t *relation = osm->relation;
  while(relation && clean) {
    if(OSM_FLAGS(relation)) clean = FALSE;
    relation = relation->next;
  }

  return clean;
}

void diff_save(const project_t *project, const osm_t *osm) {
  if(!project || !osm) return;

  gchar *diff_name = diff_filename(project);

  if(diff_is_clean(osm, TRUE)) {
    printf("data set is clean, removing diff if present\n");
    g_remove(diff_name);
    g_free(diff_name);
    return;
  }

  printf("data set is dirty, generating diff\n");

  /* write the diff to a new file so the original one needs intact until
   * saving is completed */
  char *ndiff = g_strconcat(project->path, "save.diff", NULL);

  xmlDocPtr doc = xmlNewDoc(BAD_CAST "1.0");
  xmlNodePtr root_node = xmlNewNode(NULL, BAD_CAST "diff");
  xmlNewProp(root_node, BAD_CAST "name", BAD_CAST project->name);
  xmlDocSetRootElement(doc, root_node);

  diff_save_nodes(osm->node, root_node);
  diff_save_ways(osm->way, root_node);
  diff_save_relations(osm->relation, root_node);

  xmlSaveFormatFileEnc(ndiff, doc, "UTF-8", 1);
  xmlFreeDoc(doc);

  /* if we reach this point writing the new file worked and we */
  /* can delete the backup */
  g_rename(ndiff, diff_name);

  g_free(diff_name);
  g_free(ndiff);
}

static item_id_t xml_get_prop_int(xmlNode *node, const char *prop, item_id_t def) {
  xmlChar *str = xmlGetProp(node, BAD_CAST prop);
  item_id_t value = def;

  if(str) {
    value = strtoll((char*)str, NULL, 10);
    xmlFree(str);
  }

  return value;
}

static int xml_get_prop_state(xmlNode *node) {
  xmlChar *str = xmlGetProp(node, BAD_CAST "state");

  if(str) {
    if(strcmp((char*)str, "new") == 0) {
      xmlFree(str);
      return OSM_FLAG_NEW;
    } else if(strcmp((char*)str, "deleted") == 0) {
      xmlFree(str);
      return OSM_FLAG_DELETED;
    } else {
      xmlFree(str);
    }

    g_assert_not_reached();
  }

  return OSM_FLAG_DIRTY;
}

static gboolean xml_get_prop_pos(xmlNode *node, pos_t *pos) {
  xmlChar *str_lat = xmlGetProp(node, BAD_CAST "lat");
  xmlChar *str_lon = xmlGetProp(node, BAD_CAST "lon");

  if(!str_lon || !str_lat) {
    if(!str_lon) xmlFree(str_lon);
    if(!str_lat) xmlFree(str_lat);
    return FALSE;
  }

  pos->lat = g_ascii_strtod((char*)str_lat, NULL);
  pos->lon = g_ascii_strtod((char*)str_lon, NULL);

  xmlFree(str_lon);
  xmlFree(str_lat);

  return TRUE;
}

static tag_t *xml_scan_tags(xmlNodePtr node) {
  /* scan for tags */
  tag_t *first_tag = NULL;
  tag_t **tag = &first_tag;

  while(node) {
    if(node->type == XML_ELEMENT_NODE) {
      if(strcmp((char*)node->name, "tag") == 0) {
	/* attach tag to node/way */
	*tag = osm_parse_osm_tag(node);
	if(*tag) tag = &((*tag)->next);
      }
    }
    node = node->next;
  }
  return first_tag;
}

/*
 * @brief check if all local modifications of a node are already in the upstream node
 * @param node upstream node
 * @param pos new position
 * @param ntags new tags
 * @return if changes are redundant
 * @retval TRUE the changes are the same as the upstream node
 * @retval FALSE local changes are real
 */
static gboolean
node_compare_changes(const node_t *node, const pos_t *pos, const tag_t *ntags)
{
  if (node->pos.lat != pos->lat || node->pos.lon != pos->lon)
    return FALSE;

  return !osm_tag_lists_diff(OSM_TAG(node), ntags);
}

static void diff_restore_node(xmlNodePtr node_node, osm_t *osm) {
  printf("Restoring node");

  /* read properties */
  item_id_t id = xml_get_prop_int(node_node, "id", ID_ILLEGAL);
  if(id == ID_ILLEGAL) {
    printf("\n  Node entry missing id\n");
    return;
  }

  printf(" " ITEM_ID_FORMAT "\n", id);

  int state = xml_get_prop_state(node_node);
  pos_t pos;
  gboolean pos_diff = xml_get_prop_pos(node_node, &pos);

  if(!(state & OSM_FLAG_DELETED) && !pos_diff) {
    printf("  Node not deleted, but no valid position\n");
    return;
  }

  /* evaluate properties */
  node_t *node = NULL;

  switch(state) {
  case OSM_FLAG_NEW:
    printf("  Restoring NEW node\n");

    node = g_new0(node_t, 1);
    OSM_ID(node) = id;
    OSM_VISIBLE(node) = TRUE;
    OSM_FLAGS(node) = OSM_FLAG_NEW;
    OSM_TIME(node) = xml_get_prop_int(node_node, "time", 0);
    if(!OSM_TIME(node)) OSM_TIME(node) = time(NULL);

    /* attach to end of node list */
    node_t **lnode = &osm->node;
    while(*lnode) lnode = &(*lnode)->next;
    *lnode = node;
    break;

  case OSM_FLAG_DELETED:
    printf("  Restoring DELETE flag\n");

    if((node = osm_get_node_by_id(osm, id)))
      OSM_FLAGS(node) |= OSM_FLAG_DELETED;
    else
      printf("  WARNING: no node with that id found\n");
    break;

  case OSM_FLAG_DIRTY:
    printf("  Valid id/position (DIRTY)\n");

    if((node = osm_get_node_by_id(osm, id)))
      OSM_FLAGS(node) |= OSM_FLAG_DIRTY;
    else
      printf("  WARNING: no node with that id found\n");
    break;

  default:
    printf("  Illegal node entry\n");
    return;
    break;
  }

  if(!node) {
    printf("  no valid node\n");
    return;
  }

  tag_t *ntags = xml_scan_tags(node_node->children);
  /* check if the same changes have been done upstream */
  if(state == OSM_FLAG_DIRTY && node_compare_changes(node, &pos, ntags)) {
    printf("node " ITEM_ID_FORMAT " has the same values and position as upstream, discarding diff\n", id);
    osm_tags_free(ntags);
    OSM_FLAGS(node) &= ~OSM_FLAG_DIRTY;
    return;
  }

  /* node may be an existing node, so remove tags to */
  /* make space for new ones */
  if(OSM_TAG(node)) {
    printf("  removing existing tags for diff tags\n");
    osm_tags_free(OSM_TAG(node));
  }

  OSM_TAG(node) = ntags;

  /* update position from diff */
  if(pos_diff) {
    node->pos = pos;

    pos2lpos(osm->bounds, &node->pos, &node->lpos);
  }
}

static void diff_restore_way(xmlNodePtr node_node, osm_t *osm) {
  printf("Restoring way");

  item_id_t id = xml_get_prop_int(node_node, "id", ID_ILLEGAL);
  if(id == ID_ILLEGAL) {
    printf("\n  entry missing id\n");
    return;
  }

  printf(" " ITEM_ID_FORMAT "\n", id);

  int state = xml_get_prop_state(node_node);

  /* handle hidden flag */
  gboolean hidden = FALSE;
  xmlChar *str = xmlGetProp(node_node, BAD_CAST "hidden");
  if(str) {
    if(strcasecmp((char*)str, "true") == 0)
      hidden = TRUE;

    xmlFree(str);
  }


  /* evaluate properties */
  way_t *way = NULL;
  switch(state) {
  case OSM_FLAG_NEW:
    printf("  Restoring NEW way\n");

    way = g_new0(way_t, 1);
    OSM_ID(way) = id;
    OSM_VISIBLE(way) = TRUE;
    OSM_FLAGS(way) = OSM_FLAG_NEW;
    OSM_TIME(way) = xml_get_prop_int(node_node, "time", 0);
    if(!OSM_TIME(way)) OSM_TIME(way) = time(NULL);

    /* attach to end of way list */
    way_t **lway = &osm->way;
    while(*lway) lway = &(*lway)->next;
    *lway = way;
    break;

  case OSM_FLAG_DELETED:
    printf("  Restoring DELETE flag\n");

    if((way = osm_get_way_by_id(osm, id)))
      OSM_FLAGS(way) |= OSM_FLAG_DELETED;
    else
      printf("  WARNING: no way with that id found\n");
    break;

  case OSM_FLAG_DIRTY:
    printf("  Valid id (DIRTY)\n");

    if((way = osm_get_way_by_id(osm, id)))
      OSM_FLAGS(way) |= OSM_FLAG_DIRTY;
    else
      printf("  WARNING: no way with that id found\n");
    break;

  default:
    printf("  Illegal way entry\n");
    return;
  }

  if(!way) {
    printf("  no valid way\n");
    return;
  }

  /* update node_chain */
  if(hidden)
    OSM_FLAGS(way) |= OSM_FLAG_HIDDEN;

  /* scan for nodes */
  node_chain_t *new_chain = NULL, **node_chain = &new_chain;
  xmlNode *nd_node = NULL;
  for(nd_node = node_node->children; nd_node; nd_node = nd_node->next) {
    if(nd_node->type == XML_ELEMENT_NODE) {
      if(strcmp((char*)nd_node->name, "nd") == 0) {
	/* attach node to node_chain */
	*node_chain = osm_parse_osm_way_nd(osm, nd_node);
	if(*node_chain)
	  node_chain = &((*node_chain)->next);
      }
    }
  }

  /* only replace the original nodes if new nodes have actually been */
  /* found. */
  if(new_chain != NULL) {
    gboolean was_modified = osm_node_chain_diff(way->node_chain, new_chain);

    if(was_modified == FALSE) {
      osm_node_chain_free(new_chain);
      new_chain = NULL; /* indicate that waypoints did not change */
    } else {
      /* way may be an existing way, so remove nodes to */
      /* make space for new ones */
      if(way->node_chain) {
        printf("  removing existing nodes for diff nodes\n");
        osm_node_chain_free(way->node_chain);
        way->node_chain = NULL;
      }
      way->node_chain = new_chain;
    }

    /* only replace tags if nodes have been found before. if no nodes */
    /* were found this wasn't a dirty entry but e.g. only the hidden */
    /* flag had been set */

    tag_t *ntags = xml_scan_tags(node_node->children);
    if (osm_tag_lists_diff(OSM_TAG(way), ntags)) {
      /* way may be an existing way, so remove tags to */
      /* make space for new ones */
      if(OSM_TAG(way)) {
        printf("  removing existing tags for diff tags\n");
        osm_tags_free(OSM_TAG(way));
      }

      OSM_TAG(way) = ntags;
    } else if (new_chain != NULL) {
      osm_tags_free(ntags);
    } else {
      printf("way " ITEM_ID_FORMAT " has the same nodes and tags as upstream, discarding diff\n", id);
      osm_tags_free(ntags);
      OSM_FLAGS(way) &= ~OSM_FLAG_DIRTY;
      return;
    }
  } else {
    printf("  no nodes restored, way isn't dirty!\n");
    OSM_FLAGS(way) &= ~OSM_FLAG_DIRTY;
  }
}

static void diff_restore_relation(xmlNodePtr node_rel, osm_t *osm) {
  printf("Restoring relation");

  item_id_t id = xml_get_prop_int(node_rel, "id", ID_ILLEGAL);
  if(id == ID_ILLEGAL) {
    printf("\n  entry missing id\n");
    return;
  }

  printf(" " ITEM_ID_FORMAT "\n", id);

  int state = xml_get_prop_state(node_rel);

  /* evaluate properties */
  relation_t *relation = NULL;
  switch(state) {
  case OSM_FLAG_NEW:
    printf("  Restoring NEW relation\n");

    relation = g_new0(relation_t, 1);
    OSM_ID(relation) = id;
    OSM_VISIBLE(relation) = TRUE;
    OSM_FLAGS(relation) = OSM_FLAG_NEW;
    OSM_TIME(relation) = xml_get_prop_int(node_rel, "time", 0);
    if(!OSM_TIME(relation)) OSM_TIME(relation) = time(NULL);

    /* attach to end of relation list */
    relation_t **lrelation = &osm->relation;
    while(*lrelation) lrelation = &(*lrelation)->next;
    *lrelation = relation;
    break;

  case OSM_FLAG_DELETED:
    printf("  Restoring DELETE flag\n");

    if((relation = osm_get_relation_by_id(osm, id)))
      OSM_FLAGS(relation) |= OSM_FLAG_DELETED;
    else
      printf("  WARNING: no relation with that id found\n");
    break;

  case OSM_FLAG_DIRTY:
    printf("  Valid id (DIRTY)\n");

    if((relation = osm_get_relation_by_id(osm, id)))
      OSM_FLAGS(relation) |= OSM_FLAG_DIRTY;
    else
      printf("  WARNING: no relation with that id found\n");
    break;

  default:
    printf("  Illegal relation entry\n");
    return;
  }

  if(!relation) {
    printf("  no valid relation\n");
    return;
  }

  gboolean was_changed = FALSE;
  tag_t *ntags = xml_scan_tags(node_rel->children);
  if (osm_tag_lists_diff(OSM_TAG(relation), ntags)) {
    /* relation may be an existing relation, so remove tags to */
    /* make space for new ones */
    osm_tags_free(OSM_TAG(relation));

    OSM_TAG(relation) = ntags;
    was_changed = TRUE;
  } else {
    osm_tags_free(ntags);
  }

  /* update members */

  /* scan for members */
  member_t *nmember = NULL, **member = &nmember;
  xmlNode *member_node = NULL;
  for(member_node = node_rel->children; member_node;
      member_node = member_node->next) {
    if(member_node->type == XML_ELEMENT_NODE) {
      if(strcmp((char*)member_node->name, "member") == 0) {
	/* attach member to member_chain */
	*member = osm_parse_osm_relation_member(osm, member_node);
	if(*member)
	  member = &((*member)->next);
      }
    }
  }

  if(osm_members_diff(relation->member, nmember)) {
    /* this may be an existing relation, so remove members to */
    /* make space for new ones */
    osm_members_free(relation->member);
    was_changed = TRUE;
    relation->member = nmember;
  } else {
    osm_members_free(nmember);
  }

  if(!was_changed && (OSM_FLAGS(relation) & OSM_FLAG_DIRTY)) {
    printf("relation " ITEM_ID_FORMAT " has the same members and tags as upstream, discarding diff\n", id);
    OSM_FLAGS(relation) &= ~OSM_FLAG_DIRTY;
  }
}

void diff_restore(appdata_t *appdata, project_t *project, osm_t *osm) {
  if(!project || !osm) return;

  /* first try to open a backup which is only present if saving the */
  /* actual diff didn't succeed */
  char *diff_name = g_strconcat(project->path, "backup.diff", NULL);
  if(g_file_test(diff_name, G_FILE_TEST_EXISTS)) {
    printf("diff backup present, loading it instead of real diff ...\n");
  } else {
    g_free(diff_name);
    diff_name = diff_filename(project);

    if(!g_file_test(diff_name, G_FILE_TEST_EXISTS)) {
      printf("no diff present!\n");
      g_free(diff_name);
      return;
    }
    printf("diff found, applying ...\n");
  }

  xmlDoc *doc = NULL;
  xmlNode *root_element = NULL;

  /* parse the file and get the DOM */
  if((doc = xmlReadFile(diff_name, NULL, 0)) == NULL) {
    errorf(GTK_WIDGET(appdata->window),
	   "Error: could not parse file %s\n", diff_name);
    g_free(diff_name);
    return;
  }

  g_free(diff_name);

  /* Get the root element node */
  root_element = xmlDocGetRootElement(doc);

  xmlNode *cur_node = NULL;
  for (cur_node = root_element; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if(strcmp((char*)cur_node->name, "diff") == 0) {
	xmlChar *str = xmlGetProp(cur_node, BAD_CAST "name");
	if(str) {
	  const char *cstr = (const char*)str;
	  printf("diff for project %s\n", cstr);
	  if(strcmp(project->name, cstr) != 0) {
	    messagef(GTK_WIDGET(appdata->window), _("Warning"),
		     "Diff name (%s) does not match project name (%s)",
		     cstr, project->name);
	  }
	  xmlFree(str);
	}

	xmlNodePtr node_node = cur_node->children;
	while(node_node) {
	  if(node_node->type == XML_ELEMENT_NODE) {

	    if(strcmp((char*)node_node->name, "node") == 0)
	      diff_restore_node(node_node, osm);

	    else if(strcmp((char*)node_node->name, "way") == 0)
	      diff_restore_way(node_node, osm);

	    else if(strcmp((char*)node_node->name, "relation") == 0)
	      diff_restore_relation(node_node, osm);

	    else
	      printf("WARNING: item %s not restored\n", node_node->name);
	  }
	  node_node = node_node->next;
	}
      }
    }
  }

  xmlFreeDoc(doc);

  /* check for hidden ways and update menu accordingly */
  gboolean something_is_hidden = FALSE;
  const way_t *way = osm->way;
  while(!something_is_hidden && way) {
    if(OSM_FLAGS(way) & OSM_FLAG_HIDDEN)
      something_is_hidden = TRUE;

    way = way->next;
  }

  if(something_is_hidden) {
    printf("hidden flags have been restored, enable show_add menu\n");

    statusbar_set(appdata, _("Some objects are hidden"), TRUE);
    gtk_widget_set_sensitive(appdata->menu_item_map_show_all, TRUE);
  }
}

gboolean diff_present(const project_t *project) {
  gchar *diff_name = diff_filename(project);

  gboolean ret = g_file_test(diff_name, G_FILE_TEST_EXISTS);

  g_free(diff_name);
  return ret;
}

void diff_remove(project_t *project) {
  gchar *diff_name = diff_filename(project);
  g_remove(diff_name);
  g_free(diff_name);
}
