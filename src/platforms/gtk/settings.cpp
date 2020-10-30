/*
 * SPDX-FileCopyrightText: 2008 Till Harbaum <till@harbaum.org>
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
#include <gconf/gconf.h>
#include <gconf/gconf-client.h>
#include <glib.h>
#include <map>
#include <memory>

#include "osm2go_annotations.h"
#include <osm2go_cpp.h>
#include "osm2go_platform_gtk.h"
#include "osm2go_stl.h"

#define KEYBASE "/apps/" PACKAGE "/"
static const std::string keybase = KEYBASE;
const char *api06https = "https://api.openstreetmap.org/api/0.6";
const char *apihttp = "http://api.openstreetmap.org/api/0.";

static std::map<TrackVisibility, std::string> trackVisibilityKeys;

namespace {

void initTrackVisibility()
{
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

struct gconf_value_deleter {
  inline void operator()(GConfValue *value)
  { gconf_value_free(value); }
};
typedef std::unique_ptr<GConfValue, gconf_value_deleter> gconf_value_guard;

bool gconf_value_get_bool_wrapper(const GConfValue *gvalue) {
  return gconf_value_get_bool(gvalue) == TRUE;
}

template<typename T, typename U, U GETTER(const GConfValue *)>
struct load_functor {
  std::string &key; ///< reference to avoid most reallocations
  GConfClient * const client;
  const GConfValueType type;
  inline load_functor(std::string &k, std::unique_ptr<GConfClient, g_object_deleter> &c, GConfValueType t)
    : key(k), client(c.get()), type(t) {}
  void operator()(const std::pair<const char *, T *> &p);
};

template<typename T, typename U, U GETTER(const GConfValue *)> void
load_functor<T, U, GETTER>::operator()(const std::pair<const char *, T *> &p)
{
  key = keybase + p.first;

  /* check if key is present */
  gconf_value_guard value(gconf_client_get(client, key.c_str(), nullptr));

  if(!value)
    return;

  if(unlikely(value->type != type))
    g_warning("invalid type found for key '%s': expected %u, got %u", p.first, type, value->type);
  else
    *(p.second) = GETTER(value.get());
}

} // namespace

settings_t::ref settings_t::instance() {
  static std::weak_ptr<settings_t> inst;
  ref settings = inst.lock();
  if (settings)
    return settings;

  settings.reset(new settings_t());
  inst = settings;

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
    gconf_value_guard gvalue(gconf_client_get(client.get(), key.c_str(), nullptr));
    settings->trackVisibility = DrawAll;
    if(likely(gvalue)) {
      const std::map<TrackVisibility, std::string>::const_iterator it =
          std::find_if(trackVisibilityKeys.begin(), trackVisibilityKeys.end(),
                       matchTrackVisibility(gconf_value_get_string(gvalue.get())));
      if(it != trackVisibilityKeys.end())
        settings->trackVisibility = it->first;

      gvalue.reset();
    }

    /* restore wms server list */
    const gchar *countkey = KEYBASE "wms/count";
    gconf_value_guard value(gconf_client_get(client.get(), countkey, nullptr));
    if(value) {
      unsigned int count = gconf_value_get_int(value.get());
      value.reset();

      for(unsigned int i = 0; i < count; i++) {
        char nbuf[16];
        snprintf(nbuf, sizeof(nbuf), "%u", i);

        key = keybase + "wms/server" + nbuf;
        gconf_value_guard server(gconf_client_get(client.get(), key.c_str(), nullptr));
        const size_t offs = keybase.size() + 4;
        key.replace(offs, strlen("server"), "name");
        gconf_value_guard name(gconf_client_get(client.get(), key.c_str(), nullptr));
        key.replace(offs, strlen("name"), "path");
        gconf_value_guard path(gconf_client_get(client.get(), key.c_str(), nullptr));

        /* apply valid entry to list */
        if(likely(name && server)) {
          wms_server_t *cur = new wms_server_t(gconf_value_get_string(name.get()),
                                               gconf_value_get_string(server.get()));
          // upgrade old entries
          if(unlikely(path)) {
            cur->server += gconf_value_get_string(path.get());
            gconf_client_unset(client.get(), key.c_str(), nullptr);
          }
          settings->wms_server.push_back(cur);
        }
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
      std::string fullname = osm2go_platform::find_file("demo/demo.proj");
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
    if(p == nullptr)
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
    if((p = getenv("OSM_USER")) != nullptr)
      settings->username = p;
  }

  if(settings->password.empty()) {
    if((p = getenv("OSM_PASS")) != nullptr)
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
      gconf_client_set_string(client.get(), key.c_str(), it->second->c_str(), nullptr);
    else
      gconf_client_unset(client.get(), key.c_str(), nullptr);
  }

  const BooleanKeys::const_iterator bitEnd = store_bool.end();
  for(BooleanKeys::const_iterator it = store_bool.begin();
      it != bitEnd; it++) {
    key = keybase + it->first;

    gconf_client_set_bool(client.get(), key.c_str(), *(it->second) ? TRUE : FALSE, nullptr);
  }

  key = keybase + "track_visibility";
  gconf_client_set_string(client.get(), key.c_str(), trackVisibilityKeys[trackVisibility].c_str(), nullptr);

  /* store list of wms servers */
  for(unsigned int count = 0; count < wms_server.size(); count++) {
    const wms_server_t * const cur = wms_server[count];
    char nbuf[16];
    snprintf(nbuf, sizeof(nbuf), "%u", count);

    key = keybase + "wms/server" + nbuf;
    gconf_client_set_string(client.get(), key.c_str(), cur->server.c_str(), nullptr);
    const size_t offs = keybase.size() + 4;
    key.replace(offs, strlen("server"), "name");
    gconf_client_set_string(client.get(), key.c_str(), cur->name.c_str(), nullptr);
  }

  gconf_client_set_int(client.get(), KEYBASE "wms/count", wms_server.size(), nullptr);
}

namespace {

#define ST_ENTRY(a) std::make_pair(#a, &(settings.a))

inline settings_t::StringKeys
st_mapping(settings_t &settings)
{
  settings_t::StringKeys sstring = {{
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
  }};

  return sstring;
}

inline settings_t::BooleanKeys
b_mapping(settings_t &settings)
{
  settings_t::BooleanKeys sbool = {{
               ST_ENTRY(enable_gps),
               ST_ENTRY(follow_gps),
               ST_ENTRY(imperial_units)
  }};

  return sbool;
}

} // namespace

settings_t::settings_t()
  : base_path_fd(-1)
  , enable_gps(false)
  , follow_gps(false)
  , imperial_units(false)
  , trackVisibility(DrawAll)
  , first_run_demo(false)
  , store_str(st_mapping(*this))
  , store_bool(b_mapping(*this))
{
}

settings_t::~settings_t()
{
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
