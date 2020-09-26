/*
 * SPDX-FileCopyrightText: 2008 Till Harbaum <till@harbaum.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "pos.h"
#include "project.h"

#include <ctime>
#include <vector>

struct canvas_item_t;

enum TrackVisibility {
  RecordOnly,   ///< record track, nothing drawn
  ShowPosition, ///< record track, show only current position
  DrawCurrent,  ///< only draw current segment
  DrawAll,      ///< draw everything
};

struct track_point_t {
  track_point_t();
  track_point_t(const pos_t &p, float alt = 0.0f, time_t t = 0);
  pos_t pos;               /* position in lat/lon format */
  time_t time;
  float altitude;
};

struct track_seg_t {
  std::vector<track_point_t> track_points;
  std::vector<canvas_item_t *> item_chain;
};

struct track_t {
  track_t();

  std::vector<track_seg_t> segments;
  mutable bool dirty;
  bool active; ///< if the last element in segments is currently written to

  void clear();
  void clear_current();

  static int gps_position_callback(void *context);
};

struct appdata_t;
struct project_t;
struct track_t;

/* used internally to save and restore the currently displayed track */
void track_save(project_t::ref project, const track_t *track);

/**
 * @brief restore the track of the current project
 * @param appdata global appdata object
 * @return if a track was loaded
 */
bool track_restore(appdata_t &appdata);

/* accessible via the menu */
void track_export(const track_t *track, const char *filename);
track_t *track_import(const char *filename);
/**
 * @brief set enable state of "track export" and "track clear" menu entries
 * @param appdata global appdata object
 *
 * The state will be set depending on appdata->track.track presence.
 */
void track_menu_set(appdata_t &appdata);

void track_enable_gps(appdata_t &appdata, bool enable);
