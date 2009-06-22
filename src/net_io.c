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

#include <curl/curl.h>
#include <curl/types.h> /* new for v7 */
#include <curl/easy.h>  /* new for v7 */
#include <unistd.h>

static struct http_message_s {
  int id;
  char *msg;
} http_messages [] = {
  {   0, "Curl internal failure" },
  { 200, "Ok" },
  { 400, "Bad Request" },
  { 401, "Unauthorized" },
  { 403, "Forbidden" },
  { 404, "Not Found" },
  { 405, "Method Not Allowed" },
  { 410, "Gone" },
  { 412, "Precondition Failed" },
  { 417, "(Expect rejected)" },
  { 500, "Internal Server Error" },
  { 503, "Service Unavailable" },
  { 0,   NULL }
};

typedef struct {
  char *ptr;
  int len;
} curl_mem_t;

typedef enum { NET_IO_DL_FILE, NET_IO_DL_MEM, NET_IO_DELETE } net_io_type_t;

/* structure shared between worker and master thread */
typedef struct {
  net_io_type_t type;
  gint refcount;       /* reference counter for master and worker thread */ 

  char *url, *user;
  gboolean cancel;
  float progress;

  /* curl/http related stuff: */
  CURLcode res;
  long response;
  char buffer[CURL_ERROR_SIZE];

  /* request specific fields */
  union {
    char *filename;   /* used for NET_IO_DL_FILE */
    curl_mem_t mem;   /* used for NET_IO_DL_MEM */
  };

  /* system proxy settings if present */
  proxy_t *proxy;

} net_io_request_t;

static char *http_message(int id) {
  struct http_message_s *msg = http_messages;

  while(msg->id) {
    if(msg->id == id) return _(msg->msg);
    msg++;
  }

  return NULL;
}

static gint dialog_destroy_event(GtkWidget *widget, gpointer data) {
  /* set cancel flag */
  *(gboolean*)data = TRUE;
  return FALSE;
}

static void on_cancel(GtkWidget *widget, gpointer data) {
  /* set cancel flag */
  *(gboolean*)data = TRUE;
}

/* create the dialog box shown while worker is running */
static GtkWidget *busy_dialog(GtkWidget *parent, GtkWidget **pbar,
			      gboolean *cancel_ind) {
  GtkWidget *dialog = gtk_dialog_new();

  gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
  gtk_window_set_title(GTK_WINDOW(dialog), _("Downloading"));
  gtk_window_set_default_size(GTK_WINDOW(dialog), 300, 10);

  gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
  gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(parent));

  g_assert(pbar);
  *pbar = gtk_progress_bar_new();
  gtk_progress_bar_set_pulse_step(GTK_PROGRESS_BAR(*pbar), 0.1);

  gtk_box_pack_start_defaults(GTK_BOX(GTK_DIALOG(dialog)->vbox), *pbar);

  GtkWidget *button = gtk_button_new_with_label(_("Cancel"));
  gtk_signal_connect(GTK_OBJECT(button), "clicked",
		     GTK_SIGNAL_FUNC(on_cancel), (gpointer)cancel_ind);
  gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->action_area), button);

  gtk_signal_connect(GTK_OBJECT(dialog), "destroy", 
	     G_CALLBACK(dialog_destroy_event), (gpointer)cancel_ind);

  gtk_widget_show_all(dialog);

  return dialog;
}

static void request_free(net_io_request_t *request) {
  /* decrease refcount and only free structure if no references are left */
  request->refcount--;
  if(request->refcount) {
    printf("still %d references, keeping request\n", request->refcount);
    return;
  }

  printf("no references left, freeing request\n");
  if(request->url)  g_free(request->url);
  if(request->user) g_free(request->user);

  /* filename is only a valid filename in NET_IO_DL_FILE mode */
  if(request->type == NET_IO_DL_FILE && request->filename)
    g_free(request->filename);

  g_free(request);
}

static int curl_progress_func(net_io_request_t *request,
			    double t, /* dltotal */ double d, /* dlnow */
			    double ultotal, double ulnow) {
  request->progress = t?d/t:0;
  return 0;
}

static size_t mem_write(void *ptr, size_t size, size_t nmemb, 
			void *stream) {
  curl_mem_t *p = (curl_mem_t*)stream;
  
  p->ptr = g_realloc(p->ptr, p->len + size*nmemb + 1);
  if(p->ptr) {
    memcpy(p->ptr+p->len, ptr, size*nmemb);
    p->len += size*nmemb;
    p->ptr[p->len] = 0;
  }
  return nmemb;
}

void net_io_set_proxy(CURL *curl, proxy_t *proxy) {
  if(proxy) {
    if(proxy->ignore_hosts) 
      printf("WARNING: Pproxy \"ignore_hosts\" unsupported!\n");

    printf("net_io: using proxy %s:%d\n", proxy->host, proxy->port);

    curl_easy_setopt(curl, CURLOPT_PROXY, proxy->host);
    curl_easy_setopt(curl, CURLOPT_PROXYPORT, proxy->port);

    if(proxy->use_authentication) {
      printf("net_io:   use auth for user %s\n", proxy->authentication_user);
      
      char *cred = g_strdup_printf("%s:%s", 
				   proxy->authentication_user,
				   proxy->authentication_password);

      curl_easy_setopt(curl, CURLOPT_PROXYUSERPWD, cred);
      g_free(cred);
    }
  }
}

