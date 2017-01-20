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

/* make sure strptime() is defined */
#ifndef __USE_XOPEN
#define _XOPEN_SOURCE 600
#endif

#include "track.h"

#include "appdata.h"
#include "banner.h"
#include "gps.h"
#include "misc.h"

#include <cstring>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <map>
#include <strings.h>
#include <time.h>

#ifndef LIBXML_TREE_ENABLED
#error "Tree not enabled in libxml"
#endif

#include <algorithm>

class TrackSax {
  xmlSAXHandler handler;
  bool active_seg;
  track_point_t *curPoint;

  enum State {
    DocStart,
    TagGpx,
    TagTrk,
    TagTrkSeg,
    TagTrkPt,
    TagTime,
    TagEle
  };

  State state;

  // custom find to avoid memory allocations for std::string
  struct tag_find {
    const char * const name;
    tag_find(const xmlChar *n) : name(reinterpret_cast<const char *>(n)) {}
    bool operator()(const std::pair<const char *, std::pair<State, State> > &p) {
      return (strcmp(p.first, name) == 0);
    }
  };

public:
  TrackSax();

  bool parse(const char *filename);

  track_t *track;
  std::vector<track_point_t>::size_type points;  ///< total points

private:
  std::map<const char *, std::pair<State, State> > tags;

  void characters(const char *ch, int len);
  static void cb_characters(void *ts, const xmlChar *ch, int len) {
    static_cast<TrackSax *>(ts)->characters(reinterpret_cast<const char *>(ch), len);
  }
  void startElement(const xmlChar *name, const xmlChar **attrs);
  static void cb_startElement(void *ts, const xmlChar *name, const xmlChar **attrs) {
    static_cast<TrackSax *>(ts)->startElement(name, attrs);
  }
  void endElement(const xmlChar *name);
  static void cb_endElement(void *ts, const xmlChar *name) {
    static_cast<TrackSax *>(ts)->endElement(name);
  }
};

/* make menu represent the track state */
void track_menu_set(appdata_t *appdata) {
  if(!appdata->window) return;

  gboolean present = (appdata->track.track != NULL);

  /* if a track is present, then it can be cleared or exported */
  gtk_widget_set_sensitive(appdata->track.menu_item_track_clear, present);
  gtk_widget_set_sensitive(appdata->track.menu_item_track_export, present);
}

static track_t *track_read(const char *filename, gboolean dirty) {
  printf("============================================================\n");
  printf("loading track %s\n", filename);

  TrackSax sx;
  if(!sx.parse(filename)) {
    delete sx.track;
    printf("track was empty/invalid track\n");
    return 0;
  }

  sx.track->dirty = dirty;
  printf("Track is %sdirty.\n", dirty?"":"not ");
  printf("%zu points in %zu segments\n", sx.points, sx.track->segments.size());

  return sx.track;
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

  delete track;
}

/* ----------------------  saving track --------------------------- */

struct track_save_segs {
  xmlNodePtr const node;
  track_save_segs(xmlNodePtr n) : node(n) {}

  struct save_point {
    xmlNodePtr const node;
    save_point(xmlNodePtr n) : node(n) {}
    void operator()(const track_point_t &point);
  };

  void operator()(const track_seg_t &seg) {
    xmlNodePtr node_seg = xmlNewChild(node, NULL, BAD_CAST "trkseg", NULL);
    std::for_each(seg.track_points.begin(), seg.track_points.end(),
                  save_point(node_seg));
  }
};

