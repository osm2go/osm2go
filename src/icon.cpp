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

static gchar*
icon_file_exists(const gchar *file) {
  const char *icon_exts[] = { ".gif", ".png", ".jpg", NULL };
  gint idx;

  for (idx = 0; icon_exts[idx]; idx++) {
    gchar *fullname = find_file("icons/", file, icon_exts[idx]);

    if(fullname)
      return fullname;
  }
  return NULL;
}

GdkPixbuf *icon_load(icon_t **icon, const char *name) {
  if(!name) return NULL;

  const std::string sname = name;

  if(*icon) {
    /* check if icon list already contains an icon of that name */
    const std::map<std::string, icon_item>::iterator it =
     (*icon)->entries.find(name);

    if(it != (*icon)->entries.end()) {
      it->second.use++;
      return it->second.buf;
    }
  }

  gchar *fullname = icon_file_exists(name);
  if(fullname) {
    GdkPixbuf *pix = gdk_pixbuf_new_from_file(fullname, NULL);
    g_free(fullname);

    if(!*icon)
      *icon = new icon_t();
    //    printf("Successfully loaded icon %s to %p\n", name, pix);
    (*icon)->entries[name] = pix;

      return pix;
  }

  printf("Icon %s not found\n", name);
  return NULL;
}

GtkWidget *icon_widget_load(icon_t **icon, const char *name) {
  GdkPixbuf *pix = icon_load(icon, name);
  if(!pix) return NULL;

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
  if(it == (*icon)->entries.end()) {
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

void icon_free_all(icon_t **icons) {
  if(!*icons)
    return;

  unsigned int cnt = (*icons)->entries.size();

  std::for_each((*icons)->entries.begin(), (*icons)->entries.end(),
                icon_destroy_pair);

  delete *icons;
  *icons = 0;

  printf("freed %d icons\n", cnt);
}
