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

#ifndef __USE_XOPEN
#define __USE_XOPEN
#endif
#include <time.h>

#ifndef LIBXML_TREE_ENABLED
#error "Tree not enabled in libxml"
#endif

#include "appdata.h"
#include "banner.h"
#include "gps.h"
#include "misc.h"
#include "track.h"

/* make menu represent the track state */
void track_menu_set(appdata_t *appdata) {
  if(!appdata->window) return;

  gboolean present = (appdata->track.track != NULL);

  /* if a track is present, then it can be cleared or exported */
  gtk_widget_set_sensitive(appdata->track.menu_item_track_clear, present);
  gtk_widget_set_sensitive(appdata->track.menu_item_track_export, present);
}

gint track_points_count(const track_point_t *point)
{
  gint points = 0;

  while(point) {
    points++;
    point = point->next;
  }
  return points;
}

gboolean track_is_empty(const track_seg_t *seg) {
  return (seg->track_point == NULL) ? TRUE : FALSE;
}

static gboolean track_get_prop_pos(xmlNode *node, pos_t *pos) {
  xmlChar *str_lat = xmlGetProp(node, BAD_CAST "lat");
  xmlChar *str_lon = xmlGetProp(node, BAD_CAST "lon");

  if(!str_lon || !str_lat) {
    if(!str_lon) xmlFree(str_lon);
    if(!str_lat) xmlFree(str_lat);
    return FALSE;
  }

  pos->lat = g_ascii_strtod((const gchar*)str_lat, NULL);
  pos->lon = g_ascii_strtod((const gchar*)str_lon, NULL);

  xmlFree(str_lon);
  xmlFree(str_lat);

  return TRUE;
}

static track_point_t *track_parse_trkpt(xmlNode *a_node) {
  track_point_t *point = g_new0(track_point_t, 1);

  /* parse position */
  if(!track_get_prop_pos(a_node, &point->pos)) {
    g_free(point);
    return NULL;
  }

  point->altitude = NAN;
  /* scan for children */
  xmlNode *cur_node;
  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {

      /* elevation (altitude) */
      if(strcmp((char*)cur_node->name, "ele") == 0) {
	xmlChar *str = xmlNodeGetContent(cur_node);
	point->altitude = g_ascii_strtod((const gchar*)str, NULL);
	xmlFree(str);
      } else if(strcmp((char*)cur_node->name, "time") == 0) {
	struct tm time = { 0 };
	time.tm_isdst = -1;
	xmlChar *str = xmlNodeGetContent(cur_node);
	char *ptr = strptime((const char*)str, DATE_FORMAT, &time);
	if(ptr) point->time = mktime(&time);
	xmlFree(str);
      }
    }
  }

  return point;
}

/**
 * @brief parse a <trkseg>
 * @param a_node the <trkseg> node
 * @param points counter for the created points (will not be reset)
 * @param segs counter for the created segments (will not be reset)
 * @returns the first of the newly created track segments
 *
 * This may create multiple track_seg_t objects.
 */
static track_seg_t *track_parse_trkseg(xmlNode *a_node, gint *points, gint *segs) {
  xmlNode *cur_node;
  track_point_t **point = NULL;
  track_seg_t *ret = NULL;
  track_seg_t **seg = &ret;

  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if(strcasecmp((char*)cur_node->name, "trkpt") == 0) {
	track_point_t *cpnt = track_parse_trkpt(cur_node);
	if(cpnt) {
	  if(!point) {
	    /* start a new segment */
	    *seg = g_new0(track_seg_t, 1);
	    (*segs)++;
	    point = &((*seg)->track_point);
	  }
	  /* attach point to chain */
	  *point = cpnt;
	  point = &((*point)->next);
	  (*points)++;
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
  return ret;
}

static track_t *track_parse_trk(xmlNode *a_node, gint *points, gint *segs) {
  track_t *track = g_new0(track_t, 1);
  xmlNode *cur_node;
  *points = 0;
  *segs = 0;
  track_seg_t **last = &track->track_seg;

  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if(strcasecmp((char*)cur_node->name, "trkseg") == 0) {
        *last = track_parse_trkseg(cur_node, points, segs);
        while (*last)
          last = &((*last)->next);
      } else
	printf("found unhandled gpx/trk/%s\n", cur_node->name);

    }
  }
  return track;
}

