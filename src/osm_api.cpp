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

#include "osm_api.h"

#include "appdata.h"
#include "diff.h"
#include "map.h"
#include "misc.h"
#include "net_io.h"
#include "project.h"
#include "settings.h"

#include <algorithm>
#include <curl/curl.h>
#include <curl/easy.h> /* new for v7 */
#include <map>
#include <unistd.h>

#ifdef FREMANTLE
#include <hildon/hildon-text-view.h>
#endif

#include <osm2go_cpp.h>

#define COLOR_ERR  "red"
#define COLOR_OK   "darkgreen"

#define NO_EXPECT

static std::map<int, const char *> http_msg_init() {
  std::map<int, const char *> http_messages;

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

static const char *osm_http_message(int id) {
  static const std::map<int, const char *> http_messages = http_msg_init();

  const std::map<int, const char *>::const_iterator it = http_messages.find(id);
  if(it != http_messages.end())
    return it->second;

  return O2G_NULLPTR;
}

struct log_s {
  log_s()
    : buffer(O2G_NULLPTR), view(O2G_NULLPTR) {}

  GtkTextBuffer *buffer;
  GtkWidget *view;
};

struct osm_upload_context_t {
  osm_upload_context_t(appdata_t *a, osm_t *o, project_t *p, const char *c, const char *s)
    : appdata(a)
    , dialog(O2G_NULLPTR)
    , osm(o)
    , project(p)
    , urlbasestr(p->server)
    , changeset(0)
    , proxy(appdata->settings->proxy)
    , comment(c)
    , src(s ? s : std::string())
  {}

  appdata_t * const appdata;
  GtkWidget *dialog;
  osm_t * const osm;
  project_t * const project;
  const std::string urlbasestr;

  struct log_s log;

  item_id_t changeset;

  proxy_t * const proxy;
  std::string comment;
  std::string credentials;
  const std::string src;
};

/**
 * @brief adjust an API 0.5 url to 0.6
 * @param rserver configured server URL
 * @returns if the server was changed
 */
static bool api_adjust(std::string &rserver) {
  if(!rserver.empty()) {
    std::string::size_type pos = rserver.find("0.5");
    if(pos != std::string::npos) {
      rserver[pos + 2] = '6';
      return true;
    }
  }

  return false;
}

static std::string urlbase(const project_t *project) {
  std::string url = project->server;
  if(strncmp(url.c_str(), "http://", 7) == 0 && strstr(curl_version(), "OpenSSL/1") != O2G_NULLPTR) {
    printf("dynamically switching download to HTTPS\n");
    url.insert(4, "s");
  }
  return url;
}

bool osm_download(GtkWidget *parent, settings_t *settings, project_t *project)
{
  printf("download osm ...\n");

  g_assert_nonnull(project->server);

  /* check if server name contains string "0.5" and adjust it */
  if(!project->rserver.empty()) {
    if(api_adjust(project->rserver))
      messagef(parent, _("Server changed"),
               _("It seems your current project uses a server/protocol no "
               "longer in use by OSM. It has thus been changed to:\n\n%s"),
               project->server);

    /* server url should not end with a slash */
    if(project->rserver[project->rserver.size() - 1] == '/') {
      printf("removing trailing slash\n");
      project->rserver.erase(project->rserver.size() - 1);
    }

    if(project->rserver == settings->server) {
      project->rserver.clear();
      project->server = settings->server;
    }
  }

  char minlon[G_ASCII_DTOSTR_BUF_SIZE], minlat[G_ASCII_DTOSTR_BUF_SIZE];
  char maxlon[G_ASCII_DTOSTR_BUF_SIZE], maxlat[G_ASCII_DTOSTR_BUF_SIZE];

  g_ascii_formatd(minlon, sizeof(minlon), LL_FORMAT, project->min.lon);
  g_ascii_formatd(minlat, sizeof(minlat), LL_FORMAT, project->min.lat);
  g_ascii_formatd(maxlon, sizeof(maxlon), LL_FORMAT, project->max.lon);
  g_ascii_formatd(maxlat, sizeof(maxlat), LL_FORMAT, project->max.lat);

  const std::string url = urlbase(project) + "/map?bbox=" +
                          minlon + "," + minlat + "," +
                          maxlon +  "," +  maxlat;

  /* Download the new file to a new name. If something goes wrong then the
   * old file will still be in place to be opened. */
  const std::string update = project->path + "update.osm";
  g_remove(update.c_str());

  bool result = net_io_download_file(parent, settings, url.c_str(), update.c_str(),
                                         project->name.c_str()) == TRUE;

  /* if there's a new file use this from now on */
  if(result && g_file_test(update.c_str(), G_FILE_TEST_IS_REGULAR)) {
    printf("download ok, replacing previous file\n");

    if(project->osm[0] == '/') {
      g_rename(update.c_str(), project->osm.c_str());
    } else {
      const std::string fname = project->path + project->osm;
      g_rename(update.c_str(), fname.c_str());
    }

    result = TRUE;
  }

  return result;
}

typedef struct {
  char *ptr;
  size_t len;
} curl_data_t;

static size_t read_callback(void *ptr, size_t size, size_t nmemb, void *stream) {
  curl_data_t *p = (curl_data_t*)stream;

  //  printf("request to read %d items of size %d, pointer = %p\n",
  //  nmemb, size, p->ptr);

  if(nmemb*size > p->len)
    nmemb = p->len/size;

  memcpy(ptr, p->ptr, size*nmemb);
  p->ptr += size*nmemb;
  p->len -= size*nmemb;

  return nmemb;
}

static size_t write_callback(void *ptr, size_t size, size_t nmemb, void *stream) {
  curl_data_t *p = (curl_data_t*)stream;

  p->ptr = static_cast<char *>(g_realloc(p->ptr, p->len + size*nmemb + 1));
  if(p->ptr) {
    memcpy(p->ptr+p->len, ptr, size*nmemb);
    p->len += size*nmemb;
    p->ptr[p->len] = 0;
  }
  return nmemb;
}

static G_GNUC_PRINTF(3, 4) void appendf(struct log_s &log, const char *colname,
		    const char *fmt, ...) {
  va_list args;
  va_start( args, fmt );
  char *buf = g_strdup_vprintf(fmt, args);
  va_end( args );

  printf("%s", buf);

  GtkTextTag *tag = O2G_NULLPTR;
  if(colname)
    tag = gtk_text_buffer_create_tag(log.buffer, O2G_NULLPTR,
				     "foreground", colname,
				     O2G_NULLPTR);

  GtkTextIter end;
  gtk_text_buffer_get_end_iter(log.buffer, &end);
  if(tag)
    gtk_text_buffer_insert_with_tags(log.buffer, &end, buf, -1, tag, O2G_NULLPTR);
  else
    gtk_text_buffer_insert(log.buffer, &end, buf, -1);

  g_free(buf);

  gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(log.view),
			       &end, 0.0, FALSE, 0, 0);

  while(gtk_events_pending())
    gtk_main_iteration();
}

