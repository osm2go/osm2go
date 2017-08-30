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

#include "icon.h"

#include "misc.h"

#include <algorithm>
#include <cstring>
#include <map>
#include <string>
#include <sys/stat.h>

#include <osm2go_cpp.h>

icon_t::icon_item::icon_item(GdkPixbuf *nbuf)
  : buf(nbuf)
  , use(nbuf ? 1 : 0)
{
}

static std::string
icon_file_exists(const std::string &file) {
  const char *icon_exts[] = {
#ifdef USE_SVG_ICONS
                              ".svg",
#endif
                              ".gif", ".png", ".jpg", O2G_NULLPTR };

  // absolute filenames are not mangled
  if(file[0] == '/') {
    if(g_file_test(file.c_str(), G_FILE_TEST_IS_REGULAR))
      return file;
    else
      return std::string();
  }

  std::string iname = "icons/" + file + icon_exts[0];
  iname.erase(iname.size() - strlen(icon_exts[0]));

  for(const char **ic = icon_exts; *ic; ic++) {
    iname += *ic;
    const std::string &fullname = find_file(iname);

    if(!fullname.empty())
      return fullname;
    iname.erase(iname.size() - strlen(*ic));
  }
  return std::string();
}

GdkPixbuf *icon_t::load(const std::string &sname, int limit) {
  if(sname.empty())
    return O2G_NULLPTR;

  /* check if icon list already contains an icon of that name */
  const std::map<std::string, icon_item>::iterator it = entries.find(sname);

  if(it != entries.end()) {
    it->second.use++;
    return it->second.buf;
  }

  const std::string &fullname = icon_file_exists(sname);
  if(!fullname.empty()) {
    GdkPixbuf *pix = gdk_pixbuf_new_from_file_at_size(fullname.c_str(), limit, limit, O2G_NULLPTR);

    if(G_LIKELY(pix)) {
      //    printf("Successfully loaded icon %s to %p\n", name, pix);
      entries[sname] = icon_item(pix);
    }

    return pix;
  }

  printf("Icon %s not found\n", sname.c_str());
  return O2G_NULLPTR;
}

GtkWidget *icon_t::widget_load(const std::string &name, int limit) {
  GdkPixbuf *pix = load(name, limit);
  if(!pix)
    return O2G_NULLPTR;

  return gtk_image_new_from_pixbuf(pix);
}

void icon_t::icon_item::destroy()
{
  if(buf)
    g_object_unref(buf);
}

static void icon_destroy_pair(std::pair<const std::string, icon_t::icon_item> &pair) {
  pair.second.destroy();
}

struct find_icon_buf {
  const GdkPixbuf * const buf;
  find_icon_buf(const GdkPixbuf *b) : buf(b) {}
  bool operator()(const std::pair<std::string, icon_t::icon_item> &pair) {
    return pair.second == buf;
  }
};

void icon_t::icon_free(GdkPixbuf *buf) {
  //  printf("request to free icon %p\n", buf);

  /* check if icon list already contains an icon of that name */
  const std::map<std::string, icon_item>::iterator itEnd = entries.end();
  std::map<std::string, icon_item>::iterator it = std::find_if(
                                                  entries.begin(), itEnd,
                                                  find_icon_buf(buf));
  if(G_UNLIKELY(it == itEnd)) {
    printf("ERROR: icon to be freed not found\n");
  } else {
    it->second.use--;
    if(!it->second.use) {
      //  printf("freeing unused icon %s\n", it->first.c_str());

      it->second.destroy();
      entries.erase(it);
    }
  }
}

icon_t::~icon_t()
{
  std::for_each(entries.begin(), entries.end(),
                icon_destroy_pair);
}
