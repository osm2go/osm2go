/*
 * SPDX-FileCopyrightText: 2008 Till Harbaum <till@harbaum.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/* make sure strptime() is defined */
#ifndef __USE_XOPEN
#define _XOPEN_SOURCE 600
#endif

#include "track.h"

#include "appdata.h"
#include "fdguard.h"
#include "gps_state.h"
#include "map.h"
#include "misc.h"
#include "project.h"
#include "SaxParser.h"
#include "settings.h"
#include "uicontrol.h"

#include <cassert>
#include <cmath>
#include <cstring>
#include <ctime>
#include <fcntl.h>
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
#define DATE_FORMAT "%FT%T"

namespace {

class TrackSax : public SaxParser {
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
    explicit tag_find(const char *n) : name(n) {}
    inline bool operator()(const StateMap::value_type &p) const {
      return (strcmp(p.name, name) == 0);
    }
  };

public:
  TrackSax();

  bool parse(const char *filename);

  std::unique_ptr<track_t> track;
  std::vector<track_point_t>::size_type points;  ///< total points

private:
  void characters(const char *ch, int len) override;
  void startElement(const char *name, const char **attrs) override;
  void endElement(const char *name) override;
};

} // namespace

/* make menu represent the track state */
void track_menu_set(appdata_t &appdata) {
  if(unlikely(appdata_t::window == nullptr))
    return;

  bool present = static_cast<bool>(appdata.track.track);

  /* if a track is present, then it can be cleared or exported */
  appdata.uicontrol->setActionEnable(MainUi::MENU_ITEM_TRACK_CLEAR, present);
  appdata.uicontrol->setActionEnable(MainUi::MENU_ITEM_TRACK_EXPORT, present);
}

static track_t *track_read(const char *filename, bool dirty)
{
  TrackSax sx;
  if(unlikely(!sx.parse(filename))) {
    printf("track %s was empty/invalid track\n", filename);
    return nullptr;
  }

  sx.track->dirty = dirty;
  printf("Track %s is %sdirty, %zu points in %zu segments\n", filename, dirty ? "" : "not ",
    sx.points, sx.track->segments.size());

  return sx.track.release();
}

/* ----------------------  saving track --------------------------- */

namespace {

struct track_save_segs {
  xmlNode * const node;
  explicit track_save_segs(xmlNodePtr n) : node(n) {}

  struct save_point {
    xmlNode * const node;
    explicit save_point(xmlNodePtr n) : node(n) {}
    void operator()(const track_point_t &point) const;
  };