#define MAX_TRY 5

static bool osm_update_item(osm_upload_context_t &context, xmlChar *xml_str,
                            const char *url, item_id_t *id) {
  int retry = MAX_TRY;
  char buffer[CURL_ERROR_SIZE];

  CURL *curl;
  CURLcode res;

  curl_data_t read_data, read_data_init;
  curl_data_t write_data;
  read_data_init.ptr = reinterpret_cast<char *>(xml_str);
  read_data_init.len = read_data_init.ptr ? strlen(read_data_init.ptr) : 0;

  struct log_s &log = context.log;

  while(retry >= 0) {

    if(retry != MAX_TRY)
      appendf(log, O2G_NULLPTR, _("Retry %d/%d "), MAX_TRY-retry, MAX_TRY-1);

    /* get a curl handle */
    curl = curl_easy_init();
    if(!curl) {
      appendf(log, O2G_NULLPTR, _("CURL init error\n"));
      return false;
    }

    read_data = read_data_init;
    write_data.ptr = O2G_NULLPTR;
    write_data.len = 0;

    /* we want to use our own read/write functions */
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);

    /* enable uploading */
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

    /* specify target URL, and note that this URL should include a file
       name, not only a directory */
    curl_easy_setopt(curl, CURLOPT_URL, url);

	curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);

    /* now specify which file to upload */
    curl_easy_setopt(curl, CURLOPT_READDATA, &read_data);

    /* provide the size of the upload, we specicially typecast the value
       to curl_off_t since we must be sure to use the correct data size */
    curl_easy_setopt(curl, CURLOPT_INFILESIZE, (curl_off_t)read_data.len);

    /* we pass our 'chunk' struct to the callback function */
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &write_data);

    /* some servers don't like requests that are made without a user-agent
       field, so we provide one */
    curl_easy_setopt(curl, CURLOPT_USERAGENT, PACKAGE "-libcurl/" VERSION);

#ifdef NO_EXPECT
    struct curl_slist *slist = O2G_NULLPTR;
    slist = curl_slist_append(slist, "Expect:");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);
#endif

    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, buffer);

    /* set user name and password for the authentication */
    curl_easy_setopt(curl, CURLOPT_USERPWD, context.credentials.c_str());

    net_io_set_proxy(curl, context.proxy);

    /* Now run off and do what you've been told! */
    res = curl_easy_perform(curl);

    long response;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response);

    /* always cleanup */
#ifdef NO_EXPECT
    curl_slist_free_all(slist);
#endif
    curl_easy_cleanup(curl);

    /* this will return the id on a successful create */
    if(id && (res == 0) && (response == 200)) {
      printf("request to parse successful reply as an id\n");
      *id = strtoul(write_data.ptr, O2G_NULLPTR, 10);
    }

    if(res != 0)
      appendf(log, COLOR_ERR, _("failed: %s\n"), buffer);
    else if(response != 200)
      appendf(log, COLOR_ERR, _("failed, code: %ld %s\n"),
	      response, osm_http_message(response));
    else {
      if(!id) appendf(log, COLOR_OK, _("ok\n"));
      else    appendf(log, COLOR_OK, _("ok: #" ITEM_ID_FORMAT "\n"), *id);
    }

    /* if it's neither "ok" (200), nor "internal server error" (500) */
    /* then write the message to the log */
    if((response != 200) && (response != 500) && write_data.ptr) {
      appendf(log, O2G_NULLPTR, _("Server reply: "));
      appendf(log, COLOR_ERR, _("%s\n"), write_data.ptr);
    }

    g_free(write_data.ptr);

    /* don't retry unless we had an "internal server error" */
    if(response != 500)
      return((res == 0)&&(response == 200));

    retry--;
  }

  return false;
}

