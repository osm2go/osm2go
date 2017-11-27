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

#include "net_io.h"

#include "misc.h"
#include "osm2go_platform.h"

#include <cassert>
#include <cstring>
#include <curl/curl.h>
#include <curl/easy.h>  /* new for v7 */
#include <map>
#include <memory>
#include <string>
#include <unistd.h>

#include "osm2go_annotations.h"
#include <osm2go_cpp.h>
#include <osm2go_i18n.h>
#include "osm2go_stl.h"

struct curl_mem_t {
  char *ptr;
  size_t len;
};

/* structure shared between worker and master thread */
struct net_io_request_t {
  net_io_request_t(const std::string &u, const std::string &f);
  net_io_request_t(const std::string &u, curl_mem_t *cmem) __attribute__((nonnull(3)));

  gint refcount;       /* reference counter for master and worker thread */

  const std::string url;
  bool cancel;
  curl_off_t download_cur;
  curl_off_t download_end;

  /* curl/http related stuff: */
  CURLcode res;
  long response;
  char buffer[CURL_ERROR_SIZE];

  /* request specific fields */
  const std::string filename;   /* used for NET_IO_DL_FILE */
  curl_mem_t * const mem;   /* used for NET_IO_DL_MEM */
  bool use_compression;
};

typedef std::map<int, const char *> HttpCodeMap;

static HttpCodeMap http_msg_init() {
  HttpCodeMap http_messages;

  http_messages[200] = "Ok";
  http_messages[203] = "No Content";
  http_messages[301] = "Moved Permenently";
  http_messages[302] = "Moved Temporarily";
  http_messages[400] = "Bad Request";
  http_messages[401] = "Unauthorized";
  http_messages[403] = "Forbidden";
  http_messages[404] = "Not Found";
  http_messages[405] = "Method Not Allowed";
  http_messages[409] = "Conflict";
  http_messages[410] = "Gone";
  http_messages[412] = "Precondition Failed";
  http_messages[417] = "(Expect rejected)";
  http_messages[500] = "Internal Server Error";
  http_messages[503] = "Service Unavailable";

  return http_messages;
}

const char *http_message(int id) {
  static const HttpCodeMap http_messages = http_msg_init();

  const HttpCodeMap::const_iterator it = http_messages.find(id);
  if(likely(it != http_messages.end()))
    return it->second;

  return O2G_NULLPTR;
}

static gint dialog_destroy_event(bool *data) {
  /* set cancel flag */
  *data = true;
  return FALSE;
}

static void on_cancel(bool *data) {
  /* set cancel flag */
  *data = true;
}

/* create the dialog box shown while worker is running */
static GtkWidget *busy_dialog(GtkWidget *parent, GtkProgressBar **pbar,
			      bool *cancel_ind, const char *title) {
#ifdef GTK_DIALOG_NO_SEPARATOR
  GtkWidget *dialog = gtk_dialog_new_with_buttons(O2G_NULLPTR, O2G_NULLPTR, GTK_DIALOG_NO_SEPARATOR);
#else
  GtkWidget *dialog = gtk_dialog_new();
#endif

  if(title) {
    g_string str(g_strdup_printf(_("Downloading %s"), title));
    gtk_window_set_title(GTK_WINDOW(dialog), str.get());
  } else
    gtk_window_set_title(GTK_WINDOW(dialog), _("Downloading"));
  gtk_window_set_default_size(GTK_WINDOW(dialog), 300, 10);

  gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
  gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(parent));

  assert(pbar != O2G_NULLPTR);
  /* extra cast as the version used in Maemo returns GtkWidget for whatever reason */
  *pbar = GTK_PROGRESS_BAR(gtk_progress_bar_new());
  gtk_progress_bar_set_pulse_step(*pbar, 0.1);

  gtk_box_pack_start_defaults(GTK_BOX(GTK_DIALOG(dialog)->vbox), GTK_WIDGET(*pbar));

  GtkWidget *button = button_new_with_label(_("Cancel"));
  g_signal_connect_swapped(GTK_OBJECT(button), "clicked",
                           G_CALLBACK(on_cancel), cancel_ind);
  gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->action_area), button);

  g_signal_connect_swapped(GTK_OBJECT(dialog), "destroy",
                           G_CALLBACK(dialog_destroy_event), cancel_ind);

  gtk_widget_show_all(dialog);

  return dialog;
}

