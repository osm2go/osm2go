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

#include <libxml/parser.h>
#include <libxml/tree.h>

#include "appdata.h"

#ifndef LIBXML_TREE_ENABLED
#error "Tree not enabled in libxml"
#endif

// predecs
static void track_enable_gps(appdata_t *appdata);
static void track_disable_gps(appdata_t *appdata);

/* enable/disable menu with respect to mode */
void track_set_mode(appdata_t *appdata, track_t *track, track_mode_t mode) {
  /* import and gps are always enabled */
  const gboolean clear[]  = { FALSE, TRUE,  TRUE };
  const gboolean export[] = { FALSE, FALSE, TRUE };

  gtk_widget_set_sensitive(appdata->track.menu_item_clear, clear[mode]);
  gtk_widget_set_sensitive(appdata->track.menu_item_export, export[mode]);

  /* adjust menu item if required */
  if((mode == TRACK_GPS) && 
     !gtk_check_menu_item_get_active(
		    GTK_CHECK_MENU_ITEM(appdata->track.menu_item_gps)))
     gtk_check_menu_item_set_active(
		    GTK_CHECK_MENU_ITEM(appdata->track.menu_item_gps), TRUE);

  if((mode != TRACK_GPS) && 
     gtk_check_menu_item_get_active(
		    GTK_CHECK_MENU_ITEM(appdata->track.menu_item_gps)))
     gtk_check_menu_item_set_active(
		    GTK_CHECK_MENU_ITEM(appdata->track.menu_item_gps), FALSE);

  if(track)
    track->mode = mode;
}

gint track_seg_points(track_seg_t *seg) {
  gint points = 0;

  track_point_t *point = seg->track_point;
  while(point) {
    points++;
    point = point->next;
  }
  return points;
}

static gboolean track_get_prop_pos(xmlNode *node, pos_t *pos) {
  char *str_lat = (char*)xmlGetProp(node, BAD_CAST "lat");
  char *str_lon = (char*)xmlGetProp(node, BAD_CAST "lon");

  if(!str_lon || !str_lat) {
    if(!str_lon) xmlFree(str_lon);
    if(!str_lat) xmlFree(str_lat);
    return FALSE;
  }

  pos->lat = g_ascii_strtod(str_lat, NULL);
  pos->lon = g_ascii_strtod(str_lon, NULL);

  xmlFree(str_lon);
  xmlFree(str_lat);

  return TRUE;
}

static track_point_t *track_parse_trkpt(bounds_t *bounds, xmlDocPtr doc, 
					xmlNode *a_node) {
  track_point_t *point = NULL;
  pos_t pos;

  /* parse position */
  if(!track_get_prop_pos(a_node, &pos)) 
    return NULL;

  point = g_new0(track_point_t, 1);

  pos2lpos(bounds, &pos, &point->lpos);

  /* check if point is within bounds */
  if((point->lpos.x < bounds->min.x) || (point->lpos.x > bounds->max.x) ||
     (point->lpos.y < bounds->min.y) || (point->lpos.y > bounds->max.y)) {
    g_free(point);
    point = NULL;
  }

  return point;
}

static void track_parse_trkseg(track_t *track, bounds_t *bounds, 
			       xmlDocPtr doc, xmlNode *a_node) {
  xmlNode *cur_node = NULL;
  track_point_t **point = NULL;
  track_seg_t **seg = &(track->track_seg);

  /* search end of track_seg list */
  while(*seg) seg = &((*seg)->next);

  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if(strcasecmp((char*)cur_node->name, "trkpt") == 0) {
	track_point_t *cpnt = track_parse_trkpt(bounds, doc, cur_node);
	if(cpnt) {
	  if(!point) {
	    /* start a new segment */
	    *seg = g_new0(track_seg_t, 1);
	    point = &((*seg)->track_point);
	  }
	  /* attach point to chain */
	  *point = cpnt;
	  point = &((*point)->next);
	} else {
	  /* end segment if point could not be parsed and start a new one */
	  /* close segment if there is one */
	  if(point) {
	    printf("ending track segment leaving bounds\n");
	    seg = &((*seg)->next);
	    point = NULL;
	  }
	}
      } else
	printf("found unhandled gpx/trk/trkseg/%s\n", cur_node->name);
      
    }
  }
}

static track_t *track_parse_trk(bounds_t *bounds, 
				xmlDocPtr doc, xmlNode *a_node) {
  track_t *track = g_new0(track_t, 1);
  xmlNode *cur_node = NULL;

  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if(strcasecmp((char*)cur_node->name, "trkseg") == 0) {
	track_parse_trkseg(track, bounds, doc, cur_node);
      } else
	printf("found unhandled gpx/trk/%s\n", cur_node->name);
      
    }
  }
  return track;
}