void track_save_segs::save_point::operator()(const track_point_t &point)
{
  char str[G_ASCII_DTOSTR_BUF_SIZE];

  xmlNodePtr node_point = xmlNewChild(node, NULL, BAD_CAST "trkpt", NULL);

  xml_set_prop_pos(node_point, &point.pos);

  if(!isnan(point.altitude)) {
    g_ascii_formatd(str, sizeof(str), ALT_FORMAT, point.altitude);
    xmlNewTextChild(node_point, NULL, BAD_CAST "ele", BAD_CAST str);
  }

  if(point.time) {
    struct tm loctime;
    localtime_r(&point.time, &loctime);
    strftime(str, sizeof(str), DATE_FORMAT, &loctime);
    xmlNewTextChild(node_point, NULL, BAD_CAST "time", BAD_CAST str);
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
  std::vector<track_seg_t>::const_iterator it = track->segments.begin();
  std::vector<track_seg_t>::const_iterator itEnd = track->segments.end();
  if(doc) {
    xmlNodePtr cur_node;
    xmlNodePtr root_node = xmlDocGetRootElement(doc);
    bool err = false;
    if (!root_node || root_node->type != XML_ELEMENT_NODE ||
        strcasecmp((char*)root_node->name, "gpx") != 0 ) {
      err = true;
    } else {
      cur_node = root_node->children;
      while(cur_node && cur_node->type != XML_ELEMENT_NODE)
        cur_node = cur_node->next;
      if(!cur_node || !cur_node->children ||
         strcasecmp((char*)cur_node->name, "trk") != 0) {
        err = true;
      } else {
        trk_node = cur_node;
        /* assume that at most the last segment in the file was modified */
        for (cur_node = cur_node->children; cur_node->next; cur_node = cur_node->next) {
          // skip non-nodes, e.g. if the user inserted a comment
          if (cur_node->type != XML_ELEMENT_NODE)
            continue;
          // more tracks in the file than loaded, something is wrong
          if(it == itEnd) {
            err = true;
            break;
          }
          /* something else, this track is not written from osm2go */
          if(strcasecmp((char*)cur_node->name, "trkseg") != 0) {
            err = true;
            break;
          }
          it++;
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
    it = track->segments.begin();

    trk_node = xmlNewChild(root_node, NULL, BAD_CAST "trk", NULL);
    xmlDocSetRootElement(doc, root_node);
  }

  std::for_each(it, itEnd, track_save_segs(trk_node));

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
  std::string trk_name = project->path;
  const std::string::size_type plen = trk_name.size();
  trk_name += "backup.trk";
  if(g_file_test(trk_name.c_str(), G_FILE_TEST_EXISTS)) {
    printf("track backup present, loading it instead of real track ...\n");
  } else {
    trk_name.erase(plen, std::string::npos);
    trk_name += project->name;
    trk_name += ".trk";

    if(!g_file_test(trk_name.c_str(), G_FILE_TEST_EXISTS)) {
      printf("no track present!\n");
      return FALSE;
    }
    printf("track found, loading ...\n");
  }

  appdata->track.track = track_read(trk_name.c_str(), FALSE);

  track_menu_set(appdata);

  printf("restored track\n");

  return TRUE;
}

static void track_end_segment(track_t *track) {
  if(!track) return;

  if(track->active) {
    printf("ending a segment\n");
    track->active = false;

    /* todo: check if segment only has 1 point */
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
    track = appdata->track.track = new track_t();
  }

  track_menu_set(appdata);

  if(!track->active) {
    printf("starting new segment\n");

    track_seg_t seg;
    track->segments.push_back(seg);
    track->active = true;
  } else
    printf("appending to current segment\n");

  std::vector<track_point_t> &points = track->segments.back().track_points;

  /* don't append if point is the same as last time */
  gboolean ret;
  if(!points.empty() && points.back().pos.lat == pos->lat &&
                        points.back().pos.lon == pos->lon) {
    printf("same value as last point -> ignore\n");
    ret = FALSE;
  } else {
    ret = TRUE;
    track->dirty = TRUE;
    points.push_back(track_point_t(*pos, alt, time(NULL)));

    if(points.size() == 1) {
      /* the segment can now be drawn for the first time */
      printf("initial draw\n");
      g_assert(track->segments.back().item_chain.empty());
      map_track_draw_seg(appdata->map, track->segments.back());
    } else {
      /* the segment has to be updated */
      g_assert(!track->segments.back().item_chain.empty());
      map_track_update_seg(appdata->map, track->segments.back());
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
      appdata->track.track = new track_t();
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

track_point_t::track_point_t()
  : time(0)
  , altitude(NAN)
{
  pos.lat = NAN;
  pos.lon = NAN;
}

track_point_t::track_point_t(const pos_t &p, float alt, time_t t)
  : pos(p)
  , time(t)
  , altitude(alt)
{
}

TrackSax::TrackSax()
  : active_seg(false)
  , curPoint(0)
  , state(DocStart)
  , track(0)
  , points(0)
{
  memset(&handler, 0, sizeof(handler));
  handler.characters = cb_characters;
  handler.startElement = cb_startElement;
  handler.endElement = cb_endElement;

  tags["gpx"] = std::pair<State, State>(DocStart, TagGpx);
  tags["trk"] = std::pair<State, State>(TagGpx, TagTrk);
  tags["trkseg"] = std::pair<State, State>(TagTrk, TagTrkSeg);
  tags["trkpt"] = std::pair<State, State>(TagTrkSeg, TagTrkPt);
  tags["time"] = std::pair<State, State>(TagTrkPt, TagTime);
  tags["ele"] = std::pair<State, State>(TagTrkPt, TagEle);
}

bool TrackSax::parse(const char *filename)
{
  if (xmlSAXUserParseFile(&handler, this, filename) != 0)
    return false;

  return track && !track->segments.empty();
}

void TrackSax::characters(const char *ch, int len)
{
  std::string buf;

  switch(state) {
  case TagEle:
    buf.assign(ch, len);
    curPoint->altitude = g_ascii_strtod(buf.c_str(), 0);
    break;
  case TagTime: {
    buf.assign(ch, len);
    struct tm time = { 0 };
    time.tm_isdst = -1;
    if(strptime(buf.c_str(), DATE_FORMAT, &time) != 0)
      curPoint->time = mktime(&time);
    break;
  }
  default:
    for(int pos = 0; pos < len; pos++)
      if(!isspace(ch[pos])) {
        printf("unhandled character data: %*.*s state %i\n", len, len, ch, state);
        break;
      }
  }
}

void TrackSax::startElement(const xmlChar *name, const xmlChar **attrs)
{
  std::map<const char *, std::pair<State, State> >::const_iterator it =
          std::find_if(tags.begin(), tags.end(), tag_find(name));

  if(it == tags.end()) {
    fprintf(stderr, "found unhandled element %s\n", name);
    return;
  }

  if(state != it->second.first) {
    fprintf(stderr, "found element %s in state %i, but expected %i\n",
            name, state, it->second.first);
    return;
  }

  state = it->second.second;

  switch(state){
  case TagTrk:
    if(!track)
      track = new track_t();
    break;
  case TagTrkSeg:
    track->segments.push_back(track_seg_t());
    break;
  case TagTrkPt: {
    track->segments.back().track_points.push_back(track_point_t());
    points++;
    curPoint = &track->segments.back().track_points.back();
    for(unsigned int i = 0; attrs[i]; i += 2) {
      if(strcmp(reinterpret_cast<const char *>(attrs[i]), "lat") == 0)
        curPoint->pos.lat = g_ascii_strtod((gchar*)(attrs[i + 1]), NULL);
      else if(strcmp(reinterpret_cast<const char *>(attrs[i]), "lon") == 0)
        curPoint->pos.lon = g_ascii_strtod((gchar*)(attrs[i + 1]), NULL);
    }
  }
  default:
    break;
  }
}

void TrackSax::endElement(const xmlChar *name)
{
  std::map<const char *, std::pair<State, State> >::const_iterator it =
          std::find_if(tags.begin(), tags.end(), tag_find(name));

  g_assert(it != tags.end());
  g_assert(state == it->second.second);

  switch(state){
  case TagTrkSeg:
    // drop empty segments
    if(track->segments.back().track_points.empty())
     track->segments.pop_back();
    break;
  default:
    break;
  }
  state = it->second.first;
}

// vim:et:ts=8:sw=2:sts=2:ai
