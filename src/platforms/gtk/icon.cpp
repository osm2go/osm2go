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

#include "icon.h"

#include "misc.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <memory>
#include <string>
#include <sys/stat.h>
#include <unordered_map>

#include "osm2go_annotations.h"
#include <osm2go_cpp.h>
#include <osm2go_stl.h>

class icon_buffer : public icon_t {
public:
  class icon_buffer_item : public icon_item {
  public:
    explicit icon_buffer_item(Pixmap nbuf);

    std::unique_ptr<GdkPixbuf, g_object_deleter> buf;
    int use;

    inline Pixmap buffer() {
      return buf.get();
    }
  };

  ~icon_buffer();

  typedef std::unordered_map<std::string, icon_buffer_item *> BufferMap;
  BufferMap entries;
};

icon_buffer::icon_buffer_item::icon_buffer_item(Pixmap nbuf)
  : buf(nbuf)
  , use(nbuf ? 1 : 0)
{
}

static std::string
icon_file_exists(const std::string &file) {
#ifdef USE_SVG_ICONS
  const std::array<const char *, 4> icon_exts = { { ".svg",
#else
  const std::array<const char *, 3> icon_exts = { {
#endif
                              ".png", ".gif", ".jpg" } };

  // absolute filenames are not mangled
  if(file[0] == '/') {
    struct stat st;
    if(stat(file.c_str(), &st) == 0 && S_ISREG(st.st_mode))
      return file;
    else
      return std::string();
  }

  std::string iname = "icons/" + file + icon_exts[0];
  iname.erase(iname.size() - strlen(icon_exts[0]));

  for(unsigned int i = 0; i < icon_exts.size(); i++) {
    iname += icon_exts[i];
    const std::string &fullname = find_file(iname);

    if(!fullname.empty())
      return fullname;
    iname.erase(iname.size() - strlen(icon_exts[i]));
  }
  return std::string();
}

icon_t::icon_item *icon_t::load(const std::string &sname, int limit) {
  if(sname.empty())
    return nullptr;

  icon_buffer::BufferMap &entries = static_cast<icon_buffer *>(this)->entries;

  /* check if icon list already contains an icon of that name */
  const icon_buffer::BufferMap::iterator it = entries.find(sname);

  if(it != entries.end()) {
    it->second->use++;
    return it->second;
  }

  const std::string &fullname = icon_file_exists(sname);
  if(!fullname.empty()) {
    Pixmap pix = gdk_pixbuf_new_from_file_at_size(fullname.c_str(), limit, limit, nullptr);

    if(likely(pix)) {
      //    g_debug("Successfully loaded icon %s to %p", name, pix);
      icon_buffer::icon_buffer_item *ret = new icon_buffer::icon_buffer_item(pix);
      entries[sname] = ret;
      return ret;
    }
  }

  g_warning("Icon %s not found", sname.c_str());
  return nullptr;
}

GtkWidget *icon_t::widget_load(const std::string &name, int limit) {
  icon_item *pix = load(name, limit);
  if(!pix)
    return nullptr;

  return gtk_image_new_from_pixbuf(pix->buffer());
}

icon_t::Pixmap icon_t::icon_item::buffer()
{
  return static_cast<icon_buffer::icon_buffer_item *>(this)->buffer();
}

int icon_t::icon_item::maxDimension() const
{
  const Pixmap buf = const_cast<icon_t::icon_item *>(this)->buffer();
  return std::max(gdk_pixbuf_get_height(buf), gdk_pixbuf_get_width(buf));
}

static inline void icon_destroy_pair(std::pair<const std::string, icon_buffer::icon_buffer_item *> &pair) {
  delete pair.second;
}

struct find_icon_buf {
  const icon_t::icon_item * const buf;
  explicit find_icon_buf(const icon_t::icon_item *b) : buf(b) {}
  bool operator()(const std::pair<std::string, icon_t::icon_item *> &pair) {
    return pair.second == buf;
  }
};

void icon_t::icon_free(icon_item *buf) {
  //  g_debug("request to free icon %p", buf);

  /* check if icon list already contains an icon of that name */
  icon_buffer::BufferMap &entries = static_cast<icon_buffer *>(this)->entries;
  const icon_buffer::BufferMap::iterator itEnd = entries.end();
  icon_buffer::BufferMap::iterator it = std::find_if(entries.begin(), itEnd,
                                                    find_icon_buf(buf));
  if(unlikely(it == itEnd)) {
    g_warning("ERROR: icon to be freed not found");
  } else {
    it->second->use--;
    if(!it->second->use) {
      //  g_debug("freeing unused icon %s", it->first.c_str());

      delete it->second;
      entries.erase(it);
    }
  }
}

icon_buffer::~icon_buffer()
{
  std::for_each(entries.begin(), entries.end(),
                icon_destroy_pair);
}

icon_t &icon_t::instance()
{
  static icon_buffer icons;
  return icons;
}

icon_t::~icon_t()
{
}