static track_t *track_parse_gpx(bounds_t *bounds, 
				xmlDocPtr doc, xmlNode *a_node) {
  track_t *track = NULL;
  xmlNode *cur_node = NULL;

  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if(strcasecmp((char*)cur_node->name, "trk") == 0) {
	if(!track) 
	  track = track_parse_trk(bounds, doc, cur_node);
	else
	  printf("ignoring additional track\n");
      } else
	printf("found unhandled gpx/%s\n", cur_node->name);      
    }
  }
  return track;
}

/* parse root element and search for "track" */
static track_t *track_parse_root(bounds_t *bounds, 
				 xmlDocPtr doc, xmlNode *a_node) {
  track_t *track = NULL;
  xmlNode *cur_node = NULL;

  for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      /* parse track file ... */
      if(strcasecmp((char*)cur_node->name, "gpx") == 0) 
      	track = track_parse_gpx(bounds, doc, cur_node);
      else 
	printf("found unhandled %s\n", cur_node->name);
    }
  }
  return track;
}

static track_t *track_parse_doc(bounds_t *bounds, xmlDocPtr doc) {
  track_t *track;

  /* Get the root element node */
  xmlNode *root_element = xmlDocGetRootElement(doc);

  track = track_parse_root(bounds, doc, root_element);  

  /*free the document */
  xmlFreeDoc(doc);

  /*
   * Free the global variables that may
   * have been allocated by the parser.
   */
  xmlCleanupParser();

  return track;
}

void track_info(track_t *track) {
  printf("Loaded track: %s\n", track->filename);
  printf("Track has %sbeen saved in project.\n", track->saved?"":"not ");

  gint segs = 0, points = 0;
  track_seg_t *seg = track->track_seg;
  while(seg) {
    points += track_seg_points(seg);
    segs++;
    seg = seg->next;
  }

  printf("%d points in %d segments\n", points, segs);

}

static track_t *track_import(osm_t *osm, char *filename) {
  printf("============================================================\n");
  printf("loading track %s\n", filename);
 
  xmlDoc *doc = NULL;

  LIBXML_TEST_VERSION;
  
  /* parse the file and get the DOM */
  if((doc = xmlReadFile(filename, NULL, 0)) == NULL) {
    xmlErrorPtr	errP = xmlGetLastError();
    errorf(NULL, "While parsing \"%s\":\n\n%s", filename, errP->message);
    return NULL;
  }

  track_t *track = track_parse_doc(osm->bounds, doc); 
  track->filename = g_strdup(filename);
  track->saved = FALSE;

  track_info(track);
  
  return track;
}

void track_point_free(track_point_t *point) {
  g_free(point);
}

void track_seg_free(track_seg_t *seg) {
  track_point_t *point = seg->track_point;
  while(point) {
    track_point_t *next = point->next;
    track_point_free(point);
    point = next;
  }

  g_free(seg);
}

/* --------------------------------------------------------------- */

void track_clear(appdata_t *appdata, track_t *track) {
  if (! track)
    return;
  printf("clearing track\n");

  if(appdata->map)
    map_track_remove(appdata);

  track_seg_t *seg = track->track_seg;
  while(seg) {
    track_seg_t *next = seg->next;
    track_seg_free(seg);
    seg = next;
  }

  g_free(track->filename);
  g_free(track);
}

/* ----------------------  saving track --------------------------- */

void track_save_points(track_point_t *point, xmlNodePtr node) {
  while(point) {
    xmlNodePtr node_point = xmlNewChild(node, NULL, BAD_CAST "point", NULL);
 
    char *str = g_strdup_printf("%d", point->lpos.x);
    xmlNewProp(node_point, BAD_CAST "x", BAD_CAST str);
    g_free(str);
    
    str = g_strdup_printf("%d", point->lpos.y);
    xmlNewProp(node_point, BAD_CAST "y", BAD_CAST str);
    g_free(str);

    point = point->next;
  }
}

void track_save_segs(track_seg_t *seg, xmlNodePtr node) {
  while(seg) {
    xmlNodePtr node_seg = xmlNewChild(node, NULL, BAD_CAST "seg", NULL);
    track_save_points(seg->track_point, node_seg);
    seg = seg->next;
  }
}

