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
#include "wms.h"

#include <gconf/gconf.h>
#include <gconf/gconf-client.h>

#define PROXY_KEY  "/system/http_proxy/"

enum {
  STORE_STRING, STORE_FLOAT, STORE_INT, STORE_BOOL,
};

typedef struct {
  char *key;
  int type;
  int offset;
} store_t;

#define OFFSET(a) offsetof(settings_t, a)

static store_t store[] = {
  /* not user configurable */
  { "base_path",        STORE_STRING, OFFSET(base_path)    },

  /* from project.c */
  { "project",          STORE_STRING, OFFSET(project)      },

  /* from osm_api.c */
  { "server",           STORE_STRING, OFFSET(server)       },
  { "username",         STORE_STRING, OFFSET(username)     },
  { "password",         STORE_STRING, OFFSET(password)     },

  /* wms servers are saved seperately */

  /* style */
  { "style",            STORE_STRING, OFFSET(style)        },

  /* main */
  { "track_path",       STORE_STRING, OFFSET(track_path)   },
  { "enable_gps",       STORE_BOOL,   OFFSET(enable_gps)   },
  { "follow_gps",       STORE_BOOL,   OFFSET(follow_gps)   },

  { NULL, -1, -1 }
};

settings_t *settings_load(void) {
  settings_t *settings = g_new0(settings_t,1);

  /* ------ set useful defaults ------- */

  char *p = NULL;
#ifdef USE_HILDON
  /* try to use internal memory card on hildon/maemo */
  p = getenv("INTERNAL_MMC_MOUNTPOINT");
  if(!p)
#endif
    p = getenv("HOME");

  /* if everthing fails use tmp dir */
  if(!p) p = "/tmp";

  /* build image path in home directory */
  if(strncmp(p, "/home", 5) == 0)
    settings->base_path = g_strconcat(p, "/.osm2go/", NULL);
  else
    settings->base_path = g_strconcat(p, "/osm2go/", NULL);

  fprintf(stderr, "base_path = %s\n", settings->base_path);

  /* ------------- setup download defaults -------------------- */
  settings->server = g_strdup("http://api.openstreetmap.org/api/0.6");
  if((p = getenv("OSM_USER")))
    settings->username = g_strdup(p);
  else
    settings->username = g_strdup(_("<your osm username>"));

  if((p = getenv("OSM_PASS")))
    settings->password = g_strdup(p);
  else
    settings->password = g_strdup(_("<password>"));

  settings->style = g_strdup(DEFAULT_STYLE);


  /* ------ overwrite with settings from gconf if present ------- */
  GConfClient *client = gconf_client_get_default();
  if(client) {

    /* restore everything listed in the store table */
    store_t *st = store;
    while(st->key) {
      void **ptr = ((void*)settings) + st->offset;
      gchar *key = g_strconcat("/apps/" PACKAGE "/", st->key, NULL);

      /* check if key is present */
      GConfValue *value = gconf_client_get(client, key, NULL);
      if(value) {
	gconf_value_free(value);

	switch(st->type) {
	case STORE_STRING: {
	  char **str = (char**)ptr;
	  g_free(*str);
	  *str = gconf_client_get_string(client, key, NULL);
	} break;

	case STORE_BOOL:
	  *((int*)ptr) = gconf_client_get_bool(client, key, NULL);
	  break;

	case STORE_INT:
	  *((int*)ptr) = gconf_client_get_int(client, key, NULL);
	  break;

	case STORE_FLOAT:
	  *((float*)ptr) = gconf_client_get_float(client, key, NULL);
	  break;

	default:
	  printf("Unsupported type %d\n", st->type);
	  break;
	}
      }

      g_free(key);
      st++;
    }

    /* restore wms server list */
    const gchar *countkey = "/apps/" PACKAGE "/wms/count";
    GConfValue *value = gconf_client_get(client, countkey, NULL);
    if(value) {
      gconf_value_free(value);

      int i, count = gconf_client_get_int(client, countkey, NULL);

      wms_server_t **cur = &settings->wms_server;
      for(i=0;i<count;i++) {
	gchar *key = g_strdup_printf("/apps/" PACKAGE "/wms/name%d", i);
	char *name = gconf_client_get_string(client, key, NULL);
	g_free(key);
	key = g_strdup_printf("/apps/" PACKAGE "/wms/server%d", i);
	char *server = gconf_client_get_string(client, key, NULL);
	g_free(key);
	key = g_strdup_printf("/apps/" PACKAGE "/wms/path%d", i);
	char *path = gconf_client_get_string(client, key, NULL);
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

    /* ------------- get proxy settings -------------------- */
    if(gconf_client_get_bool(client, PROXY_KEY "use_http_proxy", NULL)) {
      proxy_t *proxy = settings->proxy = g_new0(proxy_t, 1);

      /* get basic settings */
      proxy->host = gconf_client_get_string(client, PROXY_KEY "host", NULL);
      proxy->port = gconf_client_get_int(client, PROXY_KEY "port", NULL);
      proxy->ignore_hosts =
	gconf_client_get_string(client, PROXY_KEY "ignore_hosts", NULL);

      /* check for authentication */
      proxy->use_authentication =
	gconf_client_get_bool(client, PROXY_KEY "use_authentication", NULL);

      if(proxy->use_authentication) {
	proxy->authentication_user =
	  gconf_client_get_string(client, PROXY_KEY "authentication_user", NULL);
	proxy->authentication_password =
	  gconf_client_get_string(client, PROXY_KEY "authentication_password",
				  NULL);
      }
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
  }


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
    case STORE_STRING:
      if((char*)(*ptr))
	gconf_client_set_string(client, key, (char*)(*ptr), NULL);
      else
	gconf_client_unset(client, key, NULL);
      break;

    case STORE_BOOL:
      gconf_client_set_bool(client, key, *((int*)ptr), NULL);
      break;

    case STORE_INT:
      gconf_client_set_int(client, key, *((int*)ptr), NULL);
      break;

    case STORE_FLOAT:
      gconf_client_set_float(client, key, *((float*)ptr), NULL);
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
    char *key = g_strdup_printf("/apps/" PACKAGE "/wms/name%d", count);
    gconf_client_set_string(client, key, cur->name, NULL);
    g_free(key);
    key = g_strdup_printf("/apps/" PACKAGE "/wms/server%d", count);
    gconf_client_set_string(client, key, cur->server, NULL);
    g_free(key);
    key = g_strdup_printf("/apps/" PACKAGE "/wms/path%d", count);
    gconf_client_set_string(client, key, cur->path, NULL);
    g_free(key);

    count++;
    cur = cur->next;
  }

  gconf_client_set_int(client, "/apps/" PACKAGE "/wms/count", count, NULL);
}

void settings_free(settings_t *settings) {
  store_t *st = store;

  wms_servers_free(settings->wms_server);

  while(st->key) {
    void **ptr = ((void*)settings) + st->offset;

    if(st->type == STORE_STRING)
      g_free(*ptr);

    st++;
  }

  /* free proxy settings if present */
  if(settings->proxy) {
    proxy_t *proxy = settings->proxy;

    g_free(proxy->host);
    g_free(proxy->ignore_hosts);
    g_free(proxy->authentication_user);
    g_free(proxy->authentication_password);

    g_free(proxy);
  }

  g_free(settings);
}
