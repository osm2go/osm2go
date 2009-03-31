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

#include "appdata.h"

#include <libxml/parser.h>
#include <libxml/tree.h>

#if !defined(LIBXML_TREE_ENABLED) || !defined(LIBXML_OUTPUT_ENABLED)
#error "libxml doesn't support required tree or output"
#endif

static void diff_save_tags(tag_t *tag, xmlNodePtr node) {
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
  char *id_str = g_strdup_printf("%ld", id);
  xmlNewProp(node, BAD_CAST "id", BAD_CAST id_str);
  g_free(id_str);
}

static void diff_save_nodes(node_t *node, xmlNodePtr root_node) {
  /* store all modfied nodes */
  while(node) {
    if(node->flags) { 
      xmlNodePtr node_node = xmlNewChild(root_node, NULL, 
					 BAD_CAST "node", NULL);

      diff_save_state_n_id(node->flags, node_node, node->id);

      if(!(node->flags & OSM_FLAG_DELETED)) {
	char str[32];

	/* additional info is only required if the node hasn't been deleted */
	g_ascii_dtostr(str, sizeof(str), node->pos.lat);
	xmlNewProp(node_node, BAD_CAST "lat", BAD_CAST str);
	g_ascii_dtostr(str, sizeof(str), node->pos.lon);
	xmlNewProp(node_node, BAD_CAST "lon", BAD_CAST str);
	snprintf(str, sizeof(str), "%ld", node->time);
	xmlNewProp(node_node, BAD_CAST "time", BAD_CAST str);

	diff_save_tags(node->tag, node_node);
      }
    }
     node = node->next;
  }
}

static void diff_save_ways(way_t *way, xmlNodePtr root_node) {

  /* store all modfied ways */
  while(way) {
    if(way->flags) {
      xmlNodePtr node_way = xmlNewChild(root_node, NULL, 
					 BAD_CAST "way", NULL);

      diff_save_state_n_id(way->flags, node_way, way->id);
      
      if(way->flags & OSM_FLAG_HIDDEN) 
	xmlNewProp(node_way, BAD_CAST "hidden", BAD_CAST "true");

      /* additional info is only required if the way hasn't been deleted */
      /* and of the dirty or new flags are set. (otherwise e.g. only */
      /* the hidden flag may be set) */
      if((!(way->flags & OSM_FLAG_DELETED)) && 
	 (way->flags & (OSM_FLAG_DIRTY | OSM_FLAG_NEW))) {
	node_chain_t *node_chain = way->node_chain;
	while(node_chain) {
	  xmlNodePtr node_node = xmlNewChild(node_way, NULL, 
					     BAD_CAST "nd", NULL);
	  char *id = g_strdup_printf("%ld", node_chain->node->id);
	  xmlNewProp(node_node, BAD_CAST "ref", BAD_CAST id);
	  g_free(id);
	  node_chain = node_chain->next;
	}
	diff_save_tags(way->tag, node_way);
      }
    }
    way = way->next;
  }
}

static void diff_save_relations(relation_t *relation, xmlNodePtr root_node) {

  /* store all modfied relations */
  while(relation) {
    if(relation->flags) { 
      xmlNodePtr node_rel = xmlNewChild(root_node, NULL, 
					 BAD_CAST "relation", NULL);

      diff_save_state_n_id(relation->flags, node_rel, relation->id);
      
      if(!(relation->flags & OSM_FLAG_DELETED)) {
	/* additional info is only required if the relation */
	/* hasn't been deleted */
	member_t *member = relation->member;
	while(member) {
	  xmlNodePtr node_member = xmlNewChild(node_rel, NULL, 
					     BAD_CAST "member", NULL);

	  char *ref = NULL;
	  switch(member->object.type) {
	  case NODE:
	    xmlNewProp(node_member, BAD_CAST "type", BAD_CAST "node");
	    ref = g_strdup_printf("%ld", member->object.node->id);
	    break;
	  case WAY:
	    xmlNewProp(node_member, BAD_CAST "type", BAD_CAST "way");
	    ref = g_strdup_printf("%ld", member->object.way->id);
	    break;
	  case RELATION:
	    xmlNewProp(node_member, BAD_CAST "type", BAD_CAST "relation");
	    ref = g_strdup_printf("%ld", member->object.relation->id);
	    break;

	    /* XXX_ID's are used if this is a reference to an item not */
	    /* stored in this xml data set */
	  case NODE_ID:
	    xmlNewProp(node_member, BAD_CAST "type", BAD_CAST "node");
	    ref = g_strdup_printf("%ld", member->object.id);
	    break;
	  case WAY_ID:
	    xmlNewProp(node_member, BAD_CAST "type", BAD_CAST "way");
	    ref = g_strdup_printf("%ld", member->object.id);
	    break;
	  case RELATION_ID:
	    xmlNewProp(node_member, BAD_CAST "type", BAD_CAST "relation");
	    ref = g_strdup_printf("%ld", member->object.id);
	    break;

	  default:
	    printf("unexpected member type %d\n", member->object.type);
	    break;
	  }

	  g_assert(ref);
	  xmlNewProp(node_member, BAD_CAST "ref", BAD_CAST ref);
	  g_free(ref);

	  if(member->role) 
	    xmlNewProp(node_member, BAD_CAST "role", BAD_CAST member->role);

	  member = member->next;
	}
	diff_save_tags(relation->tag, node_rel);
      }
    }
    relation = relation->next;
  }
}


