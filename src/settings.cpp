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

#include "settings.h"

#include "misc.h"
#include "project.h"
#include "wms.h"

#include <algorithm>
#include <cstring>
#include <gconf/gconf.h>
#include <gconf/gconf-client.h>
#if __cplusplus >= 201103L
#include <cstdint>
#else
#include <stdint.h>
#endif

#include <osm2go_cpp.h>

#define ST_ENTRY(map, a) map[#a] = &a

static const std::string keybase = "/apps/" PACKAGE "/";

template<typename T> struct load_functor {
  std::string &key; ///< reference to avoid most reallocations
  GConfClient * const client;
  const GConfValueType type;
  load_functor(std::string &k, GConfClient *c, GConfValueType t)
    : key(k), client(c), type(t) {}
  void operator()(const std::pair<const char *, T *> &p);
  T get(GConfValue *value);
};

template<typename T> void load_functor<T>::operator()(const std::pair<const char *, T *> &p)
{
  key = keybase + p.first;

  /* check if key is present */
  GConfValue *value = gconf_client_get(client, key.c_str(), O2G_NULLPTR);

  if(!value)
    return;

  if(G_UNLIKELY(value->type != type)) {
    printf("invalid type found for key '%s': expected %u, got %u\n",
           p.first, type, value->type);
  } else {
    *(p.second) = get(value);
  }
  gconf_value_free(value);
}

template<> char *load_functor<char *>::get(GConfValue *value)
{
  return g_strdup(gconf_value_get_string(value));
}

template<> gboolean load_functor<gboolean>::get(GConfValue *value)
{
  return gconf_value_get_bool(value);
}

settings_t *settings_t::load() {
  settings_t *settings = new settings_t();
  const char *api06https = "https://api.openstreetmap.org/api/0.6";

  /* ------ overwrite with settings from gconf if present ------- */
  GConfClient *client = gconf_client_get_default();

  if(G_LIKELY(client != O2G_NULLPTR)) {
    /* restore everything listed in the store tables */
    std::string key;

    std::for_each(settings->store_str.begin(), settings->store_str.end(),
                  load_functor<char *>(key, client, GCONF_VALUE_STRING));
    std::for_each(settings->store_bool.begin(), settings->store_bool.end(),
                  load_functor<gboolean>(key, client, GCONF_VALUE_BOOL));

    /* adjust default server stored in settings if required */
    if(G_UNLIKELY(settings->server && strstr(settings->server, "0.5") != O2G_NULLPTR)) {
      strstr(settings->server, "0.5")[2] = '6';
      printf("adjusting server path in settings to 0.6\n");
    }
    const char *api06http = "http://api.openstreetmap.org/api/0.6";
    if(G_UNLIKELY(settings->server && strncmp(settings->server, api06http, strlen(api06http)) == 0)) {
      g_free(settings->server);
      settings->server = g_strdup(api06https);
      printf("adjusting server path in settings to https\n");
    }

    /* restore wms server list */
    const gchar *countkey = "/apps/" PACKAGE "/wms/count";
    GConfValue *value = gconf_client_get(client, countkey, O2G_NULLPTR);
    if(value) {
      unsigned int count = gconf_value_get_int(value);
      gconf_value_free(value);

      for(unsigned int i = 0; i < count; i++) {
        char nbuf[16];
        snprintf(nbuf, sizeof(nbuf), "%i", i);

        key = keybase + "wms/server" + nbuf;
        GConfValue *server = gconf_client_get(client, key.c_str(), O2G_NULLPTR);
        key = keybase + "wms/name" + nbuf;
        GConfValue *name = gconf_client_get(client, key.c_str(), O2G_NULLPTR);
        key = keybase + "wms/path" + nbuf;
        GConfValue *path = gconf_client_get(client, key.c_str(), O2G_NULLPTR);

	/* apply valid entry to list */
        if(G_LIKELY(name && server && path)) {
          wms_server_t *cur = new wms_server_t();
          cur->name = gconf_value_get_string(name);
          cur->server = gconf_value_get_string(server);
          cur->path = gconf_value_get_string(path);
          settings->wms_server.push_back(cur);
        }
        gconf_value_free(name);
        gconf_value_free(server);
        gconf_value_free(path);
      }
    } else {
      /* add default server(s) */
      printf("No WMS servers configured, adding default\n");
      settings->wms_server = wms_server_get_default();
    }

    /* use demo setup if present */
    if(!settings->project && !settings->base_path) {
      printf("base_path not set, assuming first time boot\n");

      /* check for presence of demo project */
      std::string fullname;
      if(project_exists(settings, "demo", fullname)) {
        printf("demo project exists, use it as default\n");
        settings->project = g_strdup("demo");
        settings->first_run_demo = TRUE;
      }
    }

    g_object_unref(client);
  }

  /* ------ set useful defaults ------- */

  const char *p;
  if(G_UNLIKELY(settings->base_path == O2G_NULLPTR)) {
#ifdef USE_HILDON
    /* try to use internal memory card on hildon/maemo */
    p = getenv("INTERNAL_MMC_MOUNTPOINT");
    if(!p)
#endif
      p = getenv("HOME");

    /* if everthing fails use tmp dir */
    if(!p)
      p = "/tmp";

    /* build image path in home directory */
    if(strncmp(p, "/home", 5) == 0)
      settings->base_path = g_strconcat(p, "/.osm2go/", O2G_NULLPTR);
    else
      settings->base_path = g_strconcat(p, "/osm2go/", O2G_NULLPTR);

    fprintf(stderr, "base_path = %s\n", settings->base_path);
  }

  if(G_UNLIKELY(settings->server == O2G_NULLPTR)) {
    /* ------------- setup download defaults -------------------- */
    settings->server = g_strdup(api06https);
  }

  if(!settings->username) {
    if((p = getenv("OSM_USER")))
      settings->username = g_strdup(p);
  }

  if(!settings->password) {
    if((p = getenv("OSM_PASS")))
      settings->password = g_strdup(p);
  }

  if(G_UNLIKELY(settings->style == O2G_NULLPTR))
    settings->style = g_strdup(DEFAULT_STYLE);

  return settings;
}

