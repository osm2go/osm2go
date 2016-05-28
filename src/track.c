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

#define __USE_XOPEN
#include <time.h>

#include "appdata.h"
#include "project.h"

#ifndef LIBXML_TREE_ENABLED
#error "Tree not enabled in libxml"
#endif

// predecs
static void track_do_enable_gps(appdata_t *appdata);
static void track_do_disable_gps(appdata_t *appdata);
enum {
  PROJECT_COL_NAME = 0,
  PROJECT_COL_STATUS,
  PROJECT_COL_DESCRIPTION,
  PROJECT_COL_DATA,
  PROJECT_NUM_COLS
};*

/* make menu represent the track state */
static void track_menu_set(appdata_t *appdata, gboolean present) {
  if(!appdata->window) return;

  /* if a track is present, then it can be cleared or exported */
  gtk_widget_set_sensitive(appdata->track.menu_item_track_clear, present);
  gtk_widget_set_sensitive(appdata->track.menu_item_track_export, present);
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
  track_point_t *point = g_new0(track_point_t, 1);
  point->altitude = NAN;

  /* parse position */
  if(!track_get_prop_pos(a_node, &point->pos)) {
    g_free(point);
    return NULL;
  }

  /* scan for children */
  xmlNode *cur_node = NULL;
  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {

      /* elevation (altitude) */
      if(strcasecmp((char*)cur_node->name, "ele") == 0) {
	char *str = (char*)xmlNodeGetContent(cur_node);
	point->altitude = g_ascii_strtod(str, NULL);
 	xmlFree(str);
      }

      /* time */
      if(strcasecmp((char*)cur_node->name, "time") == 0) {
	struct tm time;
	char *str = (char*)xmlNodeGetContent(cur_node);
	char *ptr = strptime(str, DATE_FORMAT, &time);
	if(ptr) point->time = mktime(&time);
 	xmlFree(str);
      }
    }
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

  return track;
}

void track_info(track_t *track) {
  printf("Track is %sdirty.\n", track->dirty?"":"not ");

  gint segs = 0, points = 0;
  track_seg_t *seg = track->track_seg;
  while(seg) {
    points += track_seg_points(seg);
    segs++;
    seg = seg->next;
  }

  printf("%d points in %d segments\n", points, segs);

}

static track_t *track_read(osm_t *osm, char *filename) {
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

  if(!track || !track->track_seg) {
    printf("track was empty/invalid track\n");
    return NULL;
  }

  track->dirty = TRUE;
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
  if (! track) return;

  printf("clearing track\n");

  if(appdata && appdata->map)
    map_track_remove(appdata);

  track_seg_t *seg = track->track_seg;
  while(seg) {
    track_seg_t *next = seg->next;
    track_seg_free(seg);
    seg = next;
  }

  g_free(track);

  track_menu_set(appdata, FALSE);
}

/* ----------------------  saving track --------------------------- */

void track_save_points(track_point_t *point, xmlNodePtr node) {
  while(point) {
    char str[G_ASCII_DTOSTR_BUF_SIZE];

    xmlNodePtr node_point = xmlNewChild(node, NULL, BAD_CAST "trkpt", NULL);

    g_ascii_formatd(str, sizeof(str), LL_FORMAT, point->pos.lat);
    xmlNewProp(node_point, BAD_CAST "lat", BAD_CAST str);
    
    g_ascii_formatd(str, sizeof(str), LL_FORMAT, point->pos.lon);
    xmlNewProp(node_point, BAD_CAST "lon", BAD_CAST str);

    if(!isnan(point->altitude)) {
      g_ascii_formatd(str, sizeof(str), ALT_FORMAT, point->altitude);
      xmlNewTextChild(node_point, NULL, BAD_CAST "ele", BAD_CAST str);
    }

    if(point->time) {
      strftime(str, sizeof(str), DATE_FORMAT, localtime(&point->time));
      xmlNewTextChild(node_point, NULL, BAD_CAST "time", BAD_CAST str);
    }

    point = point->next;
  }
}

void track_save_segs(track_seg_t *seg, xmlNodePtr node) {
  while(seg) {
    xmlNodePtr node_seg = xmlNewChild(node, NULL, BAD_CAST "trkseg", NULL);
    track_save_points(seg->track_point, node_seg);
    seg = seg->next;
  }
}