static gboolean osm_delete_item(osm_upload_context_t &context, xmlChar *xml_str,
                                const char *url) {
  int retry = MAX_TRY;
  char buffer[CURL_ERROR_SIZE];

  CURL *curl;
  CURLcode res;

  /* delete has a payload since api 0.6 */
  curl_data_t read_data, read_data_init;
  curl_data_t write_data;
  read_data_init.ptr = reinterpret_cast<char *>(xml_str);
  read_data_init.len = read_data_init.ptr ? strlen(read_data_init.ptr) : 0;
  struct log_s &log = context.log;

  while(retry >= 0) {

    if(retry != MAX_TRY)
      appendf(log, O2G_NULLPTR, _("Retry %d/%d "), MAX_TRY-retry, MAX_TRY-1);

    /* get a curl handle */
    curl = curl_easy_init();
    if(!curl) {
      appendf(log, O2G_NULLPTR, _("CURL init error\n"));
      return FALSE;
    }

    read_data = read_data_init;
    write_data.ptr = O2G_NULLPTR;
    write_data.len = 0;

    /* we want to use our own read/write functions */
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);

    curl_easy_setopt(curl, CURLOPT_INFILESIZE, (curl_off_t)read_data.len);

    /* now specify which file to upload */
    curl_easy_setopt(curl, CURLOPT_READDATA, &read_data);

    /* we pass our 'chunk' struct to the callback function */
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &write_data);

    /* enable uploading */
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

    /* no read/write functions required */
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");

    /* specify target URL, and note that this URL should include a file
       name, not only a directory */
    curl_easy_setopt(curl, CURLOPT_URL, url);

	curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);

    /* some servers don't like requests that are made without a user-agent
       field, so we provide one */
    curl_easy_setopt(curl, CURLOPT_USERAGENT, PACKAGE "-libcurl/" VERSION);

#ifdef NO_EXPECT
    struct curl_slist *slist = O2G_NULLPTR;
    slist = curl_slist_append(slist, "Expect:");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);
#endif

    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, buffer);

    /* set user name and password for the authentication */
    curl_easy_setopt(curl, CURLOPT_USERPWD, context.credentials.c_str());

#ifndef CURL_SSLVERSION_MAX_DEFAULT
#define CURL_SSLVERSION_MAX_DEFAULT 0
#endif
    curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1 |
                     CURL_SSLVERSION_MAX_DEFAULT);

    net_io_set_proxy(curl, context.proxy);

    /* Now run off and do what you've been told! */
    res = curl_easy_perform(curl);

    long response;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response);

    /* always cleanup */
#ifdef NO_EXPECT
    curl_slist_free_all(slist);
#endif
    curl_easy_cleanup(curl);

    if(res != 0)
      appendf(log, COLOR_ERR, _("failed: %s\n"), buffer);
    else if(response != 200)
      appendf(log, COLOR_ERR, _("failed, code: %ld %s\n"),
	      response, osm_http_message(response));
    else
      appendf(log, COLOR_OK, _("ok\n"));

    /* if it's neither "ok" (200), nor "internal server error" (500) */
    /* then write the message to the log */
    if((response != 200) && (response != 500) && write_data.ptr) {
      appendf(log, O2G_NULLPTR, _("Server reply: "));
      appendf(log, COLOR_ERR, _("%s\n"), write_data.ptr);
    }

    g_free(write_data.ptr);

    /* don't retry unless we had an "internal server error" */
    if(response != 500)
      return((res == 0)&&(response == 200));

    retry--;
  }

  return FALSE;
}

struct osm_dirty_t {
  GtkWidget *dialog;
  struct counter {
    int total, added, dirty, deleted;
  } ways, nodes, relations;
};

static GtkWidget *table_attach_label_c(GtkWidget *table, char *str,
				       int x1, int x2, int y1, int y2) {
  GtkWidget *label =  gtk_label_new(str);
  gtk_table_attach_defaults(GTK_TABLE(table), label, x1, x2, y1, y2);
  return label;
}

static GtkWidget *table_attach_label_l(GtkWidget *table, char *str,
				       int x1, int x2, int y1, int y2) {
  GtkWidget *label = table_attach_label_c(table, str, x1, x2, y1, y2);
  gtk_misc_set_alignment(GTK_MISC(label), 0.f, 0.5f);
  return label;
}

static GtkWidget *table_attach_int(GtkWidget *table, int num,
				   int x1, int x2, int y1, int y2) {
  gchar str[G_ASCII_DTOSTR_BUF_SIZE];
  g_snprintf(str, sizeof(str), "%d", num);
  GtkWidget *label = table_attach_label_c(table, str, x1, x2, y1, y2);
  return label;
}

