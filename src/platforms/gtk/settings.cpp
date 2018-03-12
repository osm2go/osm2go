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

#include "settings.h"

#include "misc.h"
#include "project.h"
#include "wms.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <gconf/gconf.h>
#include <gconf/gconf-client.h>
#include <glib.h>
#include <map>
#include <memory>

#include "osm2go_annotations.h"
#include <osm2go_cpp.h>
#include "osm2go_stl.h"

#define ST_ENTRY(map, a) map.push_back(std::pair<const char *, typeof(a) *>(#a, &a))

static const std::string keybase = "/apps/" PACKAGE "/";
const char *api06https = "https://api.openstreetmap.org/api/0.6";
const char *apihttp = "http://api.openstreetmap.org/api/0.";

static std::map<TrackVisibility, std::string> trackVisibilityKeys;

static settings_t *settings;

static void initTrackVisibility() {
  trackVisibilityKeys[RecordOnly] = "RecordOnly";
  trackVisibilityKeys[ShowPosition] = "ShowPosition";
  trackVisibilityKeys[DrawCurrent] = "DrawCurrent";
  trackVisibilityKeys[DrawAll] = "DrawAll";
}

struct matchTrackVisibility {
  const std::string key;
  explicit matchTrackVisibility(const std::string &k) : key(k) {}
  bool operator()(const std::pair<TrackVisibility, std::string> &p) {
    return p.second == key;
  }
};

bool gconf_value_get_bool_wrapper(const GConfValue *gvalue) {
  return gconf_value_get_bool(gvalue) == TRUE;
}

template<typename T, typename U, U GETTER(const GConfValue *)> struct load_functor {
  std::string &key; ///< reference to avoid most reallocations
  GConfClient * const client;
  const GConfValueType type;
  load_functor(std::string &k, std::unique_ptr<GConfClient, g_object_deleter> &c, GConfValueType t)
    : key(k), client(c.get()), type(t) {}
  void operator()(const std::pair<const char *, T *> &p);
};

template<typename T, typename U, U GETTER(const GConfValue *)> void load_functor<T, U, GETTER>::operator()(const std::pair<const char *, T *> &p)
{
  key = keybase + p.first;

  /* check if key is present */
  GConfValue *value = gconf_client_get(client, key.c_str(), O2G_NULLPTR);

  if(!value)
    return;

  if(unlikely(value->type != type)) {
    g_warning("invalid type found for key '%s': expected %u, got %u", p.first, type, value->type);
  } else {
    *(p.second) = GETTER(value);
  }
  gconf_value_free(value);
}

settings_t *settings_t::instance() {
  if(likely(settings != O2G_NULLPTR))
    return settings;

  settings = new settings_t();

  if(likely(trackVisibilityKeys.empty()))
    initTrackVisibility();

  /* ------ overwrite with settings from gconf if present ------- */
  std::unique_ptr<GConfClient, g_object_deleter> client(gconf_client_get_default());

  if(likely(client)) {
    /* restore everything listed in the store tables */
    std::string key;

    std::for_each(settings->store_str.begin(), settings->store_str.end(),
                  load_functor<std::string, const char *, gconf_value_get_string>(key, client, GCONF_VALUE_STRING));
    std::for_each(settings->store_bool.begin(), settings->store_bool.end(),
                  load_functor<bool, bool, gconf_value_get_bool_wrapper>(key, client, GCONF_VALUE_BOOL));

    /* adjust default server stored in settings if required */
    if(unlikely(api_adjust(settings->server)))
      g_debug("adjusting server path in settings");

    key = keybase + "track_visibility";
    GConfValue *gvalue = gconf_client_get(client.get(), key.c_str(), O2G_NULLPTR);
    settings->trackVisibility = DrawAll;
    if(likely(gvalue != O2G_NULLPTR)) {
      const std::map<TrackVisibility, std::string>::const_iterator it =
          std::find_if(trackVisibilityKeys.begin(), trackVisibilityKeys.end(),
                       matchTrackVisibility(gconf_value_get_string(gvalue)));
      if(it != trackVisibilityKeys.end())
        settings->trackVisibility = it->first;

      gconf_value_free(gvalue);
    }

    /* restore wms server list */
    const gchar *countkey = "/apps/" PACKAGE "/wms/count";
    GConfValue *value = gconf_client_get(client.get(), countkey, O2G_NULLPTR);
    if(value) {
      unsigned int count = gconf_value_get_int(value);
      gconf_value_free(value);

      for(unsigned int i = 0; i < count; i++) {
        char nbuf[16];
        snprintf(nbuf, sizeof(nbuf), "%u", i);

        key = keybase + "wms/server" + nbuf;
        GConfValue *server = gconf_client_get(client.get(), key.c_str(), O2G_NULLPTR);
        key = keybase + "wms/name" + nbuf;
        GConfValue *name = gconf_client_get(client.get(), key.c_str(), O2G_NULLPTR);
        key = keybase + "wms/path" + nbuf;
        GConfValue *path = gconf_client_get(client.get(), key.c_str(), O2G_NULLPTR);

        /* apply valid entry to list */
        if(likely(name != O2G_NULLPTR && server != O2G_NULLPTR)) {
          wms_server_t *cur = new wms_server_t();
          cur->name = gconf_value_get_string(name);
          cur->server = gconf_value_get_string(server);
          // upgrade old entries
          if(unlikely(path != O2G_NULLPTR)) {
            cur->server += gconf_value_get_string(path);
            gconf_client_unset(client.get(), key.c_str(), O2G_NULLPTR);
          }
          settings->wms_server.push_back(cur);
        }
        gconf_value_free(name);
        gconf_value_free(server);
        gconf_value_free(path);
      }
    } else {
      /* add default server(s) */
      g_debug("No WMS servers configured, adding default");
      settings->wms_server = wms_server_get_default();
    }

    /* use demo setup if present */
    if(settings->project.empty() && settings->base_path.empty()) {
      g_debug("base_path not set, assuming first time run");

      /* check for presence of demo project */
      std::string fullname = find_file("demo/demo.proj");
      if(!fullname.empty()) {
        g_debug("demo project exists, use it as default");
        settings->project = fullname;
        settings->first_run_demo = true;
      }
    }

    client.reset();
  }

  /* ------ set useful defaults ------- */

  const char *p;
  if(unlikely(settings->base_path.empty())) {
#ifdef FREMANTLE
    /* try to use internal memory card on hildon/maemo */
    p = getenv("INTERNAL_MMC_MOUNTPOINT");
    if(!p)
#endif
      p = getenv("HOME");

    /* if everthing fails use tmp dir */
    if(!p)
      p = "/tmp";

    settings->base_path = p;

    /* build image path in home directory */
    if(strncmp(p, "/home", 5) == 0)
      settings->base_path += "/.osm2go/";
    else
      settings->base_path += "/osm2go/";

    g_debug("base_path = %s", settings->base_path.c_str());
  }

  fdguard fg(settings->base_path.c_str(), O_DIRECTORY | O_RDONLY);
  settings->base_path_fd.swap(fg);

  if(unlikely(settings->server.empty())) {
    /* ------------- setup download defaults -------------------- */
    settings->server = api06https;
  }

  if(settings->username.empty()) {
    if((p = getenv("OSM_USER")))
      settings->username = p;
  }

  if(settings->password.empty()) {
    if((p = getenv("OSM_PASS")))
      settings->password = p;
  }

  if(unlikely(settings->style.empty()))
    settings->style = DEFAULT_STYLE;

  return settings;
}