void track_write(char *name, track_t *track) {
  printf("writing track to %s\n", name);

  LIBXML_TEST_VERSION;
 
  xmlDocPtr doc = xmlNewDoc(BAD_CAST "1.0");
  xmlNodePtr root_node = xmlNewNode(NULL, BAD_CAST "gpx");
  xmlNewProp(root_node, BAD_CAST "creator", BAD_CAST PACKAGE " v" VERSION);
  xmlNewProp(root_node, BAD_CAST "xmlns", BAD_CAST 
	     "http://www.topografix.com/GPX/1/0");

  xmlNodePtr trk_node = xmlNewChild(root_node, NULL, BAD_CAST "trk", NULL);
  xmlDocSetRootElement(doc, root_node);
  
  track_save_segs(track->track_seg, trk_node);

  xmlSaveFormatFileEnc(name, doc, "UTF-8", 1);
  xmlFreeDoc(doc);

  track->dirty = FALSE;
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
  if(!track->dirty) {
    printf("track is not dirty, no need to save it (again)\n");
    g_free(trk_name);
    return;
  }

  /* check if there already is such a diff file and make it a backup */
  /* in case new diff saving fails */
  char *backup = g_strjoin(NULL, project->path, "backup.trk", NULL);
  if(g_file_test(trk_name, G_FILE_TEST_IS_REGULAR)) {
    printf("backing up existing file \"%s\" to \"%s\"\n", trk_name, backup);
    g_remove(backup);
    g_rename(trk_name, backup);
  }

  track_write(trk_name, track);

  /* if we reach this point writing the new file worked and we */
  /* can delete the backup */
  g_remove(backup);
  
  g_free(trk_name);
  g_free(backup);
}

void track_export(appdata_t *appdata, char *filename) {
  g_assert(appdata->track.track);
  track_write(filename, appdata->track.track);  
}

/* ----------------------  loading track --------------------------- */

track_t *track_restore(appdata_t *appdata, project_t *project) {
  track_t *track = NULL;
  
  LIBXML_TEST_VERSION;
 
  /* first try to open a backup which is only present if saving the */
  /* actual diff didn't succeed */
  char *trk_name = g_strjoin(NULL, project->path, "backup.trk", NULL);
  if(g_file_test(trk_name, G_FILE_TEST_EXISTS)) {
    printf("track backup present, loading it instead of real track ...\n");
  } else {
    g_free(trk_name);
    trk_name = g_strdup_printf("%s/%s.trk", project->path, project->name);

    if(!g_file_test(trk_name, G_FILE_TEST_EXISTS)) {
      printf("no track present!\n");
      g_free(trk_name);
      return NULL;
    }
    printf("track found, loading ...\n");
  }

  track = track_read(appdata->osm, trk_name);
  g_free(trk_name);

  track_menu_set(appdata, track != NULL);
  
  printf("restored track\n");
  if(track) {
    track->dirty = FALSE;
    track_info(track);
  }

  return track;
}

static void track_end_segment(track_t *track) {
  if(!track) return;
    
  if(track->cur_seg) {
    printf("ending a segment\n");
   
    /* todo: check if segment only has 1 point */
 
    track->cur_seg = NULL;
  }
}

static void track_append_position(appdata_t *appdata, pos_t *pos, float alt) {
  track_t *track = appdata->track.track;

  track_menu_set(appdata, TRUE);

  /* no track at all? might be due to a "clear track" while running */
  if(!track) {
    printf("restarting after \"clear\"\n");
    track = appdata->track.track = g_new0(track_t, 1);
  }

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

  map_track_pos(appdata, pos);

  /* don't append if point is the same as last time */
  track_point_t *prev = track->cur_seg->track_point;
  while(prev && prev->next) prev = prev->next;

  if(prev && prev->pos.lat == pos->lat && 
             prev->pos.lon == pos->lon) {
    printf("same value as last point -> ignore\n");
  } else {

    *point = g_new0(track_point_t, 1);
    (*point)->altitude = alt;
    (*point)->time = time(NULL);
    (*point)->pos = *pos;
    track->dirty = TRUE;

    /* if segment length was 1 the segment can now be drawn */
    /* for the first time */
    if(!seg_len) {
      printf("initial draw\n");
      g_assert(!track->cur_seg->item_chain);
      map_track_draw_seg(appdata->map, track->cur_seg);
    }
      
    /* if segment length was > 0 the segment has to be updated */
    if(seg_len > 0) {
      printf("update draw with seg_len %d\n", seg_len+1);
      
      g_assert(track->cur_seg->item_chain);
      map_track_update_seg(appdata->map, track->cur_seg);
    }
  }
  
  if(appdata->settings && appdata->settings->follow_gps) {
    lpos_t lpos;
    char * name;
    project_t * temp;
    pos2lpos(appdata->osm->bounds, pos, &lpos);
    if(!map_scroll_to_if_offscreen(appdata->map, &lpos)) {
      if(!--appdata->track.warn_cnt) {
	/* warn user once a minute that the current gps */
	/* position is outside the working area */
	banner_show_info(appdata, _("GPS position outside working area!"));
	temp=PROJECT_COL_DATA;
	while(temp!=NULL){
		if(*temp->osm){
			if(temp->min.lat){
		/*if position is inside project bounds , then name=*temp->name; and exit the loop */
			}
		}
	}
	/* closing current project */
	if(*name){
	project_save(GTK_WIDGET(appdata->window), appdata->project);
/* or this one:	project_close(appdata); */
	/* opening next project */
	project_open(appdata, name);
	}
	appdata->track.warn_cnt = 60;  // warn again after one minute
      }
    }
  }
}

