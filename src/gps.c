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

#include "gps.h"

#include "appdata.h"
#include "settings.h"

#include <libgnomevfs/gnome-vfs-inet-connection.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#ifdef ENABLE_GPSBT
#include <gpsbt.h>
#include <gpsmgr.h>
#include <errno.h>
#endif

#define MAXTAGLEN    8       /* maximum length of sentence tag name */

struct gps_fix_t {
    int    mode;	/* Mode of fix */
#define MODE_NOT_SEEN	0	/* mode update not seen yet */
#define MODE_NO_FIX	1	/* none */
#define MODE_2D  	2	/* good for latitude/longitude */
#define MODE_3D  	3	/* good for altitude/climb too */
    pos_t pos;          /* Latitude/Longitude in degrees (valid if mode >= 2) */
    double alt;
    double eph;  	/* Horizontal position uncertainty, meters */
};

typedef unsigned int gps_mask_t;

struct gps_data_t {
    gps_mask_t set;	/* has field been set since this was last cleared? */
#define LATLON_SET	0x00000008u
#define ALTITUDE_SET	0x00000010u
#define STATUS_SET	0x00000100u
#define MODE_SET	0x00000200u
#define SATELLITE_SET	0x00040000u

    struct gps_fix_t	fix;		/* accumulated PVT data */

    /* GPS status -- always valid */
    int    status;		/* Do we have a fix? */
#define STATUS_NO_FIX	0	/* no */
#define STATUS_FIX	1	/* yes, without DGPS */
#define STATUS_DGPS_FIX	2	/* yes, with DGPS */

};

/* setup for direct gpsd based communication */
struct gps_state_t {
  /* when using liblocation, events are generated on position change */
  /* and no seperate timer is required */
  guint handler_id;

#ifdef ENABLE_GPSBT
  gpsbt_t context;
#endif

  GThread* thread_p;
  GMutex *mutex;
  GnomeVFSInetConnection *iconn;
  GnomeVFSSocket *socket;

  struct gps_data_t gpsdata;

#if GLIB_CHECK_VERSION(2,32,0)
  GMutex rmutex;
#endif
};

/* maybe user configurable later on ... */
#define GPSD_HOST "127.0.0.1"
#define GPSD_PORT 2947

gboolean gps_get_pos(gps_state_t *gps_state, pos_t *pos, float *alt) {
  pos_t tmp;
  if(!pos) pos = &tmp;
  pos->lat = NAN;

  g_mutex_lock(gps_state->mutex);
  if(gps_state->gpsdata.set & STATUS_SET) {
    if(gps_state->gpsdata.status != STATUS_NO_FIX) {
      if(gps_state->gpsdata.set & LATLON_SET)
	*pos = gps_state->gpsdata.fix.pos;
      if(alt && gps_state->gpsdata.set & ALTITUDE_SET)
	*alt = gps_state->gpsdata.fix.alt;
    }
  }

  g_mutex_unlock(gps_state->mutex);

  return(!isnan(pos->lat));
}

static int gps_connect(gps_state_t *gps_state) {
  GnomeVFSResult vfs_result;
#ifdef ENABLE_GPSBT
  char errstr[256] = "";

  /* We need to start gpsd (via gpsbt) first. */
  memset(&gps_state->context, 0, sizeof(gps_state->context));
  errno = 0;

  if(gpsbt_start(NULL, 0, 0, 0, errstr, sizeof(errstr),
		 0, &gps_state->context) < 0) {
    printf("Error connecting to GPS receiver: (%d) %s (%s)\n",
	   errno, strerror(errno), errstr);
  }
#endif

  /************** from here down pure gnome/gtk/gpsd ********************/

  /* try to connect to gpsd */
  /* Create a socket to interact with GPSD. */

  printf("GPSD: trying to connect to %s %d\n", GPSD_HOST, GPSD_PORT);

  int retries = 5;
  while(retries &&
	(GNOME_VFS_OK != (vfs_result = gnome_vfs_inet_connection_create(
		&gps_state->iconn, GPSD_HOST, GPSD_PORT, NULL)))) {
    printf("Error creating connection to GPSD, retrying ...\n");

    retries--;
    sleep(1);
  }

  if(!retries) {
    printf("Finally failed ...\n");
    return -1;
  }

  retries = 5;
  while(retries && ((gps_state->socket =
     gnome_vfs_inet_connection_to_socket(gps_state->iconn)) == NULL)) {
    printf("Error creating connecting GPSD socket, retrying ...\n");

    retries--;
    sleep(1);
  }

  if(!retries) {
    printf("Finally failed ...\n");
    gnome_vfs_inet_connection_destroy(gps_state->iconn, NULL);
    return -1;
  }

  GTimeVal timeout = { 10, 0 };
  if(GNOME_VFS_OK != (vfs_result = gnome_vfs_socket_set_timeout(
	gps_state->socket, &timeout, NULL))) {
    printf("Error setting GPSD timeout\n");
    gnome_vfs_inet_connection_destroy(gps_state->iconn, NULL);
    return -1;
  }

  printf("GPSD connected ...\n");

  return 0;
}

static void gps_clear_fix(struct gps_fix_t *fixp) {
  fixp->mode = MODE_NOT_SEEN;
  fixp->pos.lat = fixp->pos.lon = NAN;
  fixp->alt = NAN;
  fixp->eph = NAN;
}