  inline void operator()(const track_seg_t &seg) const
  {
    xmlNodePtr node_seg = xmlNewChild(node, nullptr, BAD_CAST "trkseg", nullptr);
    std::for_each(seg.track_points.begin(), seg.track_points.end(),
                  save_point(node_seg));
  }
};

void track_save_segs::save_point::operator()(const track_point_t &point) const
{
  xmlNodePtr node_point = xmlNewChild(node, nullptr, BAD_CAST "trkpt", nullptr);

  point.pos.toXmlProperties(node_point);

  if(!std::isnan(point.altitude)) {
    char str[16]; // int needs at most 10 digits, '-', '.', '\0' -> 13
    format_float(point.altitude, 2, str);
    xmlNewTextChild(node_point, nullptr, BAD_CAST "ele", BAD_CAST str);
  }

  if(likely(point.time)) {
    struct tm loctime;
    char str[32];
    localtime_r(&point.time, &loctime);
    strftime(str, sizeof(str), DATE_FORMAT, &loctime);
    xmlNewTextChild(node_point, nullptr, BAD_CAST "time", BAD_CAST str);
  }
}

/**
 * @brief write the track information to a GPX file
 * @param name the filename to write to
 * @param track the track data to write
 * @param xdoc previous xml
 *
 * If xdoc is given, the last track in xdoc is updated and all remaining ones
 * are appended. If xdoc is nullptr all tracks are saved.
 *
 * xdoc is freed.
 */
void
track_write(const char *name, const track_t *track, xmlDoc *xdoc)
{
  printf("writing track to %s\n", name);

  xmlNodePtr trk_node;
  std::vector<track_seg_t>::const_iterator it = track->segments.begin();
  std::vector<track_seg_t>::const_iterator itEnd = track->segments.end();
  xmlDocGuard doc(xdoc);
  if(doc) {
    xmlNodePtr cur_node;
    xmlNodePtr root_node = xmlDocGetRootElement(doc.get());
    bool err = false;
    if (unlikely(!root_node || root_node->type != XML_ELEMENT_NODE ||
                   strcasecmp(reinterpret_cast<const char *>(root_node->name), "gpx") != 0)) {
      err = true;
    } else {
      cur_node = root_node->children;
      while(cur_node != nullptr && cur_node->type != XML_ELEMENT_NODE)
        cur_node = cur_node->next;
      if(unlikely(!cur_node || !cur_node->children ||
                    strcasecmp(reinterpret_cast<const char *>(cur_node->name), "trk") != 0)) {
        err = true;
      } else {
        trk_node = cur_node;
        /* assume that at most the last segment in the file was modified */
        for (cur_node = cur_node->children; cur_node->next != nullptr; cur_node = cur_node->next) {
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
    xmlNodePtr root_node = xmlNewNode(nullptr, BAD_CAST "gpx");
    xmlNewProp(root_node, BAD_CAST "xmlns",
               BAD_CAST "http://www.topografix.com/GPX/1/0");
    xmlNewProp(root_node, BAD_CAST "creator", BAD_CAST PACKAGE " v" VERSION);
    it = track->segments.begin();

    trk_node = xmlNewChild(root_node, nullptr, BAD_CAST "trk", nullptr);
    xmlDocSetRootElement(doc.get(), root_node);
  }

  std::for_each(it, itEnd, track_save_segs(trk_node));

  xmlSaveFormatFileEnc(name, doc.get(), "UTF-8", 1);
}

} // namespace

/* save track in project */
void track_save(project_t::ref project, const track_t *track)
{
  if(!project)
    return;

  /* no need to save again if it has already been saved */
  if(track != nullptr && !track->dirty) {
    printf("track is not dirty, no need to save it (again)\n");
    return;
  }

  const std::string trk_name = project->path + project->name + ".trk";
  const char *trkfname = trk_name.c_str() + project->path.size();

  if(track == nullptr) {
    unlinkat(project->dirfd, trkfname, 0);
    return;
  }

  /* check if there already is such a track file and make it a backup */
  /* in case saving new track fails */
  const char *backupfn = "backup.trk";
  xmlDocPtr doc = nullptr;

  struct stat st;
  if(fstatat(project->dirfd, trkfname, &st, 0) == 0 && S_ISREG(st.st_mode)) {
    printf("backing up existing file '%s' to '%s'\n", trkfname, backupfn);
    if(renameat(project->dirfd, trkfname, project->dirfd, backupfn) == 0) {
      /* parse the old file and get the DOM */
      fdguard bupfd(project->dirfd, backupfn, O_RDONLY);
      if(likely(bupfd.valid()))
        doc = xmlReadFd(bupfd, nullptr, nullptr, XML_PARSE_NONET);
    }
  }

  track_write(trk_name.c_str(), track, doc);
  track->dirty = false;

  /* if we reach this point writing the new file worked and we */
  /* can delete the backup */
  unlinkat(project->dirfd, backupfn, 0);
}

void track_export(const track_t *track, const char *filename) {
  track_write(filename, track, nullptr);
}

/* ----------------------  loading track --------------------------- */

bool track_restore(appdata_t &appdata) {
  project_t::ref project = appdata.project;

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
    appdata.track.track.reset(track_read(trk_name.c_str(), false));
    ret = static_cast<bool>(appdata.track.track);
  }

  track_menu_set(appdata);

  printf("restored track\n");

  return ret;
}

static void track_end_segment(std::unique_ptr<track_t> &track) {
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
  /* no track at all? might be due to a "clear track" while running */
  if(unlikely(!appdata.track.track)) {
    printf("restarting after \"clear\"\n");
    appdata.track.track.reset(new track_t());
  }
  track_t * const track = appdata.track.track.get();

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
  settings_t::ref settings = settings_t::instance();
  bool ret;
  if(unlikely(!points.empty() && points.back().pos == pos)) {
    printf("same value as last point -> ignore\n");
    ret = false;
  } else {
    ret = true;
    track->dirty = true;
    points.push_back(track_point_t(pos, alt, time(nullptr)));

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
      if(--appdata.track.warn_cnt == 0) {
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

  /* stopping the GPS removes the marker ... */
  appdata.map->remove_gps_position();

  /* ... and terminates the current segment if present */
  track_end_segment(appdata.track.track);
}

int track_t::gps_position_callback(void *context) {
  appdata_t &appdata = *static_cast<appdata_t *>(context);

  /* ignore updates while no valid osm file is loaded, e.g. when switching */
  /* projects */
  if(unlikely(!appdata.project || !appdata.project->osm))
    return 1;

  /* the map is only gone of the main screen is being closed */
  if(unlikely(!appdata.map)) {
    printf("map has gone while tracking was active, stopping tracker\n");

    appdata.gps_state->setEnable(false);

    return 0;
  }

  settings_t::ref settings = settings_t::instance();
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

  if(!appdata.track.track) {
    printf("GPS: no track yet, starting new one\n");
    appdata.track.track.reset(new track_t());
  } else
    printf("GPS: extending existing track\n");
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
  : SaxParser()
  , curPoint(nullptr)
  , state(DocStart)
  , track(nullptr)
  , points(0)
{
  tags.push_back(StateMap::value_type("gpx", DocStart, TagGpx));
  tags.push_back(StateMap::value_type("trk", TagGpx, TagTrk));
  tags.push_back(StateMap::value_type("trkseg", TagTrk, TagTrkSeg));
  tags.push_back(StateMap::value_type("trkpt", TagTrkSeg, TagTrkPt));
  tags.push_back(StateMap::value_type("time", TagTrkPt, TagTime));
  tags.push_back(StateMap::value_type("ele", TagTrkPt, TagEle));
}

bool TrackSax::parse(const char *filename)
{
  if(unlikely(!parseFile(filename)))
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
    if(likely(strptime(buf.c_str(), DATE_FORMAT, &time) != nullptr))
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

void TrackSax::startElement(const char *name, const char **attrs)
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
      track.reset(new track_t());
    break;
  case TagTrkSeg:
    track->segments.push_back(track_seg_t());
    break;
  case TagTrkPt:
    track->segments.back().track_points.push_back(track_point_t());
    points++;
    curPoint = &track->segments.back().track_points.back();
    for(unsigned int i = 0; attrs[i] != nullptr; i += 2) {
      if(strcmp(reinterpret_cast<const char *>(attrs[i]), "lat") == 0)
        curPoint->pos.lat = osm2go_platform::string_to_double(attrs[i + 1]);
      else if(likely(strcmp(reinterpret_cast<const char *>(attrs[i]), "lon") == 0))
        curPoint->pos.lon = osm2go_platform::string_to_double(attrs[i + 1]);
    }
    break;
  default:
    break;
  }
}

void TrackSax::endElement(const char *name)
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
