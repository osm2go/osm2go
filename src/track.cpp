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

/* make sure strptime() is defined */
#ifndef __USE_XOPEN
#define _XOPEN_SOURCE 600
#endif

#include "track.h"

#include "appdata.h"
#include "fdguard.h"
#include "gps.h"
#include "map.h"
#include "project.h"
#include "settings.h"
#include "uicontrol.h"
#include "xml_helpers.h"

#include <cassert>
#include <cmath>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <map>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef LIBXML_TREE_ENABLED
#error "Tree not enabled in libxml"
#endif

#include <algorithm>

#include "osm2go_annotations.h"
#include <osm2go_cpp.h>
#include <osm2go_i18n.h>
#include <osm2go_stl.h>

/* format string used to altitude and time */
#define ALT_FORMAT  "%.02f"
#define DATE_FORMAT "%FT%T"

class TrackSax {
  xmlSAXHandler handler;
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

  struct StateChange {
    StateChange(const char *nm, State os, State ns)
      : name(nm), oldState(os), newState(ns) {}
    const char *name;
    State oldState;
    State newState;
  };

  typedef std::vector<StateChange> StateMap;
  StateMap tags;

  // custom find to avoid memory allocations for std::string
  struct tag_find {
    const char * const name;
    explicit tag_find(const xmlChar *n) : name(reinterpret_cast<const char *>(n)) {}
    bool operator()(const StateMap::value_type &p) {
      return (strcmp(p.name, name) == 0);
    }
  };

public:
  TrackSax();

  bool parse(const char *filename);

  track_t *track;
  std::vector<track_point_t>::size_type points;  ///< total points

private:
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
void track_menu_set(appdata_t &appdata) {
  if(unlikely(appdata_t::window == O2G_NULLPTR))
    return;

  bool present = (appdata.track.track != O2G_NULLPTR);

  /* if a track is present, then it can be cleared or exported */
  appdata.uicontrol->setActionEnable(MainUi::MENU_ITEM_TRACK_CLEAR, present);
  appdata.uicontrol->setActionEnable(MainUi::MENU_ITEM_TRACK_EXPORT, present);
}

static track_t *track_read(const char *filename, bool dirty) {
  printf("============================================================\n");
  printf("loading track %s\n", filename);

  TrackSax sx;
  if(unlikely(!sx.parse(filename))) {
    delete sx.track;
    printf("track was empty/invalid track\n");
    return O2G_NULLPTR;
  }

  sx.track->dirty = dirty;
  printf("Track is %sdirty.\n", dirty?"":"not ");
  printf("%zu points in %zu segments\n", sx.points, sx.track->segments.size());

  return sx.track;
}

/* ----------------------  saving track --------------------------- */

struct track_save_segs {
  xmlNodePtr const node;
  explicit track_save_segs(xmlNodePtr n) : node(n) {}

  struct save_point {
    xmlNodePtr const node;
    explicit save_point(xmlNodePtr n) : node(n) {}
    void operator()(const track_point_t &point);
  };