/* unpack a daemon response into a status structure */
static void gps_unpack(char *buf, struct gps_data_t *gpsdata) {
  char *ns, *sp, *tp;

  for(ns = strstr(buf,"GPSD"); ns; ns = strstr(ns+1, "GPSD")) {
    /* the following should execute each time we have a good next sp */
    for (sp = ns + 5; *sp != '\0'; sp = tp+1) {
      tp = sp + strcspn(sp, ",\r\n");
      if (*tp == '\0')
        tp--;
      else
        *tp = '\0';

      if (*sp == '0') {
	if (sp[2] == '?') {
	  gpsdata->set =
	    (gpsdata->set & SATELLITE_SET) | // fix for below
	    MODE_SET | STATUS_SET;  // this clears sat info??
	  gpsdata->status = STATUS_NO_FIX;
	  gps_clear_fix(&gpsdata->fix);
	} else {
	  struct gps_fix_t nf;
	  char tag[MAXTAGLEN+1], alt[20], eph[20], lat[20], lon[20], mode[2];
	  int st = sscanf(sp+2,
			  "%8s %*s %*s %19s %19s "
			  "%19s %19s %*s %*s %*s %*s "
			  "%*s %*s %*s %1s",
			  tag, lat, lon,
			  alt, eph,
			  mode);
	  if (st >= 5) {
#define DEFAULT(val) (val[0] == '?') ? NAN : g_ascii_strtod(val, NULL)
	    nf.pos.lat = DEFAULT(lat);
	    nf.pos.lon = DEFAULT(lon);
	    nf.eph = DEFAULT(eph);
	    nf.alt = DEFAULT(alt);
#undef DEFAULT
	    if (st >= 6)
	      nf.mode = (mode[0] == '?') ? MODE_NOT_SEEN : atoi(mode);
	    else
	      nf.mode = (alt[0] == '?') ? MODE_2D : MODE_3D;
	    gpsdata->fix = nf;
	    gpsdata->set |= LATLON_SET|MODE_SET;
	    gpsdata->status = STATUS_FIX;
	    gpsdata->set |= STATUS_SET;

	    if(alt[0] != '?')
	      gpsdata->set |= ALTITUDE_SET;
	  }
	}
      }
    }
  }
}

void gps_enable(G_GNUC_UNUSED gps_state_t *gps_state, G_GNUC_UNUSED gboolean enable) {
}

gpointer gps_thread(gpointer data) {
  GnomeVFSFileSize bytes_read;
  char str[512];
  appdata_t *appdata = (appdata_t*)data;

  const char *msg = "o\r\n";   /* pos request */

  gps_state_t * const gps_state = appdata->gps_state;
  gps_state->gpsdata.set = 0;

  gboolean connected = FALSE;

  while(1) {
    if(appdata->settings && appdata->settings->enable_gps) {
      if(!connected) {
	printf("trying to connect\n");

	if(gps_connect(gps_state) < 0)
	  sleep(10);
	else
	  connected = TRUE;
      } else if(gnome_vfs_socket_write(gps_state->socket,
		msg, strlen(msg)+1, &bytes_read, NULL) == GNOME_VFS_OK) {

	/* update every second, wait here to make sure a complete */
	/* reply is received */
	sleep(1);

	if(bytes_read == (strlen(msg)+1)) {
	  if(gnome_vfs_socket_read(gps_state->socket,
	     str, sizeof(str)-1, &bytes_read, NULL) == GNOME_VFS_OK) {
	    str[bytes_read] = 0;

	    printf("msg: %s (%zu)\n", str, strlen(str));

	    g_mutex_lock(gps_state->mutex);

	    gps_state->gpsdata.set &=
	      ~(LATLON_SET|MODE_SET|STATUS_SET);

	    gps_unpack(str, &gps_state->gpsdata);
	    g_mutex_unlock(gps_state->mutex);
	  }
	}
      }
    } else {
      if(connected) {
	printf("stopping GPS connection due to user request\n");
	gnome_vfs_inet_connection_destroy(gps_state->iconn, NULL);

#ifdef ENABLE_GPSBT
	gpsbt_stop(&gps_state->context);
#endif
	connected = FALSE;
      } else
	sleep(1);
    }
  }

  printf("GPS thread ended???\n");
  return NULL;
}

gps_state_t *gps_init(appdata_t *appdata) {
  gps_state_t *gps_state = g_new0(gps_state_t, 1);

  printf("GPS init: Using gpsd\n");

  /* start a new thread to listen to gpsd */
#if GLIB_CHECK_VERSION(2,32,0)
  gps_state->mutex = &gps_state->rmutex;
  g_mutex_init(gps_state->mutex);
  gps_state->thread_p =
    g_thread_try_new("gps", gps_thread, appdata, NULL);
#else
  gps_state->mutex = g_mutex_new();
  gps_state->thread_p =
    g_thread_create(gps_thread, appdata, FALSE, NULL);
#endif

  return gps_state;
}

void gps_release(gps_state_t *gps_state) {
  gps_register_callback(gps_state, NULL, NULL);
#ifdef ENABLE_GPSBT
  gpsbt_stop(&gps_state->context);
#endif
#if GLIB_CHECK_VERSION(2,32,0)
  g_mutex_clear(gps_state->mutex);
  if (gps_state->thread_p)
    g_thread_unref(gps_state->thread_p);
#else
  g_mutex_free(gps_state->mutex);
#endif
  g_free(gps_state);
}

gboolean gps_register_callback(gps_state_t *gps_state, GtkFunction cb, gpointer context) {
  if(gps_state->handler_id) {
    if(cb == NULL) {
      g_source_remove(gps_state->handler_id);
      gps_state->handler_id = 0;
    }
    return TRUE;
  } else {
    if(cb != NULL) {
      gps_state->handler_id = g_timeout_add_seconds(1, cb, context);
    }
    return FALSE;
  }
}