struct osm_delete_nodes {
  osm_upload_context_t &context;
  osm_delete_nodes(osm_upload_context_t &co) : context(co) {}
  void operator()(std::pair<item_id_t, node_t *> pair);
};

void osm_delete_nodes::operator()(std::pair<item_id_t, node_t *> pair)
{
  node_t *node = pair.second;
  project_t *project = context.project;

    /* make sure gui gets updated */
    while(gtk_events_pending()) gtk_main_iteration();

  if(!(node->flags & OSM_FLAG_DELETED))
      return;

    printf("deleting node on server\n");

    appendf(context.log, O2G_NULLPTR, _("Delete node #" ITEM_ID_FORMAT " "), node->id);

    char *url = g_strdup_printf("%s/node/" ITEM_ID_FORMAT,
                                context.urlbasestr.c_str(), node->id);

    xmlChar *xml_str = node->generate_xml(context.changeset);

    if(osm_delete_item(context, xml_str, url)) {
      node->flags &= ~(OSM_FLAG_DIRTY | OSM_FLAG_DELETED);
      project->data_dirty = true;
    }
    xmlFree(xml_str);
    g_free(url);
}

struct osm_upload_nodes {
  osm_upload_context_t &context;
  osm_upload_nodes(osm_upload_context_t &co) : context(co) {}
  void operator()(std::pair<item_id_t, node_t *> pair);
};

void osm_upload_nodes::operator()(std::pair<item_id_t, node_t *> pair)
{
  node_t * const node = pair.second;
  project_t *project = context.project;

  /* make sure gui gets updated */
  while(gtk_events_pending()) gtk_main_iteration();

  if(!(node->flags & (OSM_FLAG_DIRTY | OSM_FLAG_NEW)) ||
     (node->flags & OSM_FLAG_DELETED))
    return;

  char *url = O2G_NULLPTR;

  if(node->flags & OSM_FLAG_NEW) {
    url = g_strconcat(context.urlbasestr.c_str(), "/node/create", O2G_NULLPTR);
    appendf(context.log, O2G_NULLPTR, _("New node "));
  } else {
    url = g_strdup_printf("%s/node/" ITEM_ID_FORMAT,
                          context.urlbasestr.c_str(), node->id);
    appendf(context.log, O2G_NULLPTR, _("Modified node #" ITEM_ID_FORMAT " "), node->id);
  }

  /* upload this node */
  xmlChar *xml_str = node->generate_xml(context.changeset);
  if(xml_str) {
    printf("uploading node %s from address %p\n", url, xml_str);

    if(osm_update_item(context, xml_str, url,
       (node->flags & OSM_FLAG_NEW) ? &(node->id) : &node->version)) {
      node->flags &= ~(OSM_FLAG_DIRTY | OSM_FLAG_NEW);
      project->data_dirty = true;
    }
    xmlFree(xml_str);
  }
  g_free(url);
}

struct osm_delete_ways {
  osm_upload_context_t &context;
  osm_delete_ways(osm_upload_context_t &co) : context(co) {}
  void operator()(std::pair<item_id_t, way_t *> pair);
};

void osm_delete_ways::operator()(std::pair<item_id_t, way_t *> pair)
{
  way_t * const way = pair.second;
  project_t *project = context.project;
  /* make sure gui gets updated */
  while(gtk_events_pending()) gtk_main_iteration();

  if(!(way->flags & OSM_FLAG_DELETED))
    return;

  printf("deleting way on server\n");

  appendf(context.log, O2G_NULLPTR, _("Delete way #" ITEM_ID_FORMAT " "), way->id);

  char *url = g_strdup_printf("%s/way/" ITEM_ID_FORMAT,
                                context.urlbasestr.c_str(), way->id);
  xmlChar *xml_str = way->generate_xml(context.changeset);

  if(osm_delete_item(context, xml_str, url)) {
    way->flags &= ~(OSM_FLAG_DIRTY | OSM_FLAG_DELETED);
    project->data_dirty = true;
  }

  xmlFree(xml_str);
  g_free(url);
}

struct osm_upload_ways {
  osm_upload_context_t &context;
  osm_upload_ways(osm_upload_context_t &co) : context(co) {}
  void operator()(std::pair<item_id_t, way_t *> pair);
};

void osm_upload_ways::operator()(std::pair<item_id_t, way_t *> pair)
{
  way_t * const way = pair.second;
  project_t *project = context.project;

  /* make sure gui gets updated */
  while(gtk_events_pending()) gtk_main_iteration();

  if(!(way->flags & (OSM_FLAG_DIRTY | OSM_FLAG_NEW)) ||
     (way->flags & OSM_FLAG_DELETED))
    return;

  char *url = O2G_NULLPTR;

  if(way->flags & OSM_FLAG_NEW) {
    url = g_strconcat(context.urlbasestr.c_str(), "/way/create", O2G_NULLPTR);
    appendf(context.log, O2G_NULLPTR, _("New way "));
  } else {
    url = g_strdup_printf("%s/way/" ITEM_ID_FORMAT,
                          context.urlbasestr.c_str(), way->id);
    appendf(context.log, O2G_NULLPTR, _("Modified way #" ITEM_ID_FORMAT " "), way->id);
  }

  /* upload this node */
  xmlChar *xml_str = way->generate_xml(context.changeset);
  if(xml_str) {
    printf("uploading way %s from address %p\n", url, xml_str);

    if(osm_update_item(context, xml_str, url,
        (way->flags & OSM_FLAG_NEW) ? &way->id : &way->version)) {
      way->flags &= ~(OSM_FLAG_DIRTY | OSM_FLAG_NEW);
      project->data_dirty = true;
    }
    xmlFree(xml_str);
  }
  g_free(url);
}

