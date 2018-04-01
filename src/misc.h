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

#ifndef MISC_H
#define MISC_H

#include "fdguard.h"

#include <glib.h>
#include <gtk/gtk.h>

enum {
  MISC_AGAIN_ID_DELETE           = (1<<0),
  MISC_AGAIN_ID_JOIN_NODES       = (1<<1),
  MISC_AGAIN_ID_JOIN_WAYS        = (1<<2),
  MISC_AGAIN_ID_OVERWRITE_TAGS   = (1<<3),
  MISC_AGAIN_ID_EXTEND_WAY       = (1<<4),
  MISC_AGAIN_ID_EXTEND_WAY_END   = (1<<5),
  MISC_AGAIN_ID_EXPORT_OVERWRITE = (1<<6),
  MISC_AGAIN_ID_AREA_TOO_BIG     = (1<<7),

  /* these flags prevent you from leaving the dialog with no (or yes) */
  /* if the "dont show me this dialog again" checkbox is selected. This */
  /* makes sure, that you can't permanently switch certain things in, but */
  /* only on. e.g. it doesn't make sense to answer a "do you really want to */
  /* delete this" dialog with "no and don't ask me again". You'd never be */
  /* able to delete anything again */
  MISC_AGAIN_FLAG_DONT_SAVE_NO   = (1<<30),
  MISC_AGAIN_FLAG_DONT_SAVE_YES  = (1<<31)
};

#include <osm2go_cpp.h>
#include <osm2go_platform.h>

#include <memory>
#include <string>
#include <vector>

#include <osm2go_stl.h>

struct datapath {
#if __cplusplus >= 201103L
  explicit inline datapath(fdguard &&f)  : fd(std::move(f)) {}
#else
  explicit inline datapath(fdguard &f)  : fd(f) {}
#endif
  fdguard fd;
  std::string pathname;
};

const std::vector<datapath> &base_paths();

std::string find_file(const std::string &n);

/* some compat code */

bool yes_no_f(osm2go_platform::Widget *parent, unsigned int again_flags,
              const char *title, const char *fmt, ...) __attribute__((format (printf, 4, 5)));

// simplified form of unique_ptr
struct g_deleter {
  inline void operator()(gpointer mem) {
    g_free(mem);
  }
};

typedef std::unique_ptr<gchar, g_deleter> g_string;

struct g_object_deleter {
  inline void operator()(gpointer obj) {
    g_object_unref(obj);
  }
};

#endif // MISC_H