void settings_t::save() const {
  std::unique_ptr<GConfClient, g_object_deleter> client(gconf_client_get_default());
  if(!client) return;

  std::string key;

  if(unlikely(trackVisibilityKeys.empty()))
    initTrackVisibility();

  /* store everything listed in the store tables */
  const StringKeys::const_iterator sitEnd = store_str.end();
  for(StringKeys::const_iterator it = store_str.begin();
      it != sitEnd; it++) {
    key = keybase + it->first;

    if(!it->second->empty())
      gconf_client_set_string(client.get(), key.c_str(), it->second->c_str(), O2G_NULLPTR);
    else
      gconf_client_unset(client.get(), key.c_str(), O2G_NULLPTR);
  }

  const BooleanKeys::const_iterator bitEnd = store_bool.end();
  for(BooleanKeys::const_iterator it = store_bool.begin();
      it != bitEnd; it++) {
    key = keybase + it->first;

    gconf_client_set_bool(client.get(), key.c_str(), *(it->second), O2G_NULLPTR);
  }

  key = keybase + "track_visibility";
  gconf_client_set_string(client.get(), key.c_str(), trackVisibilityKeys[trackVisibility].c_str(), O2G_NULLPTR);

  /* store list of wms servers */
  for(unsigned int count = 0; count < wms_server.size(); count++) {
    const wms_server_t * const cur = wms_server[count];
    char nbuf[16];
    snprintf(nbuf, sizeof(nbuf), "%u", count);

    key = keybase + "wms/server" + nbuf;
    gconf_client_set_string(client.get(), key.c_str(), cur->server.c_str(), O2G_NULLPTR);
    key = keybase + "wms/name" + nbuf;
    gconf_client_set_string(client.get(), key.c_str(), cur->name.c_str(), O2G_NULLPTR);
  }

  gconf_client_set_int(client.get(), "/apps/" PACKAGE "/wms/count", wms_server.size(), O2G_NULLPTR);
}

settings_t::settings_t()
  : base_path_fd(-1)
  , enable_gps(FALSE)
  , follow_gps(FALSE)
  , first_run_demo(false)
  , store_str(7)
  , store_bool(2)
{
  store_str.clear();
  store_bool.clear();

  /* not user configurable */
  ST_ENTRY(store_str, base_path);

  /* from project.c */
  ST_ENTRY(store_str, project);

  /* from osm_api.c */
  ST_ENTRY(store_str, server);
  ST_ENTRY(store_str, username);
  ST_ENTRY(store_str, password);

  /* wms servers are saved seperately */

  /* style */
  ST_ENTRY(store_str, style);

  /* main */
  ST_ENTRY(store_str, track_path);

  ST_ENTRY(store_bool, enable_gps);
  ST_ENTRY(store_bool, follow_gps);
}

settings_t::~settings_t()
{
  settings = O2G_NULLPTR;
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