static track_t *track_parse_gpx(xmlNode *a_node, gint *points, gint *segs) {
  track_t *track = NULL;
  xmlNode *cur_node;

  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if(strcasecmp((char*)cur_node->name, "trk") == 0) {
	if(!track)
	  track = track_parse_trk(cur_node, points, segs);
	else
	  printf("ignoring additional track\n");
      } else
	printf("found unhandled gpx/%s\n", cur_node->name);
    }
  }
  return track;
}

static track_t *track_parse_doc(xmlDocPtr doc, gint *points, gint *segs) {
  track_t *track = NULL;
  xmlNode *cur_node;

  for (cur_node = xmlDocGetRootElement(doc); cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      /* parse track file ... */
      if(strcasecmp((char*)cur_node->name, "gpx") == 0)
        track = track_parse_gpx(cur_node, points, segs);
      else
        printf("found unhandled %s\n", cur_node->name);
    }
  }

  return track;
}

static track_t *track_read(const char *filename, gboolean dirty) {
  printf("============================================================\n");
  printf("loading track %s\n", filename);

  xmlDoc *doc = NULL;

  /* parse the file and get the DOM */
  if((doc = xmlReadFile(filename, NULL, 0)) == NULL) {
    xmlErrorPtr	errP = xmlGetLastError();
    errorf(NULL, "While parsing \"%s\":\n\n%s", filename, errP->message);
    return NULL;
  }

  gint points, segs;
  track_t *track = track_parse_doc(doc, &points, &segs);
  xmlFreeDoc(doc);

  if(!track || !track->track_seg) {
    g_free(track);
    printf("track was empty/invalid track\n");
    return NULL;
  }

  track->dirty = dirty;
  printf("Track is %sdirty.\n", dirty?"":"not ");
  printf("%d points in %d segments\n", points, segs);

  return track;
}

static void track_point_free(track_point_t *point) {
  g_free(point);
}

static void track_seg_free(track_seg_t *seg) {
  track_point_t *point = seg->track_point;
  while(point) {
    track_point_t *next = point->next;
    track_point_free(point);
    point = next;
  }

  g_free(seg);
}

/* --------------------------------------------------------------- */

void track_clear(appdata_t *appdata) {
  track_t *track = appdata->track.track;
  if (! track) return;

  printf("clearing track\n");

  if(appdata && appdata->map)
    map_track_remove(appdata);

  appdata->track.track = NULL;
  track_menu_set(appdata);

  track_delete(track);
}

void track_delete(track_t *track) {
  track_seg_t *seg = track->track_seg;
  while(seg) {
    track_seg_t *next = seg->next;
    track_seg_free(seg);
    seg = next;
  }

  g_free(track);
}

/* ----------------------  saving track --------------------------- */

static void track_save_points(const track_point_t *point, xmlNodePtr node) {
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
      struct tm loctime;
      localtime_r(&point->time, &loctime);
      strftime(str, sizeof(str), DATE_FORMAT, &loctime);
      xmlNewTextChild(node_point, NULL, BAD_CAST "time", BAD_CAST str);
    }

    point = point->next;
  }
}

static void track_save_segs(const track_seg_t *seg, xmlNodePtr node) {
  while(seg) {
    xmlNodePtr node_seg = xmlNewChild(node, NULL, BAD_CAST "trkseg", NULL);
    track_save_points(seg->track_point, node_seg);
    seg = seg->next;
  }
}

/**
 * @brief write the track information to a GPX file
 * @param name the filename to write to
 * @param track the track data to write
 * @param doc previous xml
 *
 * If doc is given, the last track in doc is updated and all remaining ones
 * are appended. If doc is NULL all tracks are saved.
 *
 * doc is freed.
 */