/* save track in project */
void track_save(project_t *project, track_t *track) {
  if(!project) return;

  char *trk_name = g_strdup_printf("%s/%s.trk", project->path, project->name);

  if(!track) {
    g_remove(trk_name);
    g_free(trk_name);
    return;
  }

  /* no need to save again if it has already been saved */
  if(track->saved) {
    printf("track already saved, don't save it again\n");
    g_free(trk_name);
    return;
  }

  printf("saving track\n");

  LIBXML_TEST_VERSION;
 
  xmlDocPtr doc = xmlNewDoc(BAD_CAST "1.0");
  xmlNodePtr root_node = xmlNewNode(NULL, BAD_CAST "trk");
  xmlNewProp(root_node, BAD_CAST "filename", BAD_CAST track->filename);
  char *mode_str = g_strdup_printf("%d", track->mode);
  xmlNewProp(root_node, BAD_CAST "mode", BAD_CAST mode_str);
  g_free(mode_str);
  xmlDocSetRootElement(doc, root_node);
  
  track_save_segs(track->track_seg, root_node);

  xmlSaveFormatFileEnc(trk_name, doc, "UTF-8", 1);
  xmlFreeDoc(doc);
  xmlCleanupParser();

  g_free(trk_name);

  track->saved = TRUE;
}

/* ----------------------  loading track --------------------------- */

static int xml_get_prop_int(xmlNode *node, char *prop) {
  char *str = (char*)xmlGetProp(node, BAD_CAST prop);
  int value = 0;

  if(str) {
    value = strtoul(str, NULL, 10);
    xmlFree(str);
  }

  return value;
}

track_t *track_restore(appdata_t *appdata, project_t *project) {
  char *trk_name = g_strdup_printf("%s/%s.trk", project->path, project->name);
  track_t *track = NULL;
  
  LIBXML_TEST_VERSION;
 
  if(!g_file_test(trk_name, G_FILE_TEST_EXISTS)) {
    printf("no track present!\n");
    g_free(trk_name);
    return NULL;
  }
  
  printf("track found, loading ...\n");
  
  xmlDoc *doc = NULL;
  xmlNode *root_element = NULL;
  
  /* parse the file and get the DOM */
  if((doc = xmlReadFile(trk_name, NULL, 0)) == NULL) {
    errorf(GTK_WIDGET(appdata->window), 
	   "Error: could not parse file %s\n", trk_name);
    g_free(trk_name);
    return NULL;
  }
  
  /* Get the root element node */
  root_element = xmlDocGetRootElement(doc);
  
  xmlNode *cur_node = NULL;
  for (cur_node = root_element; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if(strcasecmp((char*)cur_node->name, "trk") == 0) {
	printf("found track\n");

	track = g_new0(track_t, 1);
	track->mode = xml_get_prop_int(cur_node, "mode");

	char *str = (char*)xmlGetProp(cur_node, BAD_CAST "filename");
	if(str) {
	  track->filename = g_strdup(str);
	  xmlFree(str);
	}

	xmlNodePtr seg_node = cur_node->children;
	track_seg_t **seg = &track->track_seg;
	while(seg_node) {
	  if(seg_node->type == XML_ELEMENT_NODE) {

	    if(strcasecmp((char*)seg_node->name, "seg") == 0) {
	      *seg = g_new0(track_seg_t, 1);

	      xmlNodePtr point_node = seg_node->children;
	      track_point_t **point = &(*seg)->track_point;
	      while(point_node) {
		if(point_node->type == XML_ELEMENT_NODE) {

		  if(strcasecmp((char*)point_node->name, "point") == 0) {
		    *point = g_new0(track_point_t, 1);
		    (*point)->lpos.x = xml_get_prop_int(point_node, "x");
		    (*point)->lpos.y = xml_get_prop_int(point_node, "y");

		    point = &((*point)->next);
		  }
		}
		point_node = point_node->next;
	      }

	      seg = &((*seg)->next);
	    }
	  }
	  seg_node = seg_node->next;
	}
      }
    }
  }

  g_free(trk_name);

  printf("restoring track mode %d\n", track->mode);
  track_set_mode(appdata, track, track->mode);
  track->saved = TRUE;
  track_info(track);

  return track;
}

static void track_end_segment(track_t *track) {
  if(track->cur_seg) {
    printf("ending a segment\n");
   
    /* todo: check if segment only has 1 point */
 
    track->cur_seg = NULL;
  }
}

