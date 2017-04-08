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

#include "appdata.h"
#include "misc.h"

#include <algorithm>
#include <map>
#include <string>
#include <sys/stat.h>

struct icon_item {
  icon_item();
  icon_item(GdkPixbuf *nbuf);

  GdkPixbuf *buf;
  int use;
};

struct icon_t {
  ~icon_t();

  std::map<std::string, icon_item> entries;
};

icon_item::icon_item()
  : buf(0)
  , use(0)
{
}

icon_item::icon_item(GdkPixbuf *nbuf)
  : buf(nbuf)
  , use(1)
{
}

static std::string
icon_file_exists(const std::string &file) {
  const char *icon_exts[] = {
#ifdef USE_SVG_ICONS
                              ".svg",
#endif
                              ".gif", ".png", ".jpg", NULL };

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

GdkPixbuf *icon_load(icon_t **icon, const char *name) {
  if(!name || !*name)
    return 0;

  return icon_load(icon, std::string(name));
}

GdkPixbuf *icon_load(icon_t **icon, const std::string &sname, int limit) {
  if(sname.empty())
    return 0;

  if(*icon) {
    /* check if icon list already contains an icon of that name */
    const std::map<std::string, icon_item>::iterator it =
     (*icon)->entries.find(sname);

    if(it != (*icon)->entries.end()) {
      it->second.use++;
      return it->second.buf;
    }
  }

  const std::string &fullname = icon_file_exists(sname);
  if(!fullname.empty()) {
    GdkPixbuf *pix = gdk_pixbuf_new_from_file_at_size(fullname.c_str(), limit, limit, NULL);

    if(!*icon)
      *icon = new icon_t();
    //    printf("Successfully loaded icon %s to %p\n", name, pix);
    (*icon)->entries[sname] = pix;

      return pix;
  }

  printf("Icon %s not found\n", sname.c_str());
  return NULL;
}

GtkWidget *icon_widget_load(icon_t **icon, const char *name) {
  GdkPixbuf *pix = icon_load(icon, name);
  if(!pix) return NULL;

  return gtk_image_new_from_pixbuf(pix);
}

GtkWidget *icon_widget_load(icon_t **icon, const std::string &name, int limit) {
  GdkPixbuf *pix = icon_load(icon, name, limit);
  if(!pix)
    return NULL;

  return gtk_image_new_from_pixbuf(pix);
}

static void icon_destroy(icon_item &icon) {
  if(icon.buf)
    g_object_unref(icon.buf);
}

static void icon_destroy_pair(std::pair<const std::string, icon_item> &pair) {
  icon_destroy(pair.second);
}

struct find_icon_buf {
  const GdkPixbuf * const buf;
  find_icon_buf(const GdkPixbuf *b) : buf(b) {}
  bool operator()(const std::pair<std::string, icon_item> &pair) {
    return pair.second.buf == buf;
  }
};

void icon_free(icon_t **icon, GdkPixbuf *buf) {
  //  printf("request to free icon %p\n", buf);

  /* check if icon list already contains an icon of that name */
  std::map<std::string, icon_item>::iterator it = std::find_if(
                                                  (*icon)->entries.begin(),
                                                  (*icon)->entries.end(),
                                                  find_icon_buf(buf));
  if(G_UNLIKELY(it == (*icon)->entries.end())) {
    printf("ERROR: icon to be freed not found\n");
  } else {
    it->second.use--;
    if(!it->second.use) {
      //  printf("freeing unused icon %s\n", it->first.c_str());

      icon_destroy(it->second);
      (*icon)->entries.erase(it);
    }
  }
}

icon_t::~icon_t()
{
  std::for_each(entries.begin(), entries.end(),
                icon_destroy_pair);
}

void icon_free_all(icon_t *icons) {
  delete icons;
}