struct osm_delete_relations {
  osm_upload_context_t &context;
  osm_delete_relations(osm_upload_context_t &co) : context(co) {}
  void operator()(std::pair<item_id_t, relation_t *> pair);
};

void osm_delete_relations::operator()(std::pair<item_id_t, relation_t *> pair)
{
  relation_t * const relation = pair.second;
  project_t *project = context.project;

  /* make sure gui gets updated */
  while(gtk_events_pending()) gtk_main_iteration();

  if(!(relation->flags & OSM_FLAG_DELETED))
    return;

  printf("deleting relation on server\n");

  appendf(context.log, O2G_NULLPTR,
          _("Delete relation #" ITEM_ID_FORMAT " "), relation->id);

  char *url = g_strdup_printf("%s/relation/" ITEM_ID_FORMAT,
                              context.urlbasestr.c_str(), relation->id);
  xmlChar *xml_str = relation->generate_xml(context.changeset);

  if(osm_delete_item(context, xml_str, url)) {
    relation->flags &= ~(OSM_FLAG_DIRTY | OSM_FLAG_DELETED);
    project->data_dirty = true;
  }

  xmlFree(xml_str);
  g_free(url);
}

struct osm_upload_relations {
  osm_upload_context_t &context;
  osm_upload_relations(osm_upload_context_t &co) : context(co) {}
  void operator()(std::pair<item_id_t, relation_t *> pair);
};

void osm_upload_relations::operator()(std::pair<item_id_t, relation_t *> pair)
{
  relation_t * const relation = pair.second;
  project_t *project = context.project;

  /* make sure gui gets updated */
  while(gtk_events_pending()) gtk_main_iteration();

  if(!(relation->flags & (OSM_FLAG_DIRTY | OSM_FLAG_NEW)) ||
     (relation->flags & OSM_FLAG_DELETED))
    return;

  char *url = O2G_NULLPTR;

  if(relation->flags & OSM_FLAG_NEW) {
    url = g_strdup_printf("%s/relation/create", context.urlbasestr.c_str());
    appendf(context.log, O2G_NULLPTR, _("New relation "));
  } else {
    url = g_strdup_printf("%s/relation/" ITEM_ID_FORMAT,
                          context.urlbasestr.c_str(), relation->id);
    appendf(context.log, O2G_NULLPTR, _("Modified relation #" ITEM_ID_FORMAT " "),
            relation->id);
  }

  /* upload this relation */
  xmlChar *xml_str = relation->generate_xml(context.changeset);
  if(xml_str) {
    printf("uploading relation %s from address %p\n", url, xml_str);

    if(osm_update_item(context, xml_str, url,
        (relation->flags & OSM_FLAG_NEW) ? &relation->id : &relation->version)) {
      relation->flags &= ~(OSM_FLAG_DIRTY | OSM_FLAG_NEW);
      project->data_dirty = true;
    }
    xmlFree(xml_str);
  }
  g_free(url);
}

static bool osm_create_changeset(osm_upload_context_t &context) {
  bool result = false;
  context.changeset = ILLEGAL;

  /* make sure gui gets updated */
  while(gtk_events_pending()) gtk_main_iteration();

  const std::string url = context.urlbasestr + "/changeset/create";
  appendf(context.log, O2G_NULLPTR, _("Create changeset "));

  /* create changeset request */
  xmlChar *xml_str = osm_generate_xml_changeset(context.comment, context.src);
  if(xml_str) {
    printf("creating changeset %s from address %p\n", url.c_str(), xml_str);

    context.credentials = context.appdata->settings->username + std::string(":") +
                          context.appdata->settings->password;

    if(osm_update_item(context, xml_str, url.c_str(), &context.changeset)) {
      printf("got changeset id " ITEM_ID_FORMAT "\n", context.changeset);
      result = true;
    }
    xmlFree(xml_str);
  }

  return result;
}

static bool osm_close_changeset(osm_upload_context_t &context) {
  bool result = false;

  g_assert(context.changeset != ILLEGAL);

  /* make sure gui gets updated */
  while(gtk_events_pending()) gtk_main_iteration();

  char *url = g_strdup_printf("%s/changeset/" ITEM_ID_FORMAT "/close",
                              context.urlbasestr.c_str(), context.changeset);
  appendf(context.log, O2G_NULLPTR, _("Close changeset "));

  result = osm_update_item(context, O2G_NULLPTR, url, O2G_NULLPTR);

  g_free(url);

  return result;
}