static void *worker_thread(void *ptr) {
  net_io_request_t *request = (net_io_request_t*)ptr;
  
  printf("thread: running\n");

  CURL *curl = curl_easy_init();
  if(curl) {
    FILE *outfile = NULL;
    gboolean ok = FALSE;

    /* prepare target (file, memory, ...) */
    switch(request->type) {
    case NET_IO_DL_FILE:
      outfile = fopen(request->filename, "w");
      ok = (outfile != NULL);
      break;

    case NET_IO_DL_MEM:
      request->mem.ptr = NULL;
      request->mem.len = 0;
      ok = TRUE;
      break;

    default:
      printf("thread: unsupported request\n");
      /* ugh?? */
      ok = TRUE;
      break;
    }

    if(ok) {
      curl_easy_setopt(curl, CURLOPT_URL, request->url);
      
      switch(request->type) {
      case NET_IO_DL_FILE:
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, outfile);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, fwrite);
	break;

      case NET_IO_DL_MEM:
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &request->mem);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, mem_write);
	break;
	
      case NET_IO_DELETE:
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE"); 
	break;
      }

      net_io_set_proxy(curl, request->proxy);

      /* set user name and password for the authentication */
      if(request->user)
	curl_easy_setopt(curl, CURLOPT_USERPWD, request->user);

      /* setup progress notification */
      curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
      curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, curl_progress_func);
      curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, request);

      curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, request->buffer);

      /* play nice and report some user agent */
      curl_easy_setopt(curl, CURLOPT_USERAGENT, PACKAGE "-libcurl/" VERSION); 
 
      request->res = curl_easy_perform(curl);
      printf("thread: curl perform returned with %d\n", request->res);
    
      curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &request->response);

      if(request->type == NET_IO_DL_FILE) 
	fclose(outfile);
    }

    /* always cleanup */
    curl_easy_cleanup(curl);
  } else
    printf("thread: unable to init curl\n");
  
  printf("thread: io done\n");
  request_free(request);

  printf("thread: terminating\n");
  return NULL;
}

static gboolean net_io_do(GtkWidget *parent, net_io_request_t *request) {
  /* the request structure is shared between master and worker thread. */
  /* typically the master thread will do some waiting until the worker */
  /* thread returns. But the master may very well stop waiting since e.g. */
  /* the user activated some cancel button. The client will learn this */
  /* from the fact that it's holding the only reference to the request */

  GtkWidget *pbar = NULL;
  GtkWidget *dialog = busy_dialog(parent, &pbar, &request->cancel);
  
  /* create worker thread */
  request->refcount = 2;   // master and worker hold a reference
  if(!g_thread_create(&worker_thread, request, FALSE, NULL) != 0) {
    g_warning("failed to create the worker thread");

    /* free request and return error */
    request->refcount--;    /* decrease by one for dead worker thread */
    gtk_widget_destroy(dialog);
    return FALSE;
  }

  /* wait for worker thread */
  float progress = 0;
  while(request->refcount > 1 && !request->cancel) {
    while(gtk_events_pending())
      gtk_main_iteration();

    /* worker has made progress changed the progress value */
    if(request->progress != progress) {
      gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(pbar), request->progress);
      progress = request->progress;
    } else 
      if(!progress) 
	gtk_progress_bar_pulse(GTK_PROGRESS_BAR(pbar));

    usleep(100000);
  }

  gtk_widget_destroy(dialog);

  /* user pressed cancel */
  if(request->refcount > 1) {
    printf("operation cancelled, leave worker alone\n");
    return FALSE;
  }

  printf("worker thread has ended\n");

  /* --------- evaluate result --------- */

  /* the http connection itself may have failed */
  if(request->res != 0) {
    errorf(parent, _("Download failed with message:\n\n%s"), request->buffer);
    return FALSE;
  }

  /* a valid http connection may have returned an error */
  if(request->response != 200) {
    errorf(parent, _("Download failed with code %ld:\n\n%s\n"), 
	   request->response, http_message(request->response));
    return FALSE;
  }

  return TRUE;
}

gboolean net_io_download_file(GtkWidget *parent, settings_t *settings, 
			      char *url, char *filename) {
  net_io_request_t *request = g_new0(net_io_request_t, 1);

  printf("net_io: download %s to file %s\n", url, filename);
  request->type = NET_IO_DL_FILE;
  request->url = g_strdup(url);
  request->filename = g_strdup(filename);
  if(settings && settings->proxy)
    request->proxy = settings->proxy;

  gboolean result = net_io_do(parent, request);
  if(!result) {

    /* remove the file that may have been written by now. the kernel */
    /* should cope with the fact that the worker thread may still have */
    /* an open reference to this and might thus still write to this file. */
    /* letting the worker delete the file is worse since it may take the */
    /* worker some time to come to the point to delete this file. If the */
    /* user has restarted the download by then, the worker will erase that */
    /* newly written file */

    g_remove(filename);
  }

  request_free(request);
  return result;
}


gboolean net_io_download_mem(GtkWidget *parent, settings_t *settings, 
			     char *url, char **mem) {
  net_io_request_t *request = g_new0(net_io_request_t, 1);

  printf("net_io: download %s to memory\n", url);
  request->type = NET_IO_DL_MEM;
  request->url = g_strdup(url);
  if(settings && settings->proxy)
    request->proxy = settings->proxy;

  gboolean result = net_io_do(parent, request);
  if(result) {
    printf("ptr = %p, len = %d\n", request->mem.ptr, request->mem.len);
    *mem = request->mem.ptr;
  }
  
  request_free(request);
  return result;
}
