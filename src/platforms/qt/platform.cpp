/*
 * SPDX-FileCopyrightText: 2020 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "osm2go_platform.h"
#include "osm2go_platform_qt.h"

#include <pos.h>

#include <osm2go_annotations.h>
#include <osm2go_i18n.h>

#include <cmath>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <QByteArray>
#include <QColor>
#include <QCoreApplication>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QFont>
#include <QMessageBox>
#include <QStandardPaths>
#include <QUrl>
#include <sys/stat.h>
#include <utility>

void
osm2go_platform::process_events()
{
 QCoreApplication::processEvents();
}

osm2go_platform::MappedFile::MappedFile(const std::string &fname)
  : map(QString::fromStdString(fname))
{
  map.open(QIODevice::ReadOnly);
  len = map.size();
  mem = reinterpret_cast<char *>(map.map(0, len));
  map.close();
}

osm2go_platform::MappedFile::~MappedFile()
{
  reset();
}

void
osm2go_platform::MappedFile::reset()
{
  if(likely(mem != nullptr)) {
    map.unmap(reinterpret_cast<uchar *>(mem));
    mem = nullptr;
  }
}

std::optional<color_t>
osm2go_platform::parse_color_string(const char *str)
{
  /* we parse this directly since QColor expects the alpha channel as */
  /* first components, while for Gtk compatibility color_t has it last. */
  if (strlen(str + 1) == 8) {
    char *err;

    color_t color = strtoul(str + 1, &err, 16);

    if (*err == '\0')
      return color;
  } else {
    QColor qc(str);
    if(qc.isValid())
      return color_t(static_cast<uint8_t>(qc.red()), qc.green(), qc.blue(), qc.alpha());
  }

  return {};
}

double
osm2go_platform::string_to_double(const char *str)
{
  if(str != nullptr)
    return QByteArray::fromRawData(str, strlen(str)).toDouble();
  else
    return NAN;
}

bool
osm2go_platform::yes_no(const trstring &title, const trstring &msg, unsigned int again_flags, osm2go_platform::Widget *parent)
{
  /* flags used to prevent re-appearence of dialogs */
  static struct {
    unsigned int not_again;     /* bit is set if dialog is not to be displayed again */
    unsigned int reply;         /* reply to be assumed if "not_again" bit is set */
  } dialog_again;
  const unsigned int again_bit = again_flags & ~(MISC_AGAIN_FLAG_DONT_SAVE_NO | MISC_AGAIN_FLAG_DONT_SAVE_YES);

  if(dialog_again.not_again & again_bit)
    return ((dialog_again.reply & again_bit) != 0);

  qDebug() << title << ":" << msg;

  bool yes = QMessageBox::Yes == QMessageBox::question(parent, title, msg);
  // FIXME: again

  return yes;
}

namespace {

std::vector<dirguard>
base_paths_init()
{
  // in home directory
  const auto l = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation);
  const auto home = QStandardPaths::standardLocations(QStandardPaths::HomeLocation);

  QStringList pathnames;
  pathnames.reserve(l.size() + home.size() + 3);

  for (auto &&p : l)
    pathnames.push_back(p + QLatin1Char('/'));
  for (auto &&p : home)
    pathnames.push_back(p + QStringLiteral("/.osm2go/"));
  // final installation path
  pathnames.push_back(QStringLiteral(DATADIR "/"));
  // local paths for testing
  pathnames.push_back(QStringLiteral("./data/"));
  pathnames.push_back(QStringLiteral("../data/"));

  std::vector<dirguard> ret;

  for (const QString &path : qAsConst(pathnames)) {
    assert(path.endsWith(QLatin1Char('/')));
    dirguard dfd(path.toUtf8().constData());
    if(dfd.valid())
      ret.push_back(std::move(dfd));
  }

  assert(!ret.empty());

  return ret;
}

} // namespace

const std::vector<dirguard> &
osm2go_platform::base_paths()
{
  // all entries must contain a trailing '/' !
  static std::vector<dirguard> ret = base_paths_init();

  return ret;
}

QString
osm2go_platform::find_file(const QString &n)
{
  assert(!n.isEmpty());

  QString ret;

  if(unlikely(n.startsWith(QLatin1Char('/')))) {
    QFileInfo info(n);
    if(info.isFile())
      ret = n;
    return ret;
  }

  const std::vector<dirguard> &paths = osm2go_platform::base_paths();
  const std::string &fn = n.toStdString();

  for (const auto &p : paths) {
    struct stat st;
    if(fstatat(p.dirfd(), fn.c_str(), &st, 0) == 0 && S_ISREG(st.st_mode)) {
      ret = QString::fromStdString(p.path()) + n;
      break;
    }
  }

  return ret;
}

dirguard
osm2go_platform::userdatapath()
{
  // One must not use QCoreApplication::setOrganizationName() or this will
  // return wrong paths on the desktop. For a mobile app, however, this is fine.
  const QString p = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QLatin1String("/presets/");
  return dirguard(p.toUtf8().constData());
}

bool
osm2go_platform::create_directories(const std::string &path)
{
  return QDir().mkpath(QString::fromStdString(path));
}

assert_cmpstr_struct::assert_cmpstr_struct(const trstring &a, const char *astr, const trstring &b, const char *bstr, const char *file, const char *func, int line)
{
  if(unlikely(a != b))
    fail(a.toStdString().c_str(), astr, b.toStdString().c_str(), bstr, file, func, line);
}

assert_cmpstr_struct::assert_cmpstr_struct(trstring::arg_type a, const char *astr, const char *b, const char *file, const char *func, int line)
{
  trstring::native_type nativeA = static_cast<trstring::native_type>(a);
  if(unlikely(nativeA.toStdString() != b))
    fail(nativeA.toStdString().c_str(), astr, b, file, func, line);
}

assert_cmpstr_struct::assert_cmpstr_struct(trstring::arg_type a, const char *astr, const char *b, const char *bstr, const char *file, const char *func, int line)
{
  trstring::native_type nativeA = static_cast<trstring::native_type>(a);
  if(unlikely(nativeA.toStdString() != b))
    fail(nativeA.toStdString().c_str(), astr, b, bstr, file, func, line);
}

assert_cmpstr_struct::assert_cmpstr_struct(trstring::arg_type a, const char *astr, const std::string &b, const char *bstr, const char *file, const char *func, int line)
{
  trstring::native_type nativeA = static_cast<trstring::native_type>(a);
  if(unlikely(nativeA.toStdString() != b))
    fail(nativeA.toStdString().c_str(), astr, b.c_str(), bstr, file, func, line);
}

void
osm2go_platform::open_url(const char *url)
{
  QDesktopServices::openUrl(QUrl(QLatin1String(url)));
}

void
osm2go_platform::dialog_size_hint(QWidget *window, osm2go_platform::DialogSizeHint hint)
{
  const std::array<QSize, 5> dialog_sizes = { {
    { 300, 100 },  // SMALL
    { 400, 300 },  // MEDIUM
    { 500, 350 },  // LARGE
    { 450, 100 },  // WIDE
    { 200, 350 },  // HIGH
  } };

  window->setMinimumSize(dialog_sizes.at(hint));
}

QVariant
osm2go_platform::modelHightlightModified()
{
  QFont ft;
  ft.setUnderline(true);
  return ft;
}
