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

#include "appdata.h"

#include <gconf/gconf.h>
#include <gconf/gconf-client.h>

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

  /* wms servers aren't yet saved as a major rewrite is required before */

  /* style */
  { "style",            STORE_STRING, OFFSET(style)        },

  /* main */
  { "no_icons",         STORE_BOOL,   OFFSET(no_icons)     },
  { "no_antialias",     STORE_BOOL,   OFFSET(no_antialias) },
  { "track_path",       STORE_STRING, OFFSET(track_path)   },
  { "enable_gps",       STORE_BOOL,   OFFSET(enable_gps)   },
  { "follow_gps",       STORE_BOOL,   OFFSET(follow_gps)   },

  { NULL, -1, -1 }
};

settings_t *settings_load(void) {
  settings_t *settings = g_new0(settings_t,1);

  /* ------ set useful defaults ------- */

#ifdef USE_HILDON
  char *p;
  settings->base_path = strdup(BASE_DIR);
#else
  char *p = getenv("HOME");
  g_assert(p);

  /* build image path in home directory */
  settings->base_path =
    malloc(strlen(p)+strlen(BASE_DIR)+2);
  strcpy(settings->base_path, p);
  if(settings->base_path[strlen(settings->base_path)-1] != '/')
    strcat(settings->base_path, "/");
  strcat(settings->base_path, BASE_DIR);
#endif

  /* ------------- setup download defaults -------------------- */
  settings->server = strdup("http://api.openstreetmap.org/api/0.5");
  if((p = getenv("OSM_USER")))
    settings->username = g_strdup(p);
  else
    settings->username = g_strdup(_("<your osm username>"));

  if((p = getenv("OSM_PASS")))
    settings->password = g_strdup(p);
  else
    settings->password = strdup("<password>");

  settings->style = g_strdup(DEFAULT_STYLE);


  /* ------ overwrite with settings from gconf if present ------- */
  GConfClient *client = gconf_client_get_default();
  if(client) {

#ifdef USE_HILDON
    /* special explanation for the no_icons setting on hildon/maemo */
    {
      char *key = g_strdup_printf("/apps/" PACKAGE "/no_icons");
      GConfValue *value = gconf_client_get(client, key, NULL);
      g_free(key);
      if(value) 
	gconf_value_free(value); 
      else {
	messagef(NULL, _("Icon drawing is disabled"),
		 _("You are running this version of osm2go on a Internet "
		   "Tablet for the first time. Since these currently have "
		   "problems displaying icons on the map, icons have been "
		   "disabled. You might enable them in the menu under "
		   "Map/No Icons at any time."));

	settings->no_icons = TRUE;
      }
    }
#endif
    
    /* restore everything listed in the store table */
    store_t *st = store;
    while(st->key) {
      void **ptr = ((void*)settings) + st->offset;
      char *key = g_strdup_printf("/apps/" PACKAGE "/%s", st->key);

      /* check if key is present */
      GConfValue *value = gconf_client_get(client, key, NULL);
      if(value) {
	gconf_value_free(value); 
      
	switch(st->type) {
	case STORE_STRING: {
	  char **str = (char**)ptr;
	  if(*str) g_free(*str);
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
  }

  /* restore wms server list */
  char *key = g_strdup_printf("/apps/" PACKAGE "/wms/count");
  GConfValue *value = gconf_client_get(client, key, NULL);
  if(value) {
    gconf_value_free(value); 

    int i, count = gconf_client_get_int(client, key, NULL);
    g_free(key);

    wms_server_t **cur = &settings->wms_server;
    for(i=0;i<count;i++) {
      key = g_strdup_printf("/apps/" PACKAGE "/wms/name%d", i);
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
	if(name) g_free(name);
	if(server) g_free(server);
	if(path) g_free(path);
      }
    }
  } else {
    g_free(key);

    /* add default server(s) */
    printf("No WMS servers configured, adding default\n");
    settings->wms_server = wms_server_get_default();
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
    char *key = g_strdup_printf("/apps/" PACKAGE "/%s", st->key);

    switch(st->type) {
    case STORE_STRING: 
      if((char*)(*ptr)) {
	gconf_client_set_string(client, key, (char*)(*ptr), NULL);
      }
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

  char *key = g_strdup_printf("/apps/" PACKAGE "/wms/count");
  gconf_client_set_int(client, key, count, NULL);
  g_free(key);
}

void settings_free(settings_t *settings) {
  store_t *st = store;

  wms_servers_free(settings->wms_server);
  
  while(st->key) {
    void **ptr = ((void*)settings) + st->offset;

    if(st->type == STORE_STRING)
      if((char*)(*ptr)) 
	g_free((char*)(*ptr));
	
    st++;
  }

  g_free(settings);
}