net_io_request_t::net_io_request_t(const std::string &u, const std::string &f)
  : refcount(1)
  , url(u)
  , cancel(false)
  , download_cur(0)
  , download_end(0)
  , res(CURLE_OK)
  , response(0)
  , filename(f)
  , mem(O2G_NULLPTR)
  , use_compression(true)
{
  assert(!filename.empty());
  memset(buffer, 0, sizeof(buffer));
}

net_io_request_t::net_io_request_t(const std::string &u, curl_mem_t *cmem)
  : refcount(1)
  , url(u)
  , cancel(false)
  , download_cur(0)
  , download_end(0)
  , res(CURLE_OK)
  , response(0)
  , mem(cmem)
  , use_compression(false)
{
  memset(buffer, 0, sizeof(buffer));
}

struct request_free {
  void operator()(net_io_request_t *request);
};

void request_free::operator()(net_io_request_t *request)
{
  /* decrease refcount and only free structure if no references are left */
  assert_cmpnum_op(request->refcount, >, 0);
  request->refcount--;
  if(request->refcount) {
    printf("still %d references, keeping request\n", request->refcount);
    return;
  }

  printf("no references left, freeing request\n");
  delete request;
}

static int curl_progress_func(void *req,
			    curl_off_t dltotal, curl_off_t dlnow,
                            curl_off_t, curl_off_t) {
  net_io_request_t *request = static_cast<net_io_request_t *>(req);
  request->download_cur = dlnow;
  request->download_end = dltotal;
  return 0;
}

static size_t mem_write(void *ptr, size_t size, size_t nmemb,
			void *stream) {
  curl_mem_t *p = static_cast<curl_mem_t *>(stream);

  size_t nlen = p->len + size * nmemb;
  p->ptr = static_cast<char *>(g_realloc(p->ptr, nlen + 1));
  memcpy(p->ptr + p->len, ptr, size * nmemb);
  p->len = nlen;
  p->ptr[nlen] = '\0';
  return nmemb;
}

static void *worker_thread(void *ptr) {
  std::unique_ptr<net_io_request_t, request_free> request(static_cast<net_io_request_t *>(ptr));

  printf("thread: running\n");

  CURL *curl = curl_easy_init();
  if(likely(curl != O2G_NULLPTR)) {
    bool ok = false;
    FILE *outfile = O2G_NULLPTR;

    /* prepare target (file, memory, ...) */
    if(!request->filename.empty()) {
      outfile = fopen(request->filename.c_str(), "w");
      ok = (outfile != O2G_NULLPTR);
      curl_easy_setopt(curl, CURLOPT_WRITEDATA, outfile);
    } else {
      request->mem->ptr = O2G_NULLPTR;
      request->mem->len = 0;
      curl_easy_setopt(curl, CURLOPT_WRITEDATA, request->mem);
      curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, mem_write);
      ok = true;
    }

    if(likely(ok)) {
      curl_easy_setopt(curl, CURLOPT_URL, request->url.c_str());

      /* setup progress notification */
      curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
      curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, curl_progress_func);
      curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, request.get());

      curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, request->buffer);

      curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1l);

      /* play nice and report some user agent */
      curl_easy_setopt(curl, CURLOPT_USERAGENT, PACKAGE "-libcurl/" VERSION);

#ifndef CURL_SSLVERSION_MAX_DEFAULT
#define CURL_SSLVERSION_MAX_DEFAULT 0
#endif
      curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1 |
                       CURL_SSLVERSION_MAX_DEFAULT);

      struct curl_slist *slist = !request->use_compression ? O2G_NULLPTR :
                                 curl_slist_append(O2G_NULLPTR, "Accept-Encoding: gzip");
      if(slist != O2G_NULLPTR)
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);

      request->res = curl_easy_perform(curl);
      printf("thread: curl perform returned with %d\n", request->res);

      if(slist != O2G_NULLPTR)
        curl_slist_free_all(slist);
      curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &request->response);

#if 0
      /* try to read "Error" */
      struct curl_slist *slist = O2G_NULLPTR;
      slist = curl_slist_append(slist, "Error:");
      curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);
#endif

      if(outfile != O2G_NULLPTR)
	fclose(outfile);
    }

    /* always cleanup */
    curl_easy_cleanup(curl);
  } else
    printf("thread: unable to init curl\n");

  printf("thread: io done\n");

  printf("thread: terminating\n");
  return O2G_NULLPTR;
}