/* comment buffer has been edited, allow upload if the buffer is not empty */
static void callback_buffer_modified(GtkTextBuffer *buffer, GtkDialog *dialog) {
  GtkTextIter start, end;
  gtk_text_buffer_get_start_iter(buffer, &start);
  gtk_text_buffer_get_end_iter(buffer, &end);
  char *text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
  gtk_dialog_set_response_sensitive(dialog, GTK_RESPONSE_ACCEPT,
				    text && strlen(text));
}

static gboolean cb_focus_in(GtkTextView *view, GdkEventFocus *,
			     GtkTextBuffer *buffer) {

  gboolean first_click =
    GPOINTER_TO_INT(g_object_get_data(G_OBJECT(view), "first_click"));

  g_object_set_data(G_OBJECT(view), "first_click", GINT_TO_POINTER(FALSE));

  if(first_click) {
    GtkTextIter start, end;
    gtk_text_buffer_get_start_iter(buffer, &start);
    gtk_text_buffer_get_end_iter(buffer, &end);
    gtk_text_buffer_delete(buffer, &start, &end);
  }

  return FALSE;
}

struct object_counter {
  osm_dirty_t::counter &dirty;
  object_counter(osm_dirty_t::counter &d) : dirty(d) {}
  void operator()(const base_object_t *obj);
  void operator()(std::pair<item_id_t, const base_object_t *> pair) {
    operator()(pair.second);
  }
};

void object_counter::operator()(const base_object_t *obj)
{
  int flags = obj->flags;
  dirty.total++;
  if(flags & OSM_FLAG_DELETED)
    dirty.deleted++;
  else if(flags & OSM_FLAG_NEW)
    dirty.added++;
  else if(flags & OSM_FLAG_DIRTY)
    dirty.dirty++;
}

static void table_insert_count(GtkWidget *table, const struct osm_dirty_t::counter &dirty,
                               const int row) {
  table_attach_int(table, dirty.total,   1, 2, row, row + 1);
  table_attach_int(table, dirty.added,   2, 3, row, row + 1);
  table_attach_int(table, dirty.dirty,   3, 4, row, row + 1);
  table_attach_int(table, dirty.deleted, 4, 5, row, row + 1);
}

static void details_table(GtkWidget *dialog, const osm_dirty_t &dirty) {
  GtkWidget *table = gtk_table_new(4, 5, TRUE);

  table_attach_label_c(table, _("Total"),          1, 2, 0, 1);
  table_attach_label_c(table, _("New"),            2, 3, 0, 1);
  table_attach_label_c(table, _("Modified"),       3, 4, 0, 1);
  table_attach_label_c(table, _("Deleted"),        4, 5, 0, 1);

  int row = 1;
  table_attach_label_l(table, _("Nodes:"),         0, 1, row, row + 1);
  table_insert_count(table, dirty.nodes, row++);

  table_attach_label_l(table, _("Ways:"),          0, 1, row, row + 1);
  table_insert_count(table, dirty.ways, row++);

  table_attach_label_l(table, _("Relations:"),     0, 1, row, row + 1);
  table_insert_count(table, dirty.relations, row++);

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),
                     table, FALSE, FALSE, 0);
}

#ifdef FREMANTLE
/* put additional infos into a seperate dialog for fremantle as */
/* screen space is sparse there */
static void info_more(const osm_dirty_t &context) {
  GtkWidget *dialog =
    misc_dialog_new(MISC_DIALOG_SMALL, _("Changeset details"),
		    GTK_WINDOW(context.dialog),
		    GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		    O2G_NULLPTR);

  gtk_dialog_set_default_response(GTK_DIALOG(dialog),
				  GTK_RESPONSE_CANCEL);

  details_table(dialog, context);
  gtk_widget_show_all(dialog);
  gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(dialog);
}
#endif

void osm_upload(appdata_t *appdata, osm_t *osm, project_t *project) {

  printf("starting upload\n");

  /* upload config and confirmation dialog */

  /* count nodes */
  osm_dirty_t dirty;
  memset(&dirty, 0, sizeof(dirty));

  std::for_each(osm->nodes.begin(), osm->nodes.end(), object_counter(dirty.nodes));
  printf("nodes:     new %2d, dirty %2d, deleted %2d\n",
	 dirty.nodes.added, dirty.nodes.dirty, dirty.nodes.deleted);

  /* count ways */
  std::for_each(osm->ways.begin(), osm->ways.end(), object_counter(dirty.ways));
  printf("ways:      new %2d, dirty %2d, deleted %2d\n",
	 dirty.ways.added, dirty.ways.dirty, dirty.ways.deleted);

  /* count relations */
  std::for_each(osm->relations.begin(), osm->relations.end(), object_counter(dirty.relations));
  printf("relations: new %2d, dirty %2d, deleted %2d\n",
	 dirty.relations.added, dirty.relations.dirty, dirty.relations.deleted);


  GtkWidget *dialog =
    misc_dialog_new(MISC_DIALOG_MEDIUM, _("Upload to OSM"),
		    GTK_WINDOW(appdata->window),
#ifdef FREMANTLE
                    _("More"), GTK_RESPONSE_HELP,
#endif
		    GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
		    GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
		    O2G_NULLPTR);
  dirty.dialog = dialog;

#ifndef FREMANTLE
  details_table(dialog, dirty);

  /* ------------------------------------------------------ */

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),
		     gtk_hseparator_new(), FALSE, FALSE, 0);
