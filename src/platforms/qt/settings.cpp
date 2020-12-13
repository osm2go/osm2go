/*
 * SPDX-FileCopyrightText: 2020 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "settings.h"

#include "project.h"
#include "wms.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <map>
#include <memory>
#include <QDebug>
#include <QSettings>
#include <QStandardPaths>
#include <QVariant>

#include "osm2go_annotations.h"
#include <osm2go_cpp.h>
#include "osm2go_stl.h"

#define ST_ENTRY(a) std::make_pair(#a, &(a))

const char *api06https = "https://api.openstreetmap.org/api/0.6";
const char *apihttp = "http://api.openstreetmap.org/api/0.";

static const std::map<TrackVisibility, QLatin1String> trackVisibilityKeys = {
  { RecordOnly, QLatin1String("RecordOnly") },
  { ShowPosition, QLatin1String("ShowPosition") },
  { DrawCurrent, QLatin1String("DrawCurrent") },
  { DrawAll, QLatin1String("DrawAll") }
};

settings_t::ref settings_t::instance()
{
  static std::weak_ptr<settings_t> inst;
  ref settings = inst.lock();
  if (settings)
    return settings;

  settings.reset(new settings_t());
  inst = settings;

  /* ------ overwrite with settings from gconf if present ------- */
  QSettings qsettings;

  /* restore everything listed in the store tables */
  for (auto [k, v]: settings->store_str) {
    const QString key = QLatin1String(k);
    if (qsettings.contains(key))
      *v = qsettings.value(key).toString().toStdString();
  }
  for (auto [k, v]: settings->store_bool) {
    const QString key = QLatin1String(k);
    if (qsettings.contains(key))
      *v = qsettings.value(key).toBool();
  }

  const QVariant tv = qsettings.value(QLatin1String("track_visibility"));
  settings->trackVisibility = DrawAll;
  if (!tv.isNull()) {
    const auto it = std::find_if(trackVisibilityKeys.begin(), trackVisibilityKeys.end(),
                                 [key = tv.toString()](auto && p) { return p.second == key; });
    if(it != trackVisibilityKeys.cend())
      settings->trackVisibility = it->first;
  }

  /* restore wms server list */
  int count = qsettings.beginReadArray(QLatin1String("wms"));
  if (count > 0) {
    for(int i = 0; i < count; i++) {
      qsettings.setArrayIndex(i);
      const QString server = qsettings.value(QStringLiteral("server")).toString();
      const QString name = qsettings.value(QStringLiteral("name")).toString();

      /* apply valid entry to list */
      if(likely(!name.isEmpty() && !server.isEmpty())) {
        wms_server_t *cur = new wms_server_t();
        cur->server = server.toStdString();
        cur->name = name.toStdString();
        settings->wms_server.push_back(cur);
      }
    }
  } else {
    /* add default server(s) */
    qDebug("No WMS servers configured, adding default");
    settings->wms_server = wms_server_get_default();
  }

  /* use demo setup if present */
  if(settings->project.empty() && settings->base_path.empty()) {
    qDebug("base_path not set, assuming first time run");

    /* check for presence of demo project */
    std::string fullname = find_file("demo/demo.proj");
    if(!fullname.empty()) {
      qDebug("demo project exists, use it as default");
      settings->project = fullname;
      settings->first_run_demo = true;
    }
  }

  /* ------ set useful defaults ------- */

  if(unlikely(settings->base_path.empty())) {
    const QString bpath = QStandardPaths::standardLocations(QStandardPaths::HomeLocation).first() + QStringLiteral("/.osm2go/");
    settings->base_path = bpath.toStdString();

    qDebug() << "base_path = " << bpath;
  }

  fdguard fg(settings->base_path.c_str(), O_DIRECTORY | O_RDONLY);
  settings->base_path_fd.swap(fg);

  if(unlikely(settings->server.empty())) {
    /* ------------- setup download defaults -------------------- */
    settings->server = api06https;
  }

  if(settings->username.empty())
    settings->username = qgetenv("OSM_USER").toStdString();

  if(settings->password.empty())
    settings->password = qgetenv("OSM_PASS").toStdString();

  if(unlikely(settings->style.empty()))
    settings->style = DEFAULT_STYLE;

  return settings;
}

void settings_t::save() const
{
  QSettings qsettings;

  /* store everything listed in the store tables */
  for (auto [k, v] : store_str) {
    const QString key = QString::fromStdString(k);
    if(!v->empty())
      qsettings.setValue(key, QString::fromStdString(*v));
    else
      qsettings.remove(key);
  }

  for (auto [k, v] : store_bool)
    qsettings.setValue(QString::fromStdString(k), *v);

  qsettings.setValue(QLatin1String("track_visibility"),
                     trackVisibilityKeys.find(trackVisibility)->second);

  /* store list of wms servers */
  qsettings.beginWriteArray(QLatin1String("wms"), wms_server.size());
  for(unsigned int count = 0; count < wms_server.size(); count++) {
    const wms_server_t * const cur = wms_server[count];
    qsettings.setArrayIndex(count);
    qsettings.setValue(QLatin1String("server"), QString::fromStdString(cur->server));
    qsettings.setValue(QLatin1String("name"), QString::fromStdString(cur->name));
  }
  qsettings.endArray();
}

settings_t::settings_t()
  : base_path_fd(-1)
  , enable_gps(false)
  , follow_gps(false)
  , trackVisibility(DrawAll)
  , first_run_demo(false)
  , store_str({{
                /* not user configurable */
                ST_ENTRY(base_path),
                /* from project.cpp */
                ST_ENTRY(project),
                /* from osm_api.cpp */
                ST_ENTRY(server),
                ST_ENTRY(username),
                ST_ENTRY(password),
                /* style */
                ST_ENTRY(style),
                /* main */
                ST_ENTRY(track_path)
  }})
  , store_bool({{
               ST_ENTRY(enable_gps),
               ST_ENTRY(follow_gps),
               ST_ENTRY(imperial_units)
  }})
{
}

settings_t::~settings_t()
{
  save();
  std::for_each(wms_server.begin(), wms_server.end(), std::default_delete<wms_server_t>());
}

bool api_adjust(std::string &rserver) {
  if(unlikely(rserver.size() > strlen(apihttp) &&
                strncmp(rserver.c_str(), apihttp, strlen(apihttp)) == 0 &&
                (rserver[strlen(apihttp)] == '5' ||
                 rserver[strlen(apihttp)] == '6'))) {
    rserver = api06https;
    return true;
  }

  return false;
}