static void track_write(const char *name, const track_t *track, xmlDoc *doc) {
  printf("writing track to %s\n", name);

  xmlNodePtr trk_node;
  const track_seg_t *it = track->track_seg;
  if(doc) {
    xmlNodePtr cur_node;
    xmlNodePtr root_node = xmlDocGetRootElement(doc);
    gboolean err = FALSE;
    if (!root_node || root_node->type != XML_ELEMENT_NODE ||
        strcasecmp((char*)root_node->name, "gpx") != 0 ) {
      err = TRUE;
    } else {
      cur_node = root_node->children;
      while(cur_node && cur_node->type != XML_ELEMENT_NODE)
        cur_node = cur_node->next;
      if(!cur_node || !cur_node->children ||
         strcasecmp((char*)cur_node->name, "trk") != 0) {
        err = TRUE;
      } else {
        trk_node = cur_node;
        /* assume that at most the last segment in the file was modified */
        for (cur_node = cur_node->children; cur_node->next; cur_node = cur_node->next) {
          // skip non-nodes, e.g. if the user inserted a comment
          if (cur_node->type != XML_ELEMENT_NODE)
            continue;
          // more tracks in the file than loaded, something is wrong
          if(it == NULL) {
            err = TRUE;
            break;
          }
          /* something else, this track is not written from osm2go */
          if(strcasecmp((char*)cur_node->name, "trkseg") != 0) {
            err = TRUE;
            break;
          }
          it = it->next;
        }
      }
    }
    if(err) {
      xmlFreeDoc(doc);
      doc = NULL;
    } else {
      /* drop the last entry from XML, it will be rewritten */
      xmlUnlinkNode(cur_node);
      xmlFreeNode(cur_node);
    }
  }

  if (!doc) {
    doc = xmlNewDoc(BAD_CAST "1.0");
    xmlNodePtr root_node = xmlNewNode(NULL, BAD_CAST "gpx");
    xmlNewProp(root_node, BAD_CAST "xmlns", BAD_CAST
               "http://www.topografix.com/GPX/1/0");
    xmlNewProp(root_node, BAD_CAST "creator", BAD_CAST PACKAGE " v" VERSION);
    it = track->track_seg;

    trk_node = xmlNewChild(root_node, NULL, BAD_CAST "trk", NULL);
    xmlDocSetRootElement(doc, root_node);
  }

  track_save_segs(it, trk_node);

  xmlSaveFormatFileEnc(name, doc, "UTF-8", 1);
  xmlFreeDoc(doc);
}

/* save track in project */
void track_save(project_t *project, track_t *track) {
  if(!project) return;

  /* no need to save again if it has already been saved */
  if(track && !track->dirty) {
    printf("track is not dirty, no need to save it (again)\n");
    return;
  }

  gchar *trk_name = g_strconcat(project->path, project->name, ".trk", NULL);

  if(!track) {
    g_remove(trk_name);
    g_free(trk_name);
    return;
  }

  /* check if there already is such a diff file and make it a backup */
  /* in case new diff saving fails */
  char *backup = g_strconcat(project->path, "backup.trk", NULL);
  xmlDocPtr doc = NULL;
  if(g_file_test(trk_name, G_FILE_TEST_IS_REGULAR)) {
    printf("backing up existing file \"%s\" to \"%s\"\n", trk_name, backup);
    g_remove(backup);
    g_rename(trk_name, backup);
    /* parse the old file and get the DOM */
    doc = xmlReadFile(backup, NULL, 0);
  }

  track_write(trk_name, track, doc);
  track->dirty = FALSE;

  /* if we reach this point writing the new file worked and we */
  /* can delete the backup */
  g_remove(backup);

  g_free(trk_name);
  g_free(backup);
}

void track_export(const track_t *track, const char *filename) {
  track_write(filename, track, NULL);
}

/* ----------------------  loading track --------------------------- */

gboolean track_restore(appdata_t *appdata) {
  const project_t *project = appdata->project;

  /* first try to open a backup which is only present if saving the */
  /* actual diff didn't succeed */
  gchar *trk_name = g_strconcat(project->path, "backup.trk", NULL);
  if(g_file_test(trk_name, G_FILE_TEST_EXISTS)) {
    printf("track backup present, loading it instead of real track ...\n");
  } else {
    g_free(trk_name);
    trk_name = g_strconcat(project->path, project->name, ".trk", NULL);

    if(!g_file_test(trk_name, G_FILE_TEST_EXISTS)) {
      printf("no track present!\n");
      g_free(trk_name);
      return FALSE;
    }
    printf("track found, loading ...\n");
  }

  appdata->track.track = track_read(trk_name, FALSE);
  g_free(trk_name);

  track_menu_set(appdata);

  printf("restored track\n");

  return TRUE;
}

static void track_end_segment(track_t *track) {
  if(!track) return;

  if(track->cur_seg) {
    printf("ending a segment\n");

    /* todo: check if segment only has 1 point */

    track->cur_seg = NULL;
  }
}