/* return true if no diff needs to be saved */
gboolean diff_is_clean(osm_t *osm, gboolean honor_hidden_flags) {
  gboolean clean = TRUE;

  /* check if a diff is necessary */
  node_t *node = osm->node;
  while(node && clean) {
    if(node->flags) clean = FALSE;
    node = node->next;
  }

  way_t *way = osm->way;
  while(way && clean) {
    if(honor_hidden_flags) {
      if(way->flags) clean = FALSE;
    } else
      if(way->flags & ~OSM_FLAG_HIDDEN) 
	clean = FALSE;

    way = way->next;
  }

  relation_t *relation = osm->relation;
  while(relation && clean) {
    if(relation->flags) clean = FALSE;
    relation = relation->next;
  }

  return clean;
}

void diff_save(project_t *project, osm_t *osm) {
  if(!project || !osm) return;

  char *diff_name = 
  g_strdup_printf("%s/%s.diff", project->path, project->name);

  if(diff_is_clean(osm, TRUE)) {
    printf("data set is clean, removing diff if present\n");
    g_remove(diff_name);
    g_free(diff_name);
    return;
  }

  printf("data set is dirty, generating diff\n");

  LIBXML_TEST_VERSION;

  xmlDocPtr doc = xmlNewDoc(BAD_CAST "1.0");
  xmlNodePtr root_node = xmlNewNode(NULL, BAD_CAST "diff");
  xmlNewProp(root_node, BAD_CAST "name", BAD_CAST project->name);
  xmlDocSetRootElement(doc, root_node);

  diff_save_nodes(osm->node, root_node);
  diff_save_ways(osm->way, root_node);
  diff_save_relations(osm->relation, root_node);

  xmlSaveFormatFileEnc(diff_name, doc, "UTF-8", 1);
  xmlFreeDoc(doc);
  xmlCleanupParser();

  g_free(diff_name);
}

static int xml_get_prop_int(xmlNode *node, char *prop, int def) {
  char *str = (char*)xmlGetProp(node, BAD_CAST prop);
  int value = def;

  if(str) {
    value = strtoul(str, NULL, 10);
    xmlFree(str);
  }

  return value;
}

static int xml_get_prop_state(xmlNode *node, char *prop) {
  char *str = (char*)xmlGetProp(node, BAD_CAST prop);

  if(str) {
    if(strcasecmp(str, "new") == 0) {
      xmlFree(str);
      return OSM_FLAG_NEW;
    }

    if(strcasecmp(str, "deleted") == 0) {
      xmlFree(str);
      return OSM_FLAG_DELETED;
    }

    g_assert(0);
  }

  return OSM_FLAG_DIRTY;
}

static pos_t *xml_get_prop_pos(xmlNode *node) {
  char *str_lat = (char*)xmlGetProp(node, BAD_CAST "lat");
  char *str_lon = (char*)xmlGetProp(node, BAD_CAST "lon");

  if(!str_lon || !str_lat) {
    if(!str_lon) xmlFree(str_lon);
    if(!str_lat) xmlFree(str_lat);
    return NULL;
  }

  pos_t *pos = g_new0(pos_t, 1);
  pos->lat = g_ascii_strtod(str_lat, NULL);
  pos->lon = g_ascii_strtod(str_lon, NULL);

  xmlFree(str_lon);
  xmlFree(str_lat);

  return pos;
}

