/*
 * SPDX-FileCopyrightText: 2020 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "icon.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <memory>
#include <QDebug>
#include <QLabel>
#include <QPainter>
#include <QString>
#include <QStringBuilder>
#include <QSvgRenderer>
#include <string>
#include <sys/stat.h>
#include <unordered_map>

#include "osm2go_annotations.h"
#include <osm2go_cpp.h>
#include "osm2go_platform.h"
#include "osm2go_platform_qt.h"

namespace {

class icon_buffer_item : public icon_item {
public:
  icon_buffer_item() = delete;
  icon_buffer_item(const icon_buffer_item &) = delete;
  icon_buffer_item(icon_buffer_item &&) = delete;
  inline icon_buffer_item(const QPixmap &p, std::unique_ptr<QSvgRenderer> &&r)
    : icon_item(), buf(p), renderer(std::move(r))
  {
    assert(!!buf);
  }
  icon_buffer_item &operator=(const icon_buffer_item &) = delete;
  icon_buffer_item &operator=(icon_buffer_item &&) = delete;
  ~icon_buffer_item() = default;

  QPixmap buf;
  const std::unique_ptr<QSvgRenderer> renderer;

  int use = 1;
};

class icon_buffer : public icon_t {
public:
  icon_buffer() = default;
  icon_buffer(const icon_buffer &) = delete;
  icon_buffer(icon_buffer &&) = delete;
  icon_buffer &operator=(const icon_buffer &) = delete;
  icon_buffer &operator=(icon_buffer &&) = delete;
  ~icon_buffer() = default;

  typedef std::unordered_map<std::string, std::unique_ptr<icon_buffer_item>> BufferMap;
  BufferMap entries;
};

QString
icon_file_exists(const std::string &file)
{
  const std::array<QString, 4> icon_exts = { { QStringLiteral(".svg"), QStringLiteral(".png"),
                                               QStringLiteral(".gif"), QStringLiteral(".jpg") } };
  QString ret;

  // absolute filenames are not mangled
  if(unlikely(file[0] == '/')) {
    if(likely(std::filesystem::is_regular_file(file)))
      ret = QString::fromStdString(file);

    return ret;
  }

  const int wpos = QStringLiteral("icons/").length() + file.size();
  QString iname = QStringLiteral("icons/") % QString::fromStdString(file) % icon_exts[0];

  for (auto &&ext : icon_exts) {
    const int longEnough = 32; // must be larger than any entry in icon_exts
    iname.replace(wpos, longEnough, ext);
    ret = osm2go_platform::find_file(iname);

    if (!ret.isEmpty())
      break;
  }

  return ret;
}

} // namespace

icon_item *
icon_t::load(const std::string &sname, int limit)
{
  assert(!sname.empty());

  icon_buffer::BufferMap &entries = static_cast<icon_buffer *>(this)->entries;

  /* check if icon list already contains an icon of that name */
  const auto it = entries.find(sname);

  if(it != entries.end()) {
    it->second->use++;
    return it->second.get();
  }

  const QString fullname = icon_file_exists(sname);
  if(!fullname.isEmpty()) {
    QPixmap pix;
    if(pix.load(fullname)) {
      std::unique_ptr<QSvgRenderer> rnd;
      if(fullname.endsWith(QLatin1String(".svg"))) {
        rnd = std::make_unique<QSvgRenderer>(fullname);
        if(!rnd->isValid())
          rnd.reset();
      }

      if(limit > 0)
        pix = pix.scaledToWidth(limit);

      qDebug() << "Successfully loaded icon" << fullname << "to" << pix << rnd.get() << limit;

      icon_buffer_item *ret = new icon_buffer_item(pix, std::move(rnd));

      entries[sname] = std::unique_ptr<icon_buffer_item>(ret);
      return ret;
    }
  }

  qDebug() << "Icon not found:" << QString::fromStdString(sname);
  return nullptr;
}

int icon_item::maxDimension() const
{
  const auto bi = static_cast<const icon_buffer_item *>(this);
  if (bi->renderer == nullptr) {
    const QPixmap &buf = bi->buf;
    return std::max(buf.height(), buf.width());
  } else {
    auto r = bi->renderer->viewBox();
    return std::max(r.height(), r.width());
  }
}

void
icon_t::icon_free(icon_item *buf)
{
  // check if icon list already contains an icon of that name
  icon_buffer::BufferMap &entries = static_cast<icon_buffer *>(this)->entries;
  const auto itEnd = entries.end();
  auto it = std::find_if(entries.begin(), itEnd, [buf](auto &&p) {
    return p.second.get() == buf;
  });

  assert(it != itEnd);

  it->second->use--;
  if(it->second->use == 0)
    entries.erase(it);
}

icon_t &icon_t::instance()
{
  static icon_buffer icons;
  return icons;
}

QPixmap
osm2go_platform::icon_pixmap(icon_item *icon)
{
  return static_cast<icon_buffer_item *>(icon)->buf;
}

QSvgRenderer *
osm2go_platform::icon_renderer(const icon_item *icon)
{
  return static_cast<const icon_buffer_item *>(icon)->renderer.get();
}