/**
 * @brief append the new position to the current track
 * @param appdata application data
 * @param pos the new position
 * @param alt the new altitude
 * @returns if the position changed
 * @retval FALSE if the GPS position marker needs to be redrawn (i.e. the position changed)
 */
static gboolean track_append_position(appdata_t *appdata, const pos_t *pos, float alt, const lpos_t *lpos) {
  track_t *track = appdata->track.track;

  /* no track at all? might be due to a "clear track" while running */
  if(!track) {
    printf("restarting after \"clear\"\n");
    track = appdata->track.track = g_new0(track_t, 1);
  }

  track_menu_set(appdata);

  if(!track->cur_seg) {
    printf("starting new segment\n");

    track_seg_t **seg = &(track->track_seg);
    while(*seg) seg = &((*seg)->next);

    *seg = track->cur_seg = g_new0(track_seg_t, 1);
  } else
    printf("appending to current segment\n");

  track_point_t **point;
  track_point_t *prev = track->cur_seg->track_point;
  if (prev) {
    while(prev->next)
      prev = prev->next;
    point = &(prev->next);
  } else {
    point = &(track->cur_seg->track_point);
  }

  /* don't append if point is the same as last time */
  gboolean ret;
  if(prev && prev->pos.lat == pos->lat &&
             prev->pos.lon == pos->lon) {
    printf("same value as last point -> ignore\n");
    ret = FALSE;
  } else {
    ret = TRUE;
    *point = g_new0(track_point_t, 1);
    (*point)->altitude = alt;
    (*point)->time = time(NULL);
    (*point)->pos = *pos;
    track->dirty = TRUE;

    if(!prev) {
      /* the segment can now be drawn for the first time */
      printf("initial draw\n");
      g_assert(!track->cur_seg->item_chain);
      map_track_draw_seg(appdata->map, track->cur_seg);
    } else {
      /* the segment has to be updated */
      g_assert(track->cur_seg->item_chain);
      map_track_update_seg(appdata->map, track->cur_seg);
    }
  }

  if(appdata->settings && appdata->settings->follow_gps) {
    if(!map_scroll_to_if_offscreen(appdata->map, lpos)) {
      if(!--appdata->track.warn_cnt) {
	/* warn user once a minute that the current gps */
	/* position is outside the working area */
	banner_show_info(appdata, _("GPS position outside working area!"));
	appdata->track.warn_cnt = 60;  // warn again after one minute
      }
    } else
      ret = TRUE;
  }

  return ret;
}

static void track_do_disable_gps(appdata_t *appdata) {
  gps_enable(appdata, FALSE);

  gps_register_callback(appdata, NULL);

  /* stopping the GPS removes the marker ... */
  map_track_remove_pos(appdata);

  /* ... and terminates the current segment if present */
  track_end_segment(appdata->track.track);
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

    gps_register_callback(appdata, NULL);

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
    lpos_t lpos;
    pos2lpos(appdata->osm->bounds, &pos, &lpos);
    if(track_append_position(appdata, &pos, alt, &lpos))
      map_track_pos(appdata, &lpos);
  } else {
    printf("no valid position\n");
    /* end segment */
    track_end_segment(appdata->track.track);
    map_track_remove_pos(appdata);
  }

  return TRUE;
}

static void track_do_enable_gps(appdata_t *appdata) {
  gps_enable(appdata, TRUE);
  appdata->track.warn_cnt = 1;

  if (!gps_register_callback(appdata, update)) {
    if(!appdata->track.track) {
      printf("GPS: no track yet, starting new one\n");
      appdata->track.track = g_new0(track_t, 1);
      appdata->track.track->dirty = FALSE;
    } else
      printf("GPS: extending existing track\n");
  }
}

void track_enable_gps(appdata_t *appdata, gboolean enable) {
  printf("request to %sable gps\n", enable?"en":"dis");

  gtk_widget_set_sensitive(appdata->track.menu_item_track_follow_gps, enable);

  if(enable) track_do_enable_gps(appdata);
  else       track_do_disable_gps(appdata);
}

track_t *track_import(const char *name) {
  printf("import %s\n", name);

  return track_read(name, TRUE);
}

// vim:et:ts=8:sw=2:sts=2:ai
