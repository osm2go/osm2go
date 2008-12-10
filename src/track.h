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

#ifndef TRACK_H
#define TRACK_H

typedef enum { TRACK_NONE = 0, TRACK_IMPORT, TRACK_GPS } track_mode_t;

typedef struct track_point_s {
  lpos_t lpos;               /* position in screen/map format */
  struct track_point_s *next;
} track_point_t;

typedef struct track_seg_s {
  track_point_t *track_point;
  struct track_seg_s *next;
  canvas_item_t *item;
} track_seg_t;

typedef struct track_s {
  char *filename;
  track_seg_t *track_seg;
  track_mode_t mode;
  gboolean saved;
  track_seg_t *cur_seg;
} track_t;

void track_do(appdata_t *appdata, track_mode_t mode, char *name);

gint track_seg_points(track_seg_t *seg);

void track_save(project_t *project, track_t *track);
track_t *track_restore(appdata_t *appdata, project_t *project);

#endif // TRACK_H