#endif

  /* ------- add username and password entries ------------ */

  GtkWidget *table = gtk_table_new(2, 2, FALSE);
  table_attach_label_l(table, _("Username:"), 0, 1, 0, 1);
  GtkWidget *uentry = entry_new();
  HILDON_ENTRY_NO_AUTOCAP(uentry);
  gtk_entry_set_text(GTK_ENTRY(uentry), appdata->settings->username);
  gtk_table_attach_defaults(GTK_TABLE(table),  uentry, 1, 2, 0, 1);
  table_attach_label_l(table, _("Password:"), 0, 1, 1, 2);
  GtkWidget *pentry = entry_new();
  HILDON_ENTRY_NO_AUTOCAP(pentry);
  gtk_entry_set_text(GTK_ENTRY(pentry), appdata->settings->password);
  gtk_entry_set_visibility(GTK_ENTRY(pentry), FALSE);
  gtk_table_attach_defaults(GTK_TABLE(table),  pentry, 1, 2, 1, 2);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, FALSE, FALSE, 0);

  table_attach_label_l(table, _("Source:"), 0, 1, 2, 3);
  GtkWidget *sentry = entry_new();
  HILDON_ENTRY_NO_AUTOCAP(sentry);
  gtk_table_attach_defaults(GTK_TABLE(table),  sentry, 1, 2, 2, 3);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, FALSE, FALSE, 0);

  GtkWidget *scrolled_win = misc_scrolled_window_new(TRUE);

  GtkTextBuffer *buffer = gtk_text_buffer_new(O2G_NULLPTR);
  gtk_text_buffer_set_text(buffer, _("Please add a comment"), -1);

  /* disable ok button until user edited the comment */
  gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog),
				    GTK_RESPONSE_ACCEPT, FALSE);

  g_signal_connect(G_OBJECT(buffer), "changed",
		   G_CALLBACK(callback_buffer_modified), dialog);

#ifndef FREMANTLE
  GtkWidget *view = gtk_text_view_new_with_buffer(buffer);
#else
  GtkWidget *view = hildon_text_view_new();
  hildon_text_view_set_buffer(HILDON_TEXT_VIEW(view), buffer);