static void track_append_position(appdata_t *appdata, pos_t *pos) {
  track_t *track = appdata->track.track;

  if(!track->cur_seg) {
    printf("starting new segment\n");
    
    track_seg_t **seg = &(track->track_seg);
    while(*seg) seg = &((*seg)->next);

    *seg = track->cur_seg = g_new0(track_seg_t, 1);
  } else
    printf("appending to current segment\n");

  gint seg_len = 0;
  track_point_t **point = &(track->cur_seg->track_point);
  while(*point) { seg_len++; point = &((*point)->next); }

  /* create utm coordinates */
  lpos_t lpos;
  bounds_t *bounds = appdata->osm->bounds;
  pos2lpos(bounds, pos, &lpos);

  /* check if point is within bounds */
  if((lpos.x < bounds->min.x) || (lpos.x > bounds->max.x) ||
     (lpos.y < bounds->min.y) || (lpos.y > bounds->max.y)) {
    printf("position out of bounds\n");

    /* end segment */
    track_end_segment(track);
    map_track_pos(appdata, NULL);
  } else {
    map_track_pos(appdata, &lpos);

    /* don't append if point is the same as last time */
    track_point_t *prev = track->cur_seg->track_point;
    while(prev && prev->next) prev = prev->next;

    if(prev && prev->lpos.x == lpos.x && prev->lpos.y == lpos.y) {
      printf("same value as last point -> ignore\n");
    } else {

      *point = g_new0(track_point_t, 1);
      (*point)->lpos.x = lpos.x;
      (*point)->lpos.y = lpos.y;
      track->saved = FALSE;

      /* if segment length was 1 the segment can now be drawn for the first time */
      if(seg_len <= 1) {
	printf("initial/second draw with seg_len %d\n", seg_len);

	if(seg_len == 0) g_assert(!track->cur_seg->item);
	else             g_assert(track->cur_seg->item);

	map_track_draw_seg(appdata->map, track->cur_seg);
      }
      
      /* if segment length was > 1 the segment has to be updated */
      if(seg_len > 1) {
	printf("update draw\n");
	
	g_assert(track->cur_seg->item);
	map_track_update_seg(appdata->map, track->cur_seg);
      }
    }

#ifdef USE_GOOCANVAS
    map_scroll_to_if_offscreen(appdata->map, &lpos);
#endif
  }
}

static gboolean update(gpointer data) {
  appdata_t *appdata = (appdata_t*)data;

  if(! appdata->map) {
    printf("map has gone while tracking was active, stopping tracker\n");
    
    if(appdata->track.handler_id) {
      gtk_timeout_remove(appdata->track.handler_id);
      appdata->track.handler_id = 0;
    }

    return FALSE;
  }

  if (! appdata->gps_enabled) {
    // Turn myself off gracefully.
    track_disable_gps(appdata);
    return FALSE;
  }

  pos_t *pos = gps_get_pos(appdata);
  if(pos) {
    printf("valid position %f/%f\n", pos->lat, pos->lon);
    track_append_position(appdata, pos);
  } else {
    printf("no valid position\n");
    /* end segment */
    track_end_segment(appdata->track.track);
    map_track_pos(appdata, NULL);
  }

  return TRUE;
}

static void track_enable_gps(appdata_t *appdata) {
  gps_enable(appdata, TRUE);

  if(!appdata->track.handler_id) {
    appdata->track.handler_id = gtk_timeout_add(1000, update, appdata);

    if(appdata->track.track) {
      printf("there's already a track -> restored!!\n");
      map_track_draw(appdata->map, appdata->track.track);
    } else
      appdata->track.track = g_new0(track_t, 1);
  }
}

static void track_disable_gps(appdata_t *appdata) {
  gps_enable(appdata, FALSE);

  if(appdata->track.handler_id) {
    gtk_timeout_remove(appdata->track.handler_id);
    appdata->track.handler_id = 0;
  }
}

void track_do(appdata_t *appdata, track_mode_t mode, char *name) {

  printf("track do %d\n", mode);

  /* remove existing track */
  if(appdata->track.track) {
    track_clear(appdata, appdata->track.track);
    appdata->track.track = NULL;
    map_track_pos(appdata, NULL);
  }

  switch(mode) {
  case TRACK_NONE:
    /* disable gps if it was on */
    track_disable_gps(appdata);    

    track_set_mode(appdata, appdata->track.track, TRACK_NONE);     
    break;

  case TRACK_IMPORT:
    /* disable gps if it was on */
    track_disable_gps(appdata);    

    appdata->track.track = track_import(appdata->osm, name);
    map_track_draw(appdata->map, appdata->track.track);
    track_set_mode(appdata, appdata->track.track, TRACK_IMPORT);
   break;

  case TRACK_GPS:
    track_enable_gps(appdata);    

    track_set_mode(appdata, appdata->track.track, TRACK_GPS);
    break;
  }
}
// vim:et:ts=8:sw=2:sts=2:ai