static bool net_io_do(GtkWidget *parent, net_io_request_t *rq,
                      const char *title) {
  /* the request structure is shared between master and worker thread. */
  /* typically the master thread will do some waiting until the worker */
  /* thread returns. But the master may very well stop waiting since e.g. */
  /* the user activated some cancel button. The client will learn this */
  /* from the fact that it's holding the only reference to the request */

  /* create worker thread */
  assert_cmpnum(rq->refcount, 1);
  rq->refcount = 2;   // master and worker hold a reference
  std::unique_ptr<net_io_request_t, request_free> request(rq);
  GtkProgressBar *pbar = O2G_NULLPTR;
  GtkWidget *dialog = busy_dialog(parent, &pbar, &rq->cancel, title);

  GThread *worker;

#if GLIB_CHECK_VERSION(2,32,0)
  worker = g_thread_try_new("download", worker_thread, request.get(), O2G_NULLPTR);
#else
  worker = g_thread_create(worker_thread, request.get(), FALSE, O2G_NULLPTR);
#endif
  if(unlikely(worker == O2G_NULLPTR)) {
    g_warning("failed to create the worker thread");

    /* free request and return error */
    gtk_widget_destroy(dialog);
    rq->refcount = 1;
    return false;
  }

  /* wait for worker thread */
  curl_off_t last = 0;
  while(request->refcount > 1 && !request->cancel) {
    osm2go_platform::process_events();

    /* worker has made progress changed the progress value */
    if(request->download_cur != last) {
      if(request->download_end != 0) {
        gdouble progress = static_cast<gdouble>(request->download_cur) / request->download_end;
        gtk_progress_bar_set_fraction(pbar, progress);
      } else {
        gtk_progress_bar_pulse(pbar);
      }

      char buf[G_ASCII_DTOSTR_BUF_SIZE];
      snprintf(buf, sizeof(buf), "%" CURL_FORMAT_CURL_OFF_T, request->download_cur);
      gtk_progress_bar_set_text(pbar, buf);
    }

    usleep(100000);
  }

#if GLIB_CHECK_VERSION(2,32,0)
  g_thread_unref(worker);
#endif
  gtk_widget_destroy(dialog);

  /* user pressed cancel */
  if(request->refcount > 1) {
    printf("operation cancelled, leave worker alone\n");
    return false;
  }

  printf("worker thread has ended\n");

  /* --------- evaluate result --------- */

  /* the http connection itself may have failed */
  if(request->res != 0) {
    errorf(parent, _("Download failed with message:\n\n%s"), request->buffer);
    return false;
  }

  /* a valid http connection may have returned an error */
  if(request->response != 200) {
    errorf(parent, _("Download failed with code %ld:\n\n%s\n"),
	   request->response, http_message(request->response));
    return false;
  }

  return true;
}

bool net_io_download_file(GtkWidget *parent,
                          const std::string &url, const std::string &filename,
                          const char *title, bool compress) {
  net_io_request_t *request = new net_io_request_t(url, filename);

  request->use_compression = compress;

  printf("net_io: download %s to file %s\n", url.c_str(), filename.c_str());

  bool result = net_io_do(parent, request, title);
  if(!result) {

    /* remove the file that may have been written by now. the kernel */
    /* should cope with the fact that the worker thread may still have */
    /* an open reference to this and might thus still write to this file. */
    /* letting the worker delete the file is worse since it may take the */
    /* worker some time to come to the point to delete this file. If the */
    /* user has restarted the download by then, the worker will erase that */
    /* newly written file */

    printf("request failed, deleting %s\n", filename.c_str());
    unlink(filename.c_str());
  } else
    printf("request ok\n");

  return result;
}

bool net_io_download_mem(GtkWidget *parent,
                         const std::string &url, char **mem, size_t &len) {
  curl_mem_t cmem;
  net_io_request_t *request = new net_io_request_t(url, &cmem);

  printf("net_io: download %s to memory\n", url.c_str());

  bool result = net_io_do(parent, request, O2G_NULLPTR);
  if(result) {
    printf("ptr = %p, len = %zu\n", cmem.ptr, cmem.len);
    *mem = cmem.ptr;
    len = cmem.len;
  }

  return result;
}

bool check_gzip(const char* mem, const size_t len)
{
  return len > 2 && mem[0] == 0x1f && static_cast<unsigned char>(mem[1]) == 0x8b;
}