#endif

  gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(view), GTK_WRAP_WORD);
  gtk_text_view_set_editable(GTK_TEXT_VIEW(view), TRUE);
  gtk_text_view_set_left_margin(GTK_TEXT_VIEW(view), 2 );
  gtk_text_view_set_right_margin(GTK_TEXT_VIEW(view), 2 );

  g_object_set_data(G_OBJECT(view), "first_click", GINT_TO_POINTER(TRUE));
  g_signal_connect(G_OBJECT(view), "focus-in-event",
		   G_CALLBACK(cb_focus_in), buffer);

  gtk_container_add(GTK_CONTAINER(scrolled_win), view);

  gtk_box_pack_start_defaults(GTK_BOX(GTK_DIALOG(dialog)->vbox),
  			      scrolled_win);
  gtk_widget_show_all(dialog);

  bool done = false;
  while(!done) {
    switch(gtk_dialog_run(GTK_DIALOG(dialog))) {
#ifdef FREMANTLE
    case GTK_RESPONSE_HELP:
      info_more(dirty);
      break;
#endif
    case GTK_RESPONSE_ACCEPT:
      done = true;
      break;
    default:
      printf("upload cancelled\n");
      gtk_widget_destroy(dialog);
      return;
    }
  }

  printf("clicked ok\n");

  /* retrieve username and password */
  g_free(appdata->settings->username);
  appdata->settings->username =
    g_strdup(gtk_entry_get_text(GTK_ENTRY(uentry)));

  g_free(appdata->settings->password);
  appdata->settings->password =
    g_strdup(gtk_entry_get_text(GTK_ENTRY(pentry)));

  /* fetch comment from dialog */
  GtkTextIter start, end;
  gtk_text_buffer_get_start_iter(buffer, &start);
  gtk_text_buffer_get_end_iter(buffer, &end);
  char *text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);

  osm_upload_context_t context(appdata, osm, project, text,
                               gtk_entry_get_text(GTK_ENTRY(sentry)));

  gtk_widget_destroy(dialog);
  project_save(GTK_WIDGET(appdata->window), project);

  context.dialog =
    misc_dialog_new(MISC_DIALOG_LARGE,_("Uploading"),
	  GTK_WINDOW(appdata->window),
	  GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE, O2G_NULLPTR);

  gtk_dialog_set_response_sensitive(GTK_DIALOG(context.dialog),
				    GTK_RESPONSE_CLOSE, FALSE);

  /* ------- main ui element is this text view --------------- */

  GtkWidget *scrolled_window = gtk_scrolled_window_new(O2G_NULLPTR, O2G_NULLPTR);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
  				 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  context.log.buffer = gtk_text_buffer_new(O2G_NULLPTR);

  context.log.view = gtk_text_view_new_with_buffer(context.log.buffer);
  gtk_text_view_set_editable(GTK_TEXT_VIEW(context.log.view), FALSE);
  gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(context.log.view), FALSE);
  gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(context.log.view), GTK_WRAP_WORD);

  gtk_container_add(GTK_CONTAINER(scrolled_window), context.log.view);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled_window),
				      GTK_SHADOW_IN);

  gtk_box_pack_start_defaults(GTK_BOX(GTK_DIALOG(context.dialog)->vbox),
			       scrolled_window);
  gtk_widget_show_all(context.dialog);

  /* server url should not end with a slash */
  if(!project->rserver.empty() && project->rserver[project->rserver.size() - 1] == '/') {
    printf("removing trailing slash\n");
    project->rserver.erase(project->rserver.size() - 1);
  }

  appendf(context.log, O2G_NULLPTR, _("Log generated by %s v%s using API 0.6\n"),
	  PACKAGE, VERSION);
  appendf(context.log, O2G_NULLPTR, _("User comment: %s\n"), context.comment.c_str());

  if(api_adjust(project->rserver))
    appendf(context.log, O2G_NULLPTR, _("Adjusting server name to v0.6\n"));

  appendf(context.log, O2G_NULLPTR, _("Uploading to %s\n"), project->server);

  /* create a new changeset */
  if(osm_create_changeset(context)) {
    /* check for dirty entries */
    appendf(context.log, O2G_NULLPTR, _("Uploading nodes:\n"));
    std::for_each(osm->nodes.begin(), osm->nodes.end(), osm_upload_nodes(context));
    appendf(context.log, O2G_NULLPTR, _("Uploading ways:\n"));
    std::for_each(osm->ways.begin(), osm->ways.end(), osm_upload_ways(context));
    appendf(context.log, O2G_NULLPTR, _("Uploading relations:\n"));
    std::for_each(osm->relations.begin(), osm->relations.end(), osm_upload_relations(context));
    appendf(context.log, O2G_NULLPTR, _("Deleting relations:\n"));
    std::for_each(osm->relations.begin(), osm->relations.end(), osm_delete_relations(context));
    appendf(context.log, O2G_NULLPTR, _("Deleting ways:\n"));
    std::for_each(osm->ways.begin(), osm->ways.end(), osm_delete_ways(context));
    appendf(context.log, O2G_NULLPTR, _("Deleting nodes:\n"));
    std::for_each(osm->nodes.begin(), osm->nodes.end(), osm_delete_nodes(context));

    /* close changeset */
    osm_close_changeset(context);
  }

  appendf(context.log, O2G_NULLPTR, _("Upload done.\n"));

  gboolean reload_map = FALSE;
  if(project->data_dirty) {
    appendf(context.log, O2G_NULLPTR, _("Server data has been modified.\n"));
    appendf(context.log, O2G_NULLPTR, _("Downloading updated osm data ...\n"));

    if(osm_download(context.dialog, appdata->settings, project)) {
      appendf(context.log, O2G_NULLPTR, _("Download successful!\n"));
      appendf(context.log, O2G_NULLPTR, _("The map will be reloaded.\n"));
      project->data_dirty = false;
      reload_map = TRUE;
    } else
      appendf(context.log, O2G_NULLPTR, _("Download failed!\n"));

    project_save(context.dialog, project);

    if(reload_map) {
      /* this kind of rather brute force reload is useful as the moment */
      /* after the upload is a nice moment to bring everything in sync again. */
      /* we basically restart the entire map with fresh data from the server */
      /* and the diff will hopefully be empty (if the upload was successful) */

      appendf(context.log, O2G_NULLPTR, _("Reloading map ...\n"));

      if(!diff_is_clean(appdata->osm, FALSE)) {
	appendf(context.log, COLOR_ERR, _("*** DIFF IS NOT CLEAN ***\n"));
	appendf(context.log, COLOR_ERR, _("Something went wrong during upload,\n"));
	appendf(context.log, COLOR_ERR, _("proceed with care!\n"));
      }

      /* redraw the entire map by destroying all map items and redrawing them */
      appendf(context.log, O2G_NULLPTR, _("Cleaning up ...\n"));
      diff_save(appdata->project, appdata->osm);
      map_clear(appdata->map, MAP_LAYER_OBJECTS_ONLY);
      delete appdata->osm;

      appendf(context.log, O2G_NULLPTR, _("Loading OSM ...\n"));
      appdata->osm = project_parse_osm(appdata->project, &appdata->icon);
      appendf(context.log, O2G_NULLPTR, _("Applying diff ...\n"));
      diff_restore(appdata, appdata->project, appdata->osm);
      appendf(context.log, O2G_NULLPTR, _("Painting ...\n"));
      map_paint(appdata->map);
      appendf(context.log, O2G_NULLPTR, _("Done!\n"));
    }
  }

  /* tell the user that he can stop waiting ... */
  appendf(context.log, O2G_NULLPTR, _("Process finished.\n"));

  gtk_dialog_set_response_sensitive(GTK_DIALOG(context.dialog),
				    GTK_RESPONSE_CLOSE, TRUE);

  gtk_dialog_run(GTK_DIALOG(context.dialog));
  gtk_widget_destroy(context.dialog);
}