static gboolean update(gpointer data) {
  appdata_t *appdata = (appdata_t*)data;
  
  /* ignore updates while no valid osm file is loaded, e.g. when switching */
  /* projects */
  if(!appdata->osm)
    return TRUE;

  /* the map is only gone of the main screen is being closed */
  if(!appdata->map) {
    printf("map has gone while tracking was active, stopping tracker\n");
    
#ifndef ENABLE_LIBLOCATION
    if(appdata->track.handler_id) {
      gtk_timeout_remove(appdata->track.handler_id);
      appdata->track.handler_id = 0;
    }
#else
    if(appdata->gps_state->cb) 
      appdata->gps_state->cb = NULL;
#endif

    return FALSE;
  }

  if(!appdata->settings || !appdata->settings->enable_gps) {
    // Turn myself off gracefully.
    track_do_disable_gps(appdata);
    return FALSE;
  }

  pos_t pos;
  float alt;
  if(gps_get_pos(appdata, &pos, &alt)) {
    printf("valid position %.6f/%.6f alt %.2f\n", pos.lat, pos.lon, alt);
    track_append_position(appdata, &pos, alt);
  } else {
    printf("no valid position\n");
    /* end segment */
    track_end_segment(appdata->track.track);
    map_track_pos(appdata, NULL);
  }

  return TRUE;
}

static void track_do_enable_gps(appdata_t *appdata) {
  gps_enable(appdata, TRUE);
  appdata->track.warn_cnt = 1;

#ifndef ENABLE_LIBLOCATION
  if(!appdata->track.handler_id) {
    appdata->track.handler_id = gtk_timeout_add(1000, update, appdata);
#else
  g_assert(appdata->gps_state);
  if(!appdata->gps_state->cb) {
    appdata->gps_state->cb = update;
    appdata->gps_state->data = appdata;
#endif

    if(!appdata->track.track) {
      printf("GPS: no track yet, starting new one\n");
      appdata->track.track = g_new0(track_t, 1);
      appdata->track.track->dirty = FALSE;
    } else
      printf("GPS: extending existing track\n");
  }
}

static void track_do_disable_gps(appdata_t *appdata) {
  gps_enable(appdata, FALSE);

#ifndef ENABLE_LIBLOCATION
  if(appdata->track.handler_id) {
    gtk_timeout_remove(appdata->track.handler_id);
    appdata->track.handler_id = 0;
  }
#else
  if(appdata->gps_state->cb) 
    appdata->gps_state->cb = NULL;
#endif

  /* stopping the GPS removes the marker ... */
  map_track_pos(appdata, NULL);

  /* ... and terminates the current segment if present */
  if(appdata->track.track)
    appdata->track.track->cur_seg = NULL;
}

void track_enable_gps(appdata_t *appdata, gboolean enable) {
  printf("request to %sable gps\n", enable?"en":"dis");

  gtk_widget_set_sensitive(appdata->track.menu_item_track_follow_gps, enable);

  if(enable) track_do_enable_gps(appdata);
  else       track_do_disable_gps(appdata);
}

track_t *track_import(appdata_t *appdata, char *name) {
  printf("import %s\n", name);

  /* remove any existing track */
  if(appdata->track.track) {
    track_clear(appdata, appdata->track.track);
    appdata->track.track = NULL;
  }

  track_t *track = track_read(appdata->osm, name);
  track_menu_set(appdata, track != NULL);

  if(track) {
    map_track_draw(appdata->map, track);
    track->dirty = TRUE;
  }

  return track;
}

// vim:et:ts=8:sw=2:sts=2:ai
