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

#include <osm2go_cpp.h>

typedef struct {
  const char *key;
  GConfValueType type;
  int offset;
} store_t;

#define OFFSET(a) offsetof(settings_t, a)

static store_t store[] = {
  /* not user configurable */
  { "base_path",        GCONF_VALUE_STRING, OFFSET(base_path)    },

  /* from project.c */
  { "project",          GCONF_VALUE_STRING, OFFSET(project)      },

  /* from osm_api.c */
  { "server",           GCONF_VALUE_STRING, OFFSET(server)       },
  { "username",         GCONF_VALUE_STRING, OFFSET(username)     },
  { "password",         GCONF_VALUE_STRING, OFFSET(password)     },

  /* wms servers are saved seperately */

  /* style */
  { "style",            GCONF_VALUE_STRING, OFFSET(style)        },

  /* main */
  { "track_path",       GCONF_VALUE_STRING, OFFSET(track_path)   },
  { "enable_gps",       GCONF_VALUE_BOOL,   OFFSET(enable_gps)   },
  { "follow_gps",       GCONF_VALUE_BOOL,   OFFSET(follow_gps)   },

  { NULL, -1, -1 }
};

settings_t *settings_load(void) {
  settings_t *settings = g_new0(settings_t,1);
  const char *api06https = "https://api.openstreetmap.org/api/0.6";

  /* ------ overwrite with settings from gconf if present ------- */
  GConfClient *client = gconf_client_get_default();

  if(G_LIKELY(client != O2G_NULLPTR)) {
    /* restore everything listed in the store table */
    const store_t *st;
    for(st = store; st->key; st++) {
      void **ptr = ((void*)settings) + st->offset;
      gchar *key = g_strconcat("/apps/" PACKAGE "/", st->key, NULL);

      /* check if key is present */
      GConfValue *value = gconf_client_get(client, key, NULL);
      g_free(key);

      if(!value)
        continue;

      if(value->type != st->type) {
        printf("invalid type found for key '%s': expected %u, got %u\n",
               st->key, st->type, value->type);
        gconf_value_free(value);
      }

      switch(st->type) {
      case GCONF_VALUE_STRING:
        g_assert_null(*((char**)ptr));
        *((char**)ptr) = g_strdup(gconf_value_get_string(value));
        break;

      case GCONF_VALUE_BOOL:
        *((int*)ptr) = gconf_value_get_bool(value);
        break;

      default:
        printf("Unsupported type %d\n", st->type);
        break;
      }
      gconf_value_free(value);
    }

    /* adjust default server stored in settings if required */
    if(G_UNLIKELY(settings->server && strstr(settings->server, "0.5") != NULL)) {
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
    GConfValue *value = gconf_client_get(client, countkey, NULL);
    if(value) {
      gconf_value_free(value);

      int i, count = gconf_client_get_int(client, countkey, NULL);

      wms_server_t **cur = &settings->wms_server;
      for(i=0;i<count;i++) {
        /* keep ordering, the longest key must be first */
        gchar *key = g_strdup_printf("/apps/" PACKAGE "/wms/server%d", i);
        gchar *server = gconf_client_get_string(client, key, NULL);
        g_sprintf(key, "/apps/" PACKAGE "/wms/name%d", i);
        gchar *name = gconf_client_get_string(client, key, NULL);
        g_sprintf(key, "/apps/" PACKAGE "/wms/path%d", i);
        gchar *path = gconf_client_get_string(client, key, NULL);
	g_free(key);

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
    if(!settings->project) {
      char *key = g_strdup("/apps/" PACKAGE "/base_path");
      GConfValue *value = gconf_client_get(client, key, NULL);
      if(value)
	gconf_value_free(value);
      else {
	printf("base_path not set, assuming first time boot\n");

	/* check for presence of demo project */
	if(project_exists(settings, "demo")) {
	  printf("demo project exists, use it as default\n");
	  settings->project = g_strdup("demo");
	  settings->first_run_demo = TRUE;
	}
      }
    }

    g_object_unref(client);
  }

  /* ------ set useful defaults ------- */

  char *p;
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
      settings->base_path = g_strconcat(p, "/.osm2go/", NULL);
    else
      settings->base_path = g_strconcat(p, "/osm2go/", NULL);

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

  /* store everything listed in the store table */
  store_t *st = store;
  while(st->key) {
    void **ptr = ((void*)settings) + st->offset;
    char *key = g_strconcat("/apps/" PACKAGE "/", st->key, NULL);

    switch(st->type) {
    case GCONF_VALUE_STRING:
      if((char*)(*ptr))
	gconf_client_set_string(client, key, (char*)(*ptr), NULL);
      else
	gconf_client_unset(client, key, NULL);
      break;

    case GCONF_VALUE_BOOL:
      gconf_client_set_bool(client, key, *((int*)ptr), NULL);
      break;

    default:
      printf("Unsupported type %d\n", st->type);
      break;
    }

    g_free(key);
    st++;
  }

  /* store list of wms servers */
  wms_server_t *cur = settings->wms_server;
  int count = 0;
  while(cur) {
    /* keep ordering, the longest key must be first */
    gchar *key = g_strdup_printf("/apps/" PACKAGE "/wms/server%d", count);
    gconf_client_set_string(client, key, cur->server, NULL);
    g_sprintf(key, "/apps/" PACKAGE "/wms/name%d", count);
    gconf_client_set_string(client, key, cur->name, NULL);
    g_sprintf(key, "/apps/" PACKAGE "/wms/path%d", count);
    gconf_client_set_string(client, key, cur->path, NULL);
    g_free(key);

    count++;
    cur = cur->next;
  }

  gconf_client_set_int(client, "/apps/" PACKAGE "/wms/count", count, NULL);

  g_object_unref(client);
}

void settings_free(settings_t *settings) {
  store_t *st = store;

  wms_servers_free(settings->wms_server);

  while(st->key) {
    void **ptr = ((void*)settings) + st->offset;

    if(st->type == GCONF_VALUE_STRING)
      g_free(*ptr);

    st++;
  }

  g_free(settings);
}
