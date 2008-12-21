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

#ifndef SETTINGS_H
#define SETTINGS_H

/* define this for a vertical UI layout */
#undef PORTRAIT

/* these size defaults are used in the non-hildonized version only */
#ifndef PORTRAIT
#define DEFAULT_WIDTH 640 
#define DEFAULT_HEIGHT 480
#else
/* about the size of the openmoko screen */
#define DEFAULT_WIDTH 480 
#define DEFAULT_HEIGHT 620
#endif

/* default working directory */
#ifndef USE_HILDON
#define BASE_DIR ".osm2go/"
#else
#define BASE_DIR "/media/mmc2/osm2go/"
#endif

#define DEFAULT_STYLE "mapnik"

typedef struct settings_s {

  /* never changed */
  char *base_path;

  /* changed in project.c */
  char *project;

  /* changed in osm_api.c */
  char *server, *username, *password;

  /* changed in wms.c */
  struct wms_server_s *wms_server;

  /* changed in style.c */
  char *style;

  /* changed in main.c */
  gboolean no_icons;
  gboolean no_antialias;

} settings_t;

settings_t *settings_load(void);
void settings_save(settings_t *settings);
void settings_free(settings_t *settings);

#endif // SETTINGS_H