  void operator()(const track_seg_t &seg) {
    xmlNodePtr node_seg = xmlNewChild(node, O2G_NULLPTR, BAD_CAST "trkseg", O2G_NULLPTR);
    std::for_each(seg.track_points.begin(), seg.track_points.end(),
                  save_point(node_seg));
  }
};

void track_save_segs::save_point::operator()(const track_point_t &point)
{
  char str[G_ASCII_DTOSTR_BUF_SIZE];

  xmlNodePtr node_point = xmlNewChild(node, O2G_NULLPTR, BAD_CAST "trkpt", O2G_NULLPTR);

  point.pos.toXmlProperties(node_point);

  if(!std::isnan(point.altitude)) {
    g_ascii_formatd(str, sizeof(str), ALT_FORMAT, point.altitude);
    xmlNewTextChild(node_point, O2G_NULLPTR, BAD_CAST "ele", BAD_CAST str);
  }

  if(likely(point.time)) {
    struct tm loctime;
    localtime_r(&point.time, &loctime);
    strftime(str, sizeof(str), DATE_FORMAT, &loctime);
    xmlNewTextChild(node_point, O2G_NULLPTR, BAD_CAST "time", BAD_CAST str);
  }
}

/**
 * @brief write the track information to a GPX file
 * @param name the filename to write to
 * @param track the track data to write
 * @param doc previous xml
 *
 * If doc is given, the last track in doc is updated and all remaining ones
 * are appended. If doc is O2G_NULLPTR all tracks are saved.
 *
 * doc is freed.
 */
static void track_write(const char *name, const track_t *track, xmlDoc *d) {
  printf("writing track to %s\n", name);

  xmlNodePtr trk_node;
  std::vector<track_seg_t>::const_iterator it = track->segments.begin();
  std::vector<track_seg_t>::const_iterator itEnd = track->segments.end();
  std::unique_ptr<xmlDoc, xmlDocDelete> doc(d);
  if(doc) {
    xmlNodePtr cur_node;
    xmlNodePtr root_node = xmlDocGetRootElement(doc.get());
    bool err = false;
    if (unlikely(!root_node || root_node->type != XML_ELEMENT_NODE ||
                   strcasecmp(reinterpret_cast<const char *>(root_node->name), "gpx") != 0)) {
      err = true;
    } else {
      cur_node = root_node->children;
      while(cur_node && cur_node->type != XML_ELEMENT_NODE)
        cur_node = cur_node->next;
      if(unlikely(!cur_node || !cur_node->children ||
                    strcasecmp(reinterpret_cast<const char *>(cur_node->name), "trk") != 0)) {
        err = true;
      } else {
        trk_node = cur_node;
        /* assume that at most the last segment in the file was modified */
        for (cur_node = cur_node->children; cur_node->next; cur_node = cur_node->next) {
          // skip non-nodes, e.g. if the user inserted a comment
          if (cur_node->type != XML_ELEMENT_NODE)
            continue;
          // more tracks in the file than loaded, something is wrong
          if(unlikely(it == itEnd)) {
            err = true;
            break;
          }
          /* something else, this track is not written from osm2go */
          if(unlikely(strcasecmp(reinterpret_cast<const char *>(cur_node->name), "trkseg") != 0)) {
            err = true;
            break;
          }
          it++;
        }
      }
    }
    if(unlikely(err)) {
      doc.reset();
    } else {
      /* drop the last entry from XML, it will be rewritten */
      xmlUnlinkNode(cur_node);
      xmlFreeNode(cur_node);
    }
  }

  if (!doc) {
    doc.reset(xmlNewDoc(BAD_CAST "1.0"));
    xmlNodePtr root_node = xmlNewNode(O2G_NULLPTR, BAD_CAST "gpx");
    xmlNewProp(root_node, BAD_CAST "xmlns",
               BAD_CAST "http://www.topografix.com/GPX/1/0");
    xmlNewProp(root_node, BAD_CAST "creator", BAD_CAST PACKAGE " v" VERSION);
    it = track->segments.begin();

    trk_node = xmlNewChild(root_node, O2G_NULLPTR, BAD_CAST "trk", O2G_NULLPTR);
    xmlDocSetRootElement(doc.get(), root_node);
  }

  std::for_each(it, itEnd, track_save_segs(trk_node));

  xmlSaveFormatFileEnc(name, doc.get(), "UTF-8", 1);
}

/* save track in project */
void track_save(project_t *project, track_t *track) {
  if(!project) return;

  /* no need to save again if it has already been saved */
  if(track && !track->dirty) {
    printf("track is not dirty, no need to save it (again)\n");
    return;
  }

  const std::string trkfname = project->name + ".trk";

  if(!track) {
    unlinkat(project->dirfd, trkfname.c_str(), 0);
    return;
  }

  /* check if there already is such a diff file and make it a backup */
  /* in case new diff saving fails */
  const char *backupfn = "backup.trk";
  xmlDocPtr doc = O2G_NULLPTR;

  struct stat st;
  if(fstatat(project->dirfd, trkfname.c_str(), &st, 0) == 0 && S_ISREG(st.st_mode)) {
    printf("backing up existing file '%s' to '%s'\n", trkfname.c_str(), backupfn);
    if(renameat(project->dirfd, trkfname.c_str(), project->dirfd, backupfn) == 0) {
      /* parse the old file and get the DOM */
      fdguard bupfd(project->dirfd, backupfn, O_RDONLY);
      if(likely(bupfd.valid()))
        doc = xmlReadFd(bupfd, O2G_NULLPTR, O2G_NULLPTR, XML_PARSE_NONET);
    }
  }

  const std::string trk_name = project->path + trkfname;
  track_write(trk_name.c_str(), track, doc);
  track->dirty = false;

  /* if we reach this point writing the new file worked and we */
  /* can delete the backup */
  unlinkat(project->dirfd, backupfn, 0);
}

void track_export(const track_t *track, const char *filename) {
  track_write(filename, track, O2G_NULLPTR);
}

/* ----------------------  loading track --------------------------- */

bool track_restore(appdata_t &appdata) {
  const project_t *project = appdata.project;

  /* first try to open a backup which is only present if saving the */
  /* actual diff didn't succeed */
  const char *backupfn = "backup.trk";
  std::string trk_name;

  bool ret = true;
  struct stat st;
  if(unlikely(fstatat(project->dirfd, backupfn, &st, 0) == 0 && S_ISREG(st.st_mode))) {
    printf("track backup present, loading it instead of real track ...\n");
    trk_name = project->path + backupfn;
  } else {
    // allocate in one go
    trk_name = project->path + project->name + ".trk";

    // use relative filename to test
    if(fstatat(project->dirfd, trk_name.c_str() + project->path.size(), &st, 0) != 0 || !S_ISREG(st.st_mode)) {
      printf("no track present!\n");
      ret = false;
    } else
      printf("track found, loading ...\n");
  }

  if (ret) {
    appdata.track.track = track_read(trk_name.c_str(), false);
    ret = appdata.track.track != O2G_NULLPTR;
  }

  track_menu_set(appdata);

  printf("restored track\n");

  return ret;
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
 * @retval false if the GPS position marker needs to be redrawn (i.e. the position changed)
 */
static bool track_append_position(appdata_t &appdata, const pos_t &pos, float alt, const lpos_t lpos) {
  track_t *track = appdata.track.track;

  /* no track at all? might be due to a "clear track" while running */
  if(unlikely(!track)) {
    printf("restarting after \"clear\"\n");
    track = appdata.track.track = new track_t();
  }

  track_menu_set(appdata);

  if(unlikely(!track->active)) {
    printf("starting new segment\n");

    track_seg_t seg;
    track->segments.push_back(seg);
    track->active = true;
  } else
    printf("appending to current segment\n");

  track_seg_t &seg = track->segments.back();
  std::vector<track_point_t> &points = seg.track_points;

  /* don't append if point is the same as last time */
  const settings_t * const settings = settings_t::instance();
  bool ret;
  if(unlikely(!points.empty() && points.back().pos == pos)) {
    printf("same value as last point -> ignore\n");
    ret = false;
  } else {
    ret = true;
    track->dirty = true;
    points.push_back(track_point_t(pos, alt, time(O2G_NULLPTR)));

    if(settings->trackVisibility >= DrawCurrent) {
      if(seg.item_chain.empty()) {
        /* the segment can now be drawn for the first time */
        printf("initial draw\n");
        appdata.map->track_draw_seg(seg);
      } else {
        /* the segment has to be updated */
        appdata.map->track_update_seg(seg);
      }
    }
  }

  if(settings->follow_gps) {
    if(!appdata.map->scroll_to_if_offscreen(lpos)) {
      if(!--appdata.track.warn_cnt) {
        /* warn user once a minute that the current gps */
        /* position is outside the working area */
        appdata.uicontrol->showNotification(_("GPS position outside working area!"), MainUi::Brief);
        appdata.track.warn_cnt = 60;  // warn again after one minute
      }
    } else
      ret = true;
  }

  return ret;
}

static void track_do_disable_gps(appdata_t &appdata) {
  settings_t::instance()->enable_gps = false;
  appdata.gps_state->setEnable(false);

  appdata.gps_state->registerCallback(O2G_NULLPTR, O2G_NULLPTR);

  /* stopping the GPS removes the marker ... */
  appdata.map->remove_gps_position();

  /* ... and terminates the current segment if present */
  track_end_segment(appdata.track.track);
}

static int update(void *data) {
  appdata_t &appdata = *static_cast<appdata_t *>(data);

  /* ignore updates while no valid osm file is loaded, e.g. when switching */
  /* projects */
  if(unlikely(appdata.project == O2G_NULLPTR || appdata.project->osm == O2G_NULLPTR))
    return 1;

  /* the map is only gone of the main screen is being closed */
  if(unlikely(!appdata.map)) {
    printf("map has gone while tracking was active, stopping tracker\n");

    appdata.gps_state->registerCallback(O2G_NULLPTR, O2G_NULLPTR);

    return 0;
  }

  const settings_t * const settings = settings_t::instance();
  if(!settings->enable_gps) {
    // Turn myself off gracefully.
    track_do_disable_gps(appdata);
    return 0;
  }

  float alt;
  pos_t pos = appdata.gps_state->get_pos(&alt);
  if(pos.valid()) {
    printf("valid position %.6f/%.6f alt %.2f\n", pos.lat, pos.lon, alt);
    lpos_t lpos;
    lpos = pos.toLpos(appdata.project->osm->bounds);
    if(track_append_position(appdata, pos, alt, lpos) && settings->trackVisibility >= ShowPosition)
      appdata.map->track_pos(lpos);
  } else {
    printf("no valid position\n");
    /* end segment */
    track_end_segment(appdata.track.track);
    appdata.map->remove_gps_position();
  }

  return 1;
}

static void track_do_enable_gps(appdata_t &appdata) {
  settings_t::instance()->enable_gps = true;
  appdata.gps_state->setEnable(true);
  appdata.track.warn_cnt = 1;

  if (!appdata.gps_state->registerCallback(update, &appdata)) {
    if(!appdata.track.track) {
      printf("GPS: no track yet, starting new one\n");
      appdata.track.track = new track_t();
    } else
      printf("GPS: extending existing track\n");
  }
}

void track_enable_gps(appdata_t &appdata, bool enable) {
  printf("request to %sable gps\n", enable?"en":"dis");

  appdata.uicontrol->setActionEnable(MainUi::MENU_ITEM_TRACK_FOLLOW_GPS, enable);

  if(enable) track_do_enable_gps(appdata);
  else       track_do_disable_gps(appdata);
}

track_t *track_import(const char *filename) {
  printf("import %s\n", filename);

  return track_read(filename, true);
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
  : curPoint(O2G_NULLPTR)
  , state(DocStart)
  , track(O2G_NULLPTR)
  , points(0)
{
  memset(&handler, 0, sizeof(handler));
  handler.characters = cb_characters;
  handler.startElement = cb_startElement;
  handler.endElement = cb_endElement;

  tags.push_back(StateMap::value_type("gpx", DocStart, TagGpx));
  tags.push_back(StateMap::value_type("trk", TagGpx, TagTrk));
  tags.push_back(StateMap::value_type("trkseg", TagTrk, TagTrkSeg));
  tags.push_back(StateMap::value_type("trkpt", TagTrkSeg, TagTrkPt));
  tags.push_back(StateMap::value_type("time", TagTrkPt, TagTime));
  tags.push_back(StateMap::value_type("ele", TagTrkPt, TagEle));
}

bool TrackSax::parse(const char *filename)
{
  if(unlikely(xmlSAXUserParseFile(&handler, this, filename) != 0))
    return false;

  return track && !track->segments.empty();
}

void TrackSax::characters(const char *ch, int len)
{
  std::string buf;

  switch(state) {
  case TagEle:
    buf.assign(ch, len);
    curPoint->altitude = xml_parse_float(reinterpret_cast<const xmlChar *>(buf.c_str()));
    break;
  case TagTime: {
    buf.assign(ch, len);
    struct tm time;
    memset(&time, 0, sizeof(time));
    time.tm_isdst = -1;
    if(likely(strptime(buf.c_str(), DATE_FORMAT, &time) != O2G_NULLPTR))
      curPoint->time = mktime(&time);
    break;
  }
  default:
    for(int pos = 0; pos < len; pos++)
      if(unlikely(!isspace(ch[pos]))) {
        printf("unhandled character data: %*.*s state %i\n", len, len, ch, state);
        break;
      }
  }
}

void TrackSax::startElement(const xmlChar *name, const xmlChar **attrs)
{
  StateMap::const_iterator it = std::find_if(tags.begin(), tags.end(), tag_find(name));

  if(unlikely(it == tags.end())) {
    fprintf(stderr, "found unhandled element %s\n", name);
    return;
  }

  if(unlikely(state != it->oldState)) {
    fprintf(stderr, "found element %s in state %i, but expected %i\n",
            name, state, it->oldState);
    return;
  }

  state = it->newState;

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
        curPoint->pos.lat = xml_parse_float(attrs[i + 1]);
      else if(likely(strcmp(reinterpret_cast<const char *>(attrs[i]), "lon") == 0))
        curPoint->pos.lon = xml_parse_float(attrs[i + 1]);
    }
  }
  default:
    break;
  }
}

void TrackSax::endElement(const xmlChar *name)
{
  StateMap::const_iterator it = std::find_if(tags.begin(), tags.end(), tag_find(name));

  assert(it != tags.end());
  assert(state == it->newState);

  switch(state){
  case TagTrkSeg: {
    // drop empty segments
    std::vector<track_point_t> &last = track->segments.back().track_points;
    if(unlikely(last.empty())) {
      track->segments.pop_back();
    } else {
      // this vector will never be appended to again, so shrink it to the size
      // that is actually needed
      shrink_to_fit(last);
    }
    break;
  }
  default:
    break;
  }
  state = it->oldState;
}

track_t::track_t()
  : dirty(false)
  , active(false)
{
}

// vim:et:ts=8:sw=2:sts=2:ai
