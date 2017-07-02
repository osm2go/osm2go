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

#include "appdata.h"
#include "misc.h"
#include "project.h"
#include "wms.h"

#include <gconf/gconf.h>
#include <gconf/gconf-client.h>
#if __cplusplus >= 201103L
#include <cstdint>
#else
#include <stdint.h>
#endif

#include <osm2go_cpp.h>

struct store_t {
  const char *key;
  ptrdiff_t offset;
};

#define ST_ENTRY(a) { #a, reinterpret_cast<ptrdiff_t>(&(reinterpret_cast<settings_t *>(0)->a)) }

static store_t store_str[] = {
  /* not user configurable */
  ST_ENTRY(base_path),

  /* from project.c */
  ST_ENTRY(project),

  /* from osm_api.c */
  ST_ENTRY(server),
  ST_ENTRY(username),
  ST_ENTRY(password),

  /* wms servers are saved seperately */

  /* style */
  ST_ENTRY(style),

  /* main */
  ST_ENTRY(track_path),

  { O2G_NULLPTR, -1 }
};

static store_t store_bool[] = {
  /* main */
  ST_ENTRY(enable_gps),
  ST_ENTRY(follow_gps),

  { O2G_NULLPTR, -1 }
};

static const std::string keybase = "/apps/" PACKAGE "/";

settings_t *settings_load(void) {
  settings_t *settings = g_new0(settings_t,1);
  const char *api06https = "https://api.openstreetmap.org/api/0.6";

  /* ------ overwrite with settings from gconf if present ------- */
  GConfClient *client = gconf_client_get_default();

  if(G_LIKELY(client != O2G_NULLPTR)) {
    /* restore everything listed in the store tables */
    std::string key;
    for(const store_t *st = store_str; st->key; st++) {
      char **ptr = reinterpret_cast<char **>(reinterpret_cast<uintptr_t>(settings) + st->offset);
      key = keybase + st->key;

      /* check if key is present */
      GConfValue *value = gconf_client_get(client, key.c_str(), O2G_NULLPTR);

      if(!value)
        continue;

      if(value->type != GCONF_VALUE_STRING) {
        printf("invalid type found for key '%s': expected %u, got %u\n",
               st->key, GCONF_VALUE_STRING, value->type);
      } else {
        g_assert_null(*ptr);
        *ptr = g_strdup(gconf_value_get_string(value));
      }
      gconf_value_free(value);
    }

    for(const store_t *st = store_bool; st->key; st++) {
      gboolean *ptr = reinterpret_cast<gboolean *>(reinterpret_cast<uintptr_t>(settings) + st->offset);
      key = keybase + st->key;

      /* check if key is present */
      GConfValue *value = gconf_client_get(client, key.c_str(), O2G_NULLPTR);

      if(!value)
        continue;

      if(value->type != GCONF_VALUE_BOOL) {
        printf("invalid type found for key '%s': expected %u, got %u\n",
               st->key, GCONF_VALUE_BOOL, value->type);
      } else {
        *ptr = gconf_value_get_bool(value);
      }
      gconf_value_free(value);
    }

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
      gconf_value_free(value);

      int i, count = gconf_client_get_int(client, countkey, O2G_NULLPTR);

      wms_server_t **cur = &settings->wms_server;
      for(i=0;i<count;i++) {
        char nbuf[16];
        snprintf(nbuf, sizeof(nbuf), "%i", i);

        key = keybase + "wms/server" + nbuf;
        gchar *server = gconf_client_get_string(client, key.c_str(), O2G_NULLPTR);
        key = keybase + "wms/name" + nbuf;
        gchar *name = gconf_client_get_string(client, key.c_str(), O2G_NULLPTR);
        key = keybase + "wms/path" + nbuf;
        gchar *path = gconf_client_get_string(client, key.c_str(), O2G_NULLPTR);

	/* apply valid entry to list */
	if(name && server && path) {
	  *cur = g_new0(wms_server_t, 1);
	  (*cur)->name = name;
	  (*cur)->server = server;
	  (*cur)->path = path;
	  cur = &(*cur)->next;
	} else {
	  g_free(name);
	  g_free(server);
	  g_free(path);
	}
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

void settings_save(settings_t *settings) {

  GConfClient *client = gconf_client_get_default();
  if(!client) return;

  std::string key;

  /* store everything listed in the store tables */
  for(const store_t *st = store_str; st->key; st++) {
    char **ptr = reinterpret_cast<char **>(reinterpret_cast<uintptr_t>(settings) + st->offset);
    key = keybase + st->key;

    if(*ptr)
      gconf_client_set_string(client, key.c_str(), (char*)(*ptr), O2G_NULLPTR);
    else
      gconf_client_unset(client, key.c_str(), O2G_NULLPTR);
  }

  for(const store_t *st = store_bool; st->key; st++) {
    gboolean *ptr = reinterpret_cast<gboolean *>(reinterpret_cast<uintptr_t>(settings) + st->offset);
    key = keybase + st->key;

    gconf_client_set_bool(client, key.c_str(), *ptr, O2G_NULLPTR);
  }

  /* store list of wms servers */
  unsigned int count = 0;
  for(wms_server_t *cur = settings->wms_server; cur; cur = cur->next) {
    char nbuf[16];
    snprintf(nbuf, sizeof(nbuf), "%u", count);

    key = keybase + "wms/server" + nbuf;
    gconf_client_set_string(client, key.c_str(), cur->server, O2G_NULLPTR);
    key = keybase + "wms/name" + nbuf;
    gconf_client_set_string(client, key.c_str(), cur->name, O2G_NULLPTR);
    key = keybase + "wms/path" + nbuf;
    gconf_client_set_string(client, key.c_str(), cur->path, O2G_NULLPTR);

    count++;
  }

  gconf_client_set_int(client, "/apps/" PACKAGE "/wms/count", count, O2G_NULLPTR);

  g_object_unref(client);
}

void settings_free(settings_t *settings) {
  wms_servers_free(settings->wms_server);

  for(const store_t *st = store_str; st->key; st++) {
    char **ptr = reinterpret_cast<char **>(reinterpret_cast<uintptr_t>(settings) + st->offset);

    g_free(*ptr);
  }

  g_free(settings);
}