void settings_t::save() const {

  GConfClient *client = gconf_client_get_default();
  if(!client) return;

  std::string key;

  /* store everything listed in the store tables */
  const std::map<const char *, char **>::const_iterator sitEnd = store_str.end();
  for(std::map<const char *, char **>::const_iterator it = store_str.begin();
      it != sitEnd; it++) {
    key = keybase + it->first;

    if(*(it->second))
      gconf_client_set_string(client, key.c_str(), *(it->second), O2G_NULLPTR);
    else
      gconf_client_unset(client, key.c_str(), O2G_NULLPTR);
  }

  const std::map<const char *, gboolean *>::const_iterator bitEnd = store_bool.end();
  for(std::map<const char *, gboolean *>::const_iterator it = store_bool.begin();
      it != bitEnd; it++) {
    key = keybase + it->first;

    gconf_client_set_bool(client, key.c_str(), *(it->second), O2G_NULLPTR);
  }

  /* store list of wms servers */
  for(unsigned int count = 0; count < wms_server.size(); count++) {
    const wms_server_t * const cur = wms_server[count];
    char nbuf[16];
    snprintf(nbuf, sizeof(nbuf), "%u", count);

    key = keybase + "wms/server" + nbuf;
    gconf_client_set_string(client, key.c_str(), cur->server.c_str(), O2G_NULLPTR);
    key = keybase + "wms/name" + nbuf;
    gconf_client_set_string(client, key.c_str(), cur->name.c_str(), O2G_NULLPTR);
    key = keybase + "wms/path" + nbuf;
    gconf_client_set_string(client, key.c_str(), cur->path.c_str(), O2G_NULLPTR);
  }

  gconf_client_set_int(client, "/apps/" PACKAGE "/wms/count", wms_server.size(), O2G_NULLPTR);

  g_object_unref(client);
}

settings_t::settings_t()
  : base_path(O2G_NULLPTR)
  , project(O2G_NULLPTR)
  , server(O2G_NULLPTR)
  , username(O2G_NULLPTR)
  , password(O2G_NULLPTR)
  , style(O2G_NULLPTR)
  , track_path(O2G_NULLPTR)
  , enable_gps(FALSE)
  , follow_gps(FALSE)
  , first_run_demo(FALSE)
{
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
  std::for_each(wms_server.begin(), wms_server.end(), default_delete<wms_server_t>());

  const std::map<const char *, char **>::const_iterator sitEnd = store_str.end();
  for(std::map<const char *, char **>::const_iterator it = store_str.begin();
      it != sitEnd; it++)
    g_free(*(it->second));
}
