/*
 * SPDX-FileCopyrightText: 2008 Till Harbaum <till@harbaum.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "icon.h"
#include "osm2go_platform_gtk_icon.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <memory>
#include <string>
#include <sys/stat.h>
#include <unordered_map>

#include "osm2go_annotations.h"
#include <osm2go_cpp.h>
#include "osm2go_platform.h"
#include "osm2go_platform_gtk.h"
#include <osm2go_stl.h>

namespace {

class icon_buffer_item : public icon_item {
public:
  explicit icon_buffer_item(GdkPixbuf *nbuf);

  std::unique_ptr<GdkPixbuf, g_object_deleter> buf;
  int use;

  inline GdkPixbuf * __attribute__ ((warn_unused_result)) buffer() const {
    return buf.get();
  }
};

class icon_buffer : public gtk_platform_icon_t {
public:
  icon_buffer() {}
  icon_buffer(const icon_buffer &) O2G_DELETED_FUNCTION;
  icon_buffer &operator=(const icon_buffer &) O2G_DELETED_FUNCTION;
#if __cplusplus >= 201103L
  icon_buffer(icon_buffer &&) = delete;
  icon_buffer &operator=(icon_buffer &&) = delete;
#endif
  ~icon_buffer();

  typedef std::unordered_map<std::string, icon_buffer_item *> BufferMap;
  BufferMap entries;
};

icon_buffer_item::icon_buffer_item(GdkPixbuf *nbuf)
  : buf(nbuf)
  , use(nbuf != nullptr ? 1 : 0)
{
}

std::string
icon_file_exists(const std::string &file)
{
#ifdef USE_SVG_ICONS
  const std::array<const char *, 4> icon_exts = { { ".svg",
#else
  const std::array<const char *, 3> icon_exts = { {
#endif
                              ".png", ".gif", ".jpg" } };

  std::string ret;

  // absolute filenames are not mangled
  if(file[0] == '/') {
    if(likely(std::filesystem::is_regular_file(file)))
      ret = file;

    return ret;
  }

  std::string iname = "icons/" + file + icon_exts.front();
  std::string::size_type olen = strlen(icon_exts.front());
  std::string::size_type wpos = iname.length() - olen;

  for(unsigned int i = 0; i < icon_exts.size(); i++) {
    std::string::size_type nlen = strlen(icon_exts.at(i));
    iname.replace(wpos, olen, icon_exts[i], nlen);
    olen = nlen;
    ret = find_file(iname);

    if(!ret.empty())
      break;
  }

  return ret;
}

} // namespace

icon_item *icon_t::load(const std::string &sname, int limit)
{
  assert(!sname.empty());

  icon_buffer::BufferMap &entries = static_cast<icon_buffer *>(this)->entries;

  /* check if icon list already contains an icon of that name */
  const icon_buffer::BufferMap::iterator it = entries.find(sname);

  if(it != entries.end()) {
    it->second->use++;
    return it->second;
  }

  const std::string &fullname = icon_file_exists(sname);
  if(!fullname.empty()) {
    GdkPixbuf *pix = gdk_pixbuf_new_from_file_at_size(fullname.c_str(), limit, limit, nullptr);

    if(likely(pix)) {
      icon_buffer_item *ret = new icon_buffer_item(pix);
//       g_debug("Successfully loaded icon %s to %p (pixmap %p)", sname.c_str(), ret, pix);
      entries[sname] = ret;
      return ret;
    }
  }

  g_warning("Icon %s not found", sname.c_str());
  return nullptr;
}

GtkWidget *
gtk_platform_icon_t::widget_load(const std::string &name, int limit)
{
  icon_item *pix = load(name, limit);
  if(pix == nullptr)
    return nullptr;

  return gtk_image_new_from_pixbuf(static_cast<icon_buffer_item *>(pix)->buffer());
}

int icon_item::maxDimension() const
{
  const GdkPixbuf *buf = static_cast<const icon_buffer_item *>(this)->buffer();
  return std::max(gdk_pixbuf_get_height(buf), gdk_pixbuf_get_width(buf));
}

GdkPixbuf *osm2go_platform::icon_pixmap(const icon_item *icon)
{
  return static_cast<const icon_buffer_item *>(icon)->buffer();
}

namespace {

inline void
icon_destroy_pair(std::pair<const std::string, icon_buffer_item *> &pair)
{
  delete pair.second;
}

struct find_icon_buf {
  const icon_item * const buf;
  explicit find_icon_buf(const icon_item *b) : buf(b) {}
  inline bool operator()(const std::pair<std::string, icon_item *> &pair) const
  {
    return pair.second == buf;
  }
};

} // namespace

void icon_t::icon_free(icon_item *buf)
{
  //  g_debug("request to free icon %p", buf);

  icon_buffer::BufferMap &entries = static_cast<icon_buffer *>(this)->entries;
  const icon_buffer::BufferMap::iterator itEnd = entries.end();
  icon_buffer::BufferMap::iterator it = std::find_if(entries.begin(), itEnd,
                                                    find_icon_buf(buf));
  assert(it != itEnd);
  it->second->use--;
  if(it->second->use == 0) {
    //  g_debug("freeing unused icon %s", it->first.c_str());

    delete it->second;
    entries.erase(it);
  }
}

icon_buffer::~icon_buffer()
{
  std::for_each(entries.begin(), entries.end(),
                icon_destroy_pair);
}

icon_t &icon_t::instance()
{
  return gtk_platform_icon_t::instance();
}

gtk_platform_icon_t &gtk_platform_icon_t::instance()
{
  static icon_buffer icons;
  return icons;
}