static tag_t *xml_scan_tags(xmlDoc *doc, xmlNodePtr node, osm_t *osm) {
  /* scan for tags */
  tag_t *first_tag = NULL;
  tag_t **tag = &first_tag;

  while(node) {
    if(node->type == XML_ELEMENT_NODE) {
      if(strcasecmp((char*)node->name, "tag") == 0) {
	/* attach tag to node/way */
	*tag = osm_parse_osm_tag(osm, doc, node);
	if(*tag) tag = &((*tag)->next);
      }
    }
    node = node->next;
  }
  return first_tag;
}

void diff_restore_node(xmlDoc *doc, xmlNodePtr node_node, osm_t *osm) {
  printf("Restoring node\n");

  /* read properties */
  item_id_t id = xml_get_prop_int(node_node, "id", ID_ILLEGAL);
  if(id == ID_ILLEGAL) {
    printf("  Node entry missing id\n");
    return;
  }

  int state = xml_get_prop_state(node_node, "state");
  pos_t *pos = xml_get_prop_pos(node_node);

  if(!(state & OSM_FLAG_DELETED) && !pos) {
    printf("  Node not deleted, but no valid position\n");
    return;
  }
    
  /* evaluate properties */
  node_t *node = NULL;

  switch(state) {
  case OSM_FLAG_NEW:
    printf("  Restoring NEW node\n");
    
    node = g_new0(node_t, 1);
    node->visible = TRUE;
    node->flags = OSM_FLAG_NEW;
    node->time = xml_get_prop_int(node_node, "time", 0);
    if(!node->time) node->time = time(NULL);

    /* attach to end of node list */
    node_t **lnode = &osm->node;
    while(*lnode) lnode = &(*lnode)->next;  
    *lnode = node;
    break;

  case OSM_FLAG_DELETED:
    printf("  Restoring DELETE flag\n");
    
    if((node = osm_get_node_by_id(osm, id))) 
      node->flags |= OSM_FLAG_DELETED;      
    else
      printf("  WARNING: no node with that id found\n");
    break;

  case OSM_FLAG_DIRTY:
    printf("  Valid id/position (DIRTY)\n");
    
    if((node = osm_get_node_by_id(osm, id))) 
      node->flags |= OSM_FLAG_DIRTY;
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

  /* update id and position from diff */
  node->id = id;
  if(pos) {
    node->pos.lat = pos->lat;
    node->pos.lon = pos->lon;
    
    pos2lpos(osm->bounds, &node->pos, &node->lpos);

    g_free(pos);
  }
  
  /* node may be an existing node, so remove tags to */
  /* make space for new ones */
  if(node->tag) {
    printf("  removing existing tags for diff tags\n");
    osm_tag_free(node->tag);
    node->tag = NULL;
  }
  
  node->tag = xml_scan_tags(doc, node_node->children, osm);
}

void diff_restore_way(xmlDoc *doc, xmlNodePtr node_node, osm_t *osm) {
  printf("Restoring way\n");
	      
  item_id_t id = xml_get_prop_int(node_node, "id", ID_ILLEGAL);
  if(id == ID_ILLEGAL) {
    printf("  entry missing id\n");
    return;
  }

  int state = xml_get_prop_state(node_node, "state");

  /* handle hidden flag */
  gboolean hidden = FALSE;
  char *str = (char*)xmlGetProp(node_node, BAD_CAST "hidden");
  if(str) { 
    if(strcasecmp(str, "true") == 0) 
      hidden = TRUE;

    xmlFree(str);
  }


  /* evaluate properties */
  way_t *way = NULL;
  switch(state) {
  case OSM_FLAG_NEW:
    printf("  Restoring NEW way\n");
    
    way = g_new0(way_t, 1);
    way->visible = TRUE;
    way->flags = OSM_FLAG_NEW;
    way->time = xml_get_prop_int(node_node, "time", 0);
    if(!way->time) way->time = time(NULL);

    /* attach to end of way list */
    way_t **lway = &osm->way;
    while(*lway) lway = &(*lway)->next;  
    *lway = way;
    break;

  case OSM_FLAG_DELETED:
    printf("  Restoring DELETE flag\n");
    
    if((way = osm_get_way_by_id(osm, id)))
      way->flags |= OSM_FLAG_DELETED;
    else
      printf("  WARNING: no way with that id found\n");
    break;

  case OSM_FLAG_DIRTY:
    printf("  Valid id (DIRTY)\n");

    if((way = osm_get_way_by_id(osm, id))) 
      way->flags |= OSM_FLAG_DIRTY;
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
  
  /* update id from diff */
  way->id = id;
    
  /* update node_chain */
  if(hidden)
    way->flags |= OSM_FLAG_HIDDEN;
    
  gboolean installed_new_nodes = FALSE;

  /* scan for nodes */
  node_chain_t **node_chain = &way->node_chain;
  xmlNode *nd_node = NULL;
  for(nd_node = node_node->children; nd_node; nd_node = nd_node->next) {
    if(nd_node->type == XML_ELEMENT_NODE) {
      if(strcasecmp((char*)nd_node->name, "nd") == 0) {

	/* only replace the original nodes if new nodes have actually been */
	/* found. */
	if(!installed_new_nodes) {
	  /* way may be an existing way, so remove nodes to */
	  /* make space for new ones */
	  if(way->node_chain) {
	    printf("  removing existing nodes for diff nodes\n");
	    osm_node_chain_free(way->node_chain);
	    way->node_chain = NULL;
	  }

	  installed_new_nodes = TRUE;
	}

	/* attach node to node_chain */
	*node_chain = osm_parse_osm_way_nd(osm, doc, nd_node);
	if(*node_chain) 
	  node_chain = &((*node_chain)->next);
      }
    }
  }

  /* only replace tags if nodes have been found before. if no nodes */
  /* were found this wasn't a dirty entry but e.g. only the hidden */
  /* flag had been set */
  if(installed_new_nodes) {
  
    /* node may be an existing node, so remove tags to */
    /* make space for new ones */
    if(way->tag) {
      printf("  removing existing tags for diff tags\n");
      osm_tag_free(way->tag);
      way->tag = NULL;
    }
    
    way->tag = xml_scan_tags(doc, node_node->children, osm);
  } else {
    printf("  no nodes restored, way isn't dirty!\n");
    way->flags &= ~OSM_FLAG_DIRTY;
  }
}

void diff_restore_relation(xmlDoc *doc, xmlNodePtr node_rel, osm_t *osm) {
  printf("Restoring relation\n");
	      
  item_id_t id = xml_get_prop_int(node_rel, "id", ID_ILLEGAL);
  if(id == ID_ILLEGAL) {
    printf("  entry missing id\n");
    return;
  }

  int state = xml_get_prop_state(node_rel, "state");

  /* evaluate properties */
  relation_t *relation = NULL;
  switch(state) {
  case OSM_FLAG_NEW:
    printf("  Restoring NEW relation\n");
    
    relation = g_new0(relation_t, 1);
    relation->visible = TRUE;
    relation->flags = OSM_FLAG_NEW;
    relation->time = xml_get_prop_int(node_rel, "time", 0);
    if(!relation->time) relation->time = time(NULL);

    /* attach to end of relation list */
    relation_t **lrelation = &osm->relation;
    while(*lrelation) lrelation = &(*lrelation)->next;  
    *lrelation = relation;
    break;

  case OSM_FLAG_DELETED:
    printf("  Restoring DELETE flag\n");
    
    if((relation = osm_get_relation_by_id(osm, id)))
      relation->flags |= OSM_FLAG_DELETED;
    else
      printf("  WARNING: no relation with that id found\n");
    break;

  case OSM_FLAG_DIRTY:
    printf("  Valid id (DIRTY)\n");
    
    if((relation = osm_get_relation_by_id(osm, id))) 
      relation->flags |= OSM_FLAG_DIRTY;
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
  
  /* update id from diff */
  relation->id = id;
    
  /* update members */
    
  /* this may be an existing relation, so remove members to */
  /* make space for new ones */
  if(relation->member) {
    printf("  removing existing members for diff members\n");
    osm_members_free(relation->member);
    relation->member = NULL;
  }

  /* scan for members */
  member_t **member = &relation->member;
  xmlNode *member_node = NULL;
  for(member_node = node_rel->children; member_node; 
      member_node = member_node->next) {
    if(member_node->type == XML_ELEMENT_NODE) {
      if(strcasecmp((char*)member_node->name, "member") == 0) {
	/* attach member to member_chain */
	*member = osm_parse_osm_relation_member(osm, doc, member_node);
	if(*member) 
	  member = &((*member)->next);
      }
    }
  }
  
  /* node may be an existing node, so remove tags to */
  /* make space for new ones */
  if(relation->tag) {
    printf("  removing existing tags for diff tags\n");
    osm_tag_free(relation->tag);
    relation->tag = NULL;
  }
  
  relation->tag = xml_scan_tags(doc, node_rel->children, osm);
}

void diff_restore(appdata_t *appdata, project_t *project, osm_t *osm) {
  if(!project || !osm) return;

  char *diff_name = g_strdup_printf("%s/%s.diff", project->path, project->name);

  if(!g_file_test(diff_name, G_FILE_TEST_EXISTS)) {
    printf("no diff present!\n");
    g_free(diff_name);
    return;
  }
  
  printf("diff found, applying ...\n");
  
  xmlDoc *doc = NULL;
  xmlNode *root_element = NULL;
  
  /* parse the file and get the DOM */
  if((doc = xmlReadFile(diff_name, NULL, 0)) == NULL) {
    errorf(GTK_WIDGET(appdata->window), 
	   "Error: could not parse file %s\n", diff_name);
    g_free(diff_name);
    return;
  }
  
  /* Get the root element node */
  root_element = xmlDocGetRootElement(doc);
  
  xmlNode *cur_node = NULL;
  for (cur_node = root_element; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if(strcasecmp((char*)cur_node->name, "diff") == 0) {
	char *str = (char*)xmlGetProp(cur_node, BAD_CAST "name");
	if(str) {
	  printf("diff for project %s\n", str);
	  if(strcmp(project->name, str) != 0) {
	    messagef(GTK_WIDGET(appdata->window), _("Warning"), 
		     "Diff name (%s) does not match project name (%s)",
		     str, project->name);
	  }
	  xmlFree(str);
	}

	xmlNodePtr node_node = cur_node->children;
	while(node_node) {
	  if(node_node->type == XML_ELEMENT_NODE) {

	    if(strcasecmp((char*)node_node->name, "node") == 0) 
	      diff_restore_node(doc, node_node, osm);
	    
	    else if(strcasecmp((char*)node_node->name, "way") == 0) 
	      diff_restore_way(doc, node_node, osm);

	    else if(strcasecmp((char*)node_node->name, "relation") == 0) 
	      diff_restore_relation(doc, node_node, osm);

	    else 
	      printf("WARNING: item %s not restored\n", node_node->name);
	  }
	  node_node = node_node->next;
	}
      }
    }
  }
 
  g_free(diff_name);

  xmlFreeDoc(doc);
  xmlCleanupParser();

  /* check for hidden ways and update menu accordingly */
  gboolean something_is_hidden = FALSE;
  way_t *way = osm->way;
  while(!something_is_hidden && way) {
    if(way->flags & OSM_FLAG_HIDDEN)
      something_is_hidden = TRUE;

    way = way->next;
  }

  if(something_is_hidden) {
    printf("hidden flags have been restored, enable show_add menu\n");

    statusbar_set(appdata, _("Some objects are hidden"), TRUE);
    gtk_widget_set_sensitive(appdata->menu_item_map_show_all, TRUE);
  }
}
  
gboolean diff_present(project_t *project) {
  char *diff_name = g_strdup_printf("%s/%s.diff", project->path, project->name);
  
  if(!g_file_test(diff_name, G_FILE_TEST_EXISTS)) {
    printf("no diff present!\n");
    g_free(diff_name);
    return FALSE;
  }
  
  g_free(diff_name);
  return TRUE;
}

void diff_remove(project_t *project) {
  char *diff_name = g_strdup_printf("%s/%s.diff", project->path, project->name);
  g_remove(diff_name);
  g_free(diff_name);
}
