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
#include "osm.h"
#include "osm2go_platform.h"
#include "net_io.h"
#include "project.h"
#include "settings.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <curl/curl.h>
#include <curl/easy.h> /* new for v7 */
#include <map>
#include <sys/stat.h>
#include <unistd.h>

#ifdef FREMANTLE
#include <hildon/hildon-text-view.h>
#endif

#include "osm2go_annotations.h"
#include <osm2go_cpp.h>
#include <osm2go_i18n.h>

#define COLOR_ERR  "red"
#define COLOR_OK   "darkgreen"

#define NO_EXPECT

struct log_s {
  log_s()
    : buffer(O2G_NULLPTR), view(O2G_NULLPTR) {}

  GtkTextBuffer *buffer;
  GtkWidget *view;
};

struct osm_upload_context_t {
  osm_upload_context_t(appdata_t &a, osm_t *o, project_t *p, const char *c, const char *s)
    : appdata(a)
    , dialog(O2G_NULLPTR)
    , osm(o)
    , project(p)
    , urlbasestr(p->server + std::string("/"))
    , comment(c)
    , src(s ? s : std::string())
  {}

  appdata_t &appdata;
  GtkWidget *dialog;
  osm_t * const osm;
  project_t * const project;
  const std::string urlbasestr; ///< API base URL, will always end in '/'

  struct log_s log;

  std::string changeset;

  std::string comment;
  std::string credentials;
  const std::string src;
};

bool osm_download(GtkWidget *parent, settings_t *settings, project_t *project)
{
  printf("download osm ...\n");

  assert(project->server != O2G_NULLPTR);

  if(!project->rserver.empty()) {
    if(api_adjust(project->rserver))
      messagef(parent, _("Server changed"),
               _("It seems your current project uses an outdated server/protocol. "
               "It has thus been changed to:\n\n%s"),
               project->rserver.c_str());

    /* server url should not end with a slash */
    if(unlikely(project->rserver[project->rserver.size() - 1] == '/')) {
      printf("removing trailing slash\n");
      project->rserver.erase(project->rserver.size() - 1);
    }

    if(project->rserver == settings->server) {
      project->rserver.clear();
      project->server = settings->server.c_str();
    }
  }

  char minlon[G_ASCII_DTOSTR_BUF_SIZE], minlat[G_ASCII_DTOSTR_BUF_SIZE];
  char maxlon[G_ASCII_DTOSTR_BUF_SIZE], maxlat[G_ASCII_DTOSTR_BUF_SIZE];

  g_ascii_formatd(minlon, sizeof(minlon), LL_FORMAT, project->min.lon);
  g_ascii_formatd(minlat, sizeof(minlat), LL_FORMAT, project->min.lat);
  g_ascii_formatd(maxlon, sizeof(maxlon), LL_FORMAT, project->max.lon);
  g_ascii_formatd(maxlat, sizeof(maxlat), LL_FORMAT, project->max.lat);

  const std::string url = std::string(project->server) + "/map?bbox=" +
                          minlon + "," + minlat + "," +
                          maxlon +  "," +  maxlat;

  /* Download the new file to a new name. If something goes wrong then the
   * old file will still be in place to be opened. */
  const char *updatefn = "update.osm";
  const std::string update = project->path + updatefn;
  unlinkat(project->dirfd, updatefn, 0);

  if(unlikely(!net_io_download_file(parent, url, update, project->name.c_str(), true)))
    return false;

  struct stat st;
  if(unlikely(stat(update.c_str(), &st) != 0 || !S_ISREG(st.st_mode)))
    return false;

  // if the project's gzip setting and the download one don't match change the project
  const bool wasGzip = project->osm.size() > 3 && strcmp(project->osm.c_str() + project->osm.size() - 3, ".gz") == 0;

  // check the contents of the new file
  GMappedFile *osmData = g_mapped_file_new(update.c_str(), FALSE, O2G_NULLPTR);
  if(unlikely(!osmData)) {
    messagef(parent, _("Download error"),
             _("Error accessing the downloaded file:\n\n%s"), update.c_str());
    unlink(update.c_str());
    return false;
  }

  const bool isGzip = check_gzip(g_mapped_file_get_contents(osmData),
                              g_mapped_file_get_length(osmData));
#if GLIB_CHECK_VERSION(2,22,0)
  g_mapped_file_unref(osmData);
#else
  g_mapped_file_free(osmData);
#endif

  /* if there's a new file use this from now on */
  printf("download ok, replacing previous file\n");

  if(wasGzip != isGzip) {
    const std::string oldfname = (project->osm[0] == '/' ? std::string() : project->path) +
                                 project->osm;
    std::string newfname = oldfname;
    if(wasGzip)
      newfname.erase(newfname.size() - 3);
    else
      newfname += ".gz";
    rename(update.c_str(), newfname.c_str());
    // save the project before deleting the old file so that a valid file is always found
    if(newfname.substr(0, project->path.size()) == project->path)
      newfname.erase(0, project->path.size());
    project->osm = newfname;
    project->save(parent);

    // now remove the old file
    unlink(oldfname.c_str());
  } else {
    if(project->osm[0] == '/') {
      rename(update.c_str(), project->osm.c_str());
    } else {
      const std::string fname = project->path + project->osm;
      rename(update.c_str(), fname.c_str());
    }
  }

  return true;
}

struct curl_data_t {
  explicit curl_data_t(char *p = O2G_NULLPTR, curl_off_t l = 0)
    : ptr(p), len(l) {}
  char *ptr;
  long len;
};

static size_t read_callback(void *ptr, size_t size, size_t nmemb, void *stream) {
  curl_data_t *p = static_cast<curl_data_t *>(stream);

  //  printf("request to read %d items of size %d, pointer = %p\n",
  //  nmemb, size, p->ptr);

  if(nmemb * size > static_cast<size_t>(p->len))
    nmemb = p->len/size;

  memcpy(ptr, p->ptr, size*nmemb);
  p->ptr += size*nmemb;
  p->len -= size*nmemb;

  return nmemb;
}

static size_t write_callback(void *ptr, size_t size, size_t nmemb, void *stream) {
  curl_data_t *p = static_cast<curl_data_t *>(stream);

  p->ptr = static_cast<char *>(g_realloc(p->ptr, p->len + size*nmemb + 1));
  memcpy(p->ptr+p->len, ptr, size*nmemb);
  p->len += size*nmemb;
  p->ptr[p->len] = 0;
  return nmemb;
}

static G_GNUC_PRINTF(3, 4) void appendf(struct log_s &log, const char *colname,
		    const char *fmt, ...) {
  va_list args;
  va_start( args, fmt );
  char *buf = g_strdup_vprintf(fmt, args);
  va_end( args );

  printf("%s", buf);

  GtkTextIter end;
  gtk_text_buffer_get_end_iter(log.buffer, &end);
  if(colname) {
    GtkTextTag *tag = gtk_text_buffer_create_tag(log.buffer, O2G_NULLPTR,
                                                 "foreground", colname,
                                                 O2G_NULLPTR);
    gtk_text_buffer_insert_with_tags(log.buffer, &end, buf, -1, tag, O2G_NULLPTR);
  } else
    gtk_text_buffer_insert(log.buffer, &end, buf, -1);

  g_free(buf);

  gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(log.view),
			       &end, 0.0, FALSE, 0, 0);

  osm2go_platform::process_events();
}

#define MAX_TRY 5

static CURL *curl_custom_setup(const osm_upload_context_t &context, const char *url)
{
  /* get a curl handle */
  CURL *curl = curl_easy_init();
  if(!curl)
    return curl;

  /* we want to use our own write function */
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);

  /* specify target URL, and note that this URL should include a file
     name, not only a directory */
  curl_easy_setopt(curl, CURLOPT_URL, url);

  /* some servers don't like requests that are made without a user-agent
     field, so we provide one */
  curl_easy_setopt(curl, CURLOPT_USERAGENT, PACKAGE "-libcurl/" VERSION);

  /* set user name and password for the authentication */
  curl_easy_setopt(curl, CURLOPT_USERPWD, context.credentials.c_str());

#ifndef CURL_SSLVERSION_MAX_DEFAULT
#define CURL_SSLVERSION_MAX_DEFAULT 0
#endif
  curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1 |
                   CURL_SSLVERSION_MAX_DEFAULT);

  return curl;
}

static bool osm_update_item(osm_upload_context_t &context, xmlChar *xml_str,
                            const char *url, item_id_t *id) {
  int retry = MAX_TRY;
  char buffer[CURL_ERROR_SIZE];

  CURL *curl;
  CURLcode res;

  curl_data_t read_data_init(reinterpret_cast<char *>(xml_str));
  read_data_init.len = read_data_init.ptr ? strlen(read_data_init.ptr) : 0;

  struct log_s &log = context.log;

  while(retry >= 0) {

    if(retry != MAX_TRY)
      appendf(log, O2G_NULLPTR, _("Retry %d/%d "), MAX_TRY-retry, MAX_TRY-1);

    /* get a curl handle */
    curl = curl_custom_setup(context, url);
    if(!curl) {
      appendf(log, O2G_NULLPTR, _("CURL init error\n"));
      return false;
    }

    curl_data_t read_data = read_data_init;
    curl_data_t write_data;

    /* enable uploading */
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

    /* we want to use our own read function */
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback);

    /* now specify which file to upload */
    curl_easy_setopt(curl, CURLOPT_READDATA, &read_data);

    /* provide the size of the upload */
    curl_easy_setopt(curl, CURLOPT_INFILESIZE, read_data.len);

    /* we pass our 'chunk' struct to the callback function */
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &write_data);

#ifdef NO_EXPECT
    struct curl_slist *slist = O2G_NULLPTR;
    slist = curl_slist_append(slist, "Expect:");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);
#endif

    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, buffer);

    /* Now run off and do what you've been told! */
    res = curl_easy_perform(curl);

    long response;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response);

    /* always cleanup */
#ifdef NO_EXPECT
    curl_slist_free_all(slist);
#endif
    curl_easy_cleanup(curl);

    if(unlikely(res != 0)) {
      appendf(log, COLOR_ERR, _("failed: %s\n"), buffer);
    } else if(unlikely(response != 200)) {
      appendf(log, COLOR_ERR, _("failed, code: %ld %s\n"), response,
              http_message(response));
      /* if it's neither "ok" (200), nor "internal server error" (500) */
      /* then write the message to the log */
      if(response != 500 && write_data.ptr) {
        appendf(log, O2G_NULLPTR, _("Server reply: "));
        appendf(log, COLOR_ERR, _("%s\n"), write_data.ptr);
      }
    } else if(unlikely(!id)) {
      appendf(log, COLOR_OK, _("ok\n"));
    } else {
      /* this will return the id on a successful create */
      printf("request to parse successful reply as an id\n");
      *id = strtoull(write_data.ptr, O2G_NULLPTR, 10);
      appendf(log, COLOR_OK, _("ok: #" ITEM_ID_FORMAT "\n"), *id);
    }

    g_free(write_data.ptr);

    /* don't retry unless we had an "internal server error" */
    if(response != 500)
      return((res == 0)&&(response == 200));

    retry--;
  }

  return false;
}

static bool osm_delete_item(osm_upload_context_t &context, xmlChar *xml_str,
                            int len, const char *url) {
  int retry = MAX_TRY;
  char buffer[CURL_ERROR_SIZE];

  CURL *curl;
  CURLcode res;

  struct log_s &log = context.log;

  while(retry >= 0) {

    if(retry != MAX_TRY)
      appendf(log, O2G_NULLPTR, _("Retry %d/%d "), MAX_TRY-retry, MAX_TRY-1);

    /* get a curl handle */
    curl = curl_custom_setup(context, url);
    if(!curl) {
      appendf(log, O2G_NULLPTR, _("CURL init error\n"));
      return false;
    }

    curl_data_t write_data;

    /* no read function required */
    curl_easy_setopt(curl, CURLOPT_POST, 1);

    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, xml_str);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, len);

    /* we pass our 'chunk' struct to the callback function */
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &write_data);

#ifdef NO_EXPECT
    struct curl_slist *slist = O2G_NULLPTR;
    slist = curl_slist_append(slist, "Expect:");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);
#endif

    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, buffer);

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
	      response, http_message(response));
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

  return false;
}

/**
 * @brief upload one object to the OSM server
 */
static void upload_object(osm_upload_context_t &context, base_object_t *obj) {
  /* make sure gui gets updated */
  osm2go_platform::process_events();

  assert(obj->flags & OSM_FLAG_DIRTY);

  std::string url = context.urlbasestr + obj->apiString() + '/';

  if(obj->isNew()) {
    url += "create";
    appendf(context.log, O2G_NULLPTR, _("New %s "), obj->apiString());
  } else {
    url += obj->id_string();
    appendf(context.log, O2G_NULLPTR, _("Modified %s #" ITEM_ID_FORMAT " "), obj->apiString(), obj->id);
  }

  /* upload this object */
  xmlChar *xml_str = obj->generate_xml(context.changeset);
  if(xml_str) {
    printf("uploading %s " ITEM_ID_FORMAT " to %s\n", obj->apiString(), obj->id, url.c_str());

    if(osm_update_item(context, xml_str, url.c_str(), obj->isNew() ? &obj->id : &obj->version)) {
      obj->flags ^= OSM_FLAG_DIRTY;
      context.project->data_dirty = true;
    }
    xmlFree(xml_str);
  }
}

template<typename T>
struct upload_objects {
  osm_upload_context_t &context;
  std::map<item_id_t, T *> &map;
  upload_objects(osm_upload_context_t &co, std::map<item_id_t, T*> &m)
    : context(co), map(m) {}
  void operator()(T *obj);
};

template<typename T>
void upload_objects<T>::operator()(T *obj)
{
  item_id_t oldid = obj->id;
  upload_object(context, obj);
  if(oldid != obj->id) {
    map.erase(oldid);
    map[obj->id] = obj;
  }
}

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

static void log_deletion(osm_upload_context_t &context, const base_object_t *obj) {
  assert(obj->flags & OSM_FLAG_DELETED);

  appendf(context.log, O2G_NULLPTR, _("Deleted %s #" ITEM_ID_FORMAT " (version " ITEM_ID_FORMAT ")\n"),
          obj->apiString(), obj->id, obj->version);
}

struct osm_delete_objects_final {
  osm_upload_context_t &context;
  explicit osm_delete_objects_final(osm_upload_context_t &c)
    : context(c) {}
  void operator()(relation_t *r) {
    log_deletion(context, r);
    context.osm->relation_free(r);
  }
  void operator()(way_t *w) {
    log_deletion(context, w);
    context.osm->way_free(w);
  }
  void operator()(node_t *n) {
    log_deletion(context, n);
    context.osm->node_free(n);
  }
};

/**
 * @brief upload the given osmChange document
 * @param context the context pointer
 * @param doc the document to upload
 * @returns if the operation was successful
 */
static bool osmchange_upload(osm_upload_context_t &context, xmlDocPtr doc)
{
  /* make sure gui gets updated */
  osm2go_platform::process_events();

  printf("deleting objects on server\n");

  appendf(context.log, O2G_NULLPTR, _("Uploading object deletions "));

  const std::string url = context.urlbasestr + "changeset/" + context.changeset + "/upload";

  xmlChar *xml_str = O2G_NULLPTR;
  int len = 0;

  xmlDocDumpFormatMemoryEnc(doc, &xml_str, &len, "UTF-8", 1);

  bool ret = osm_delete_item(context, xml_str, len, url.c_str());
  if(ret)
    context.project->data_dirty = true;
  xmlFree(xml_str);

  return ret;
}

static bool osm_create_changeset(osm_upload_context_t &context) {
  bool result = false;

  /* make sure gui gets updated */
  osm2go_platform::process_events();

  const std::string url = context.urlbasestr + "changeset/create";
  appendf(context.log, O2G_NULLPTR, _("Create changeset "));

  /* create changeset request */
  xmlChar *xml_str = osm_generate_xml_changeset(context.comment, context.src);
  if(xml_str) {
    printf("creating changeset %s from address %p\n", url.c_str(), xml_str);

    context.credentials = context.appdata.settings->username + ":" +
                          context.appdata.settings->password;

    item_id_t changeset;
    if(osm_update_item(context, xml_str, url.c_str(), &changeset)) {
      char str[32];
      snprintf(str, sizeof(str), ITEM_ID_FORMAT, changeset);
      printf("got changeset id %s\n", str);
      context.changeset = str;
      result = true;
    }
    xmlFree(xml_str);
  }

  return result;
}

static bool osm_close_changeset(osm_upload_context_t &context) {
  bool result = false;

  assert(!context.changeset.empty());

  /* make sure gui gets updated */
  osm2go_platform::process_events();

  const std::string url = context.urlbasestr + "changeset/" + context.changeset +
                          "/close";
  appendf(context.log, O2G_NULLPTR, _("Close changeset "));

  result = osm_update_item(context, O2G_NULLPTR, url.c_str(), O2G_NULLPTR);

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

template<typename T>
void table_insert_count(GtkWidget *table, const osm_t::dirty_t::counter<T> &counter, const int row) {
  table_attach_int(table, counter.total,   1, 2, row, row + 1);
  table_attach_int(table, counter.added,   2, 3, row, row + 1);
  table_attach_int(table, counter.dirty,   3, 4, row, row + 1);
  table_attach_int(table, counter.deleted.size(), 4, 5, row, row + 1);
}

static void details_table(GtkWidget *dialog, const osm_t::dirty_t &dirty) {
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
static void info_more(const osm_t::dirty_t &context, GtkWidget *parent) {
  GtkWidget *dialog =
    misc_dialog_new(MISC_DIALOG_SMALL, _("Changeset details"),
                    GTK_WINDOW(parent),
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

void osm_upload(appdata_t &appdata, osm_t *osm, project_t *project) {
  if(unlikely(osm->uploadPolicy == osm_t::Upload_Blocked)) {
    printf("Upload prohibited\n");
    return;
  }

  printf("starting upload\n");

  /* upload config and confirmation dialog */

  /* count objects */
  osm_t::dirty_t dirty = osm->modified();

  printf("nodes:     new %2u, dirty %2u, deleted %2zu\n",
         dirty.nodes.added, dirty.nodes.dirty, dirty.nodes.deleted.size());
  printf("ways:      new %2u, dirty %2u, deleted %2zu\n",
         dirty.ways.added, dirty.ways.dirty, dirty.ways.deleted.size());
  printf("relations: new %2u, dirty %2u, deleted %2zu\n",
         dirty.relations.added, dirty.relations.dirty, dirty.relations.deleted.size());

  GtkWidget *dialog =
    misc_dialog_new(MISC_DIALOG_MEDIUM, _("Upload to OSM"),
		    GTK_WINDOW(appdata.window),
#ifdef FREMANTLE
                    _("More"), GTK_RESPONSE_HELP,
#endif
		    GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
		    GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
		    O2G_NULLPTR);

#ifndef FREMANTLE
  details_table(dialog, dirty);

  /* ------------------------------------------------------ */

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),
		     gtk_hseparator_new(), FALSE, FALSE, 0);
#endif

  /* ------- add username and password entries ------------ */

  GtkWidget *table = gtk_table_new(2, 2, FALSE);
  table_attach_label_l(table, _("Username:"), 0, 1, 0, 1);
  GtkWidget *uentry = entry_new(EntryFlagsNoAutoCap);
  const char *username = !appdata.settings->username.empty() ?
                         appdata.settings->username.c_str() :
                         _("<your osm username>");
  gtk_entry_set_text(GTK_ENTRY(uentry), username);
  gtk_table_attach_defaults(GTK_TABLE(table),  uentry, 1, 2, 0, 1);
  table_attach_label_l(table, _("Password:"), 0, 1, 1, 2);
  GtkWidget *pentry = entry_new(EntryFlagsNoAutoCap);
  if(!appdata.settings->password.empty())
    gtk_entry_set_text(GTK_ENTRY(pentry), appdata.settings->password.c_str());
  gtk_entry_set_visibility(GTK_ENTRY(pentry), FALSE);
  gtk_table_attach_defaults(GTK_TABLE(table),  pentry, 1, 2, 1, 2);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, FALSE, FALSE, 0);

  table_attach_label_l(table, _("Source:"), 0, 1, 2, 3);
  GtkWidget *sentry = entry_new(EntryFlagsNoAutoCap);
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
      info_more(dirty, dialog);
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
  appdata.settings->username = gtk_entry_get_text(GTK_ENTRY(uentry));
  appdata.settings->password = gtk_entry_get_text(GTK_ENTRY(pentry));

  /* fetch comment from dialog */
  GtkTextIter start, end;
  gtk_text_buffer_get_start_iter(buffer, &start);
  gtk_text_buffer_get_end_iter(buffer, &end);
  char *text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);

  /* server url should not end with a slash */
  if(!project->rserver.empty() && project->rserver[project->rserver.size() - 1] == '/') {
    printf("removing trailing slash\n");
    project->rserver.erase(project->rserver.size() - 1);
  }

  osm_upload_context_t context(appdata, osm, project, text,
                               gtk_entry_get_text(GTK_ENTRY(sentry)));

  gtk_widget_destroy(dialog);
  project->save(appdata.window);

  context.dialog =
    misc_dialog_new(MISC_DIALOG_LARGE,_("Uploading"),
	  GTK_WINDOW(appdata.window),
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

  appendf(context.log, O2G_NULLPTR, _("Log generated by %s v%s using API 0.6\n"),
	  PACKAGE, VERSION);
  appendf(context.log, O2G_NULLPTR, _("User comment: %s\n"), context.comment.c_str());

  if(api_adjust(project->rserver)) {
    appendf(context.log, O2G_NULLPTR, _("Server URL adjusted to %s\n"),
            project->rserver.c_str());
    if(likely(project->rserver == context.appdata.settings->server)) {
      project->rserver.clear();
      project->server = context.appdata.settings->server.c_str();
    }
  }

  appendf(context.log, O2G_NULLPTR, _("Uploading to %s\n"), project->server);

  /* create a new changeset */
  if(osm_create_changeset(context)) {
    /* check for dirty entries */
    if(!dirty.nodes.modified.empty()) {
      appendf(context.log, O2G_NULLPTR, _("Uploading nodes:\n"));
      std::for_each(dirty.nodes.modified.begin(), dirty.nodes.modified.end(),
                    upload_objects<node_t>(context, osm->nodes));
    }
    if(!dirty.ways.modified.empty()) {
      appendf(context.log, O2G_NULLPTR, _("Uploading ways:\n"));
      std::for_each(dirty.ways.modified.begin(), dirty.ways.modified.end(),
                    upload_objects<way_t>(context, osm->ways));
    }
    if(!dirty.relations.modified.empty()) {
      appendf(context.log, O2G_NULLPTR, _("Uploading relations:\n"));
      std::for_each(dirty.relations.modified.begin(), dirty.relations.modified.end(),
                    upload_objects<relation_t>(context, osm->relations));
    }
    if(!dirty.relations.deleted.empty() || !dirty.ways.deleted.empty() || !dirty.nodes.deleted.empty()) {
      appendf(context.log, O2G_NULLPTR, _("Deleting objects:\n"));
      xmlDocPtr doc = osmchange_init();
      xmlNodePtr del_node = xmlNewChild(xmlDocGetRootElement(doc), O2G_NULLPTR, BAD_CAST "delete", O2G_NULLPTR);
      osmchange_delete(dirty, del_node, context.changeset.c_str());

      // deletion was successful, remove the objects
      if(osmchange_upload(context, doc)) {
        osm_delete_objects_final finfc(context);
        std::for_each(dirty.relations.deleted.begin(), dirty.relations.deleted.end(), finfc);
        std::for_each(dirty.ways.deleted.begin(), dirty.ways.deleted.end(), finfc);
        std::for_each(dirty.nodes.deleted.begin(), dirty.nodes.deleted.end(), finfc);
      }
    }

    /* close changeset */
    osm_close_changeset(context);
  }

  appendf(context.log, O2G_NULLPTR, _("Upload done.\n"));

  bool reload_map = false;
  if(project->data_dirty) {
    appendf(context.log, O2G_NULLPTR, _("Server data has been modified.\n"));
    appendf(context.log, O2G_NULLPTR, _("Downloading updated osm data ...\n"));

    if(osm_download(context.dialog, appdata.settings, project)) {
      appendf(context.log, O2G_NULLPTR, _("Download successful!\n"));
      appendf(context.log, O2G_NULLPTR, _("The map will be reloaded.\n"));
      project->data_dirty = false;
      reload_map = true;
    } else
      appendf(context.log, O2G_NULLPTR, _("Download failed!\n"));

    project->save(context.dialog);

    if(reload_map) {
      /* this kind of rather brute force reload is useful as the moment */
      /* after the upload is a nice moment to bring everything in sync again. */
      /* we basically restart the entire map with fresh data from the server */
      /* and the diff will hopefully be empty (if the upload was successful) */

      appendf(context.log, O2G_NULLPTR, _("Reloading map ...\n"));

      if(!diff_is_clean(appdata.osm, false)) {
	appendf(context.log, COLOR_ERR, _("*** DIFF IS NOT CLEAN ***\n"));
	appendf(context.log, COLOR_ERR, _("Something went wrong during upload,\n"));
	appendf(context.log, COLOR_ERR, _("proceed with care!\n"));
      }

      /* redraw the entire map by destroying all map items and redrawing them */
      appendf(context.log, O2G_NULLPTR, _("Cleaning up ...\n"));
      diff_save(appdata.project, appdata.osm);
      appdata.map->clear(map_t::MAP_LAYER_OBJECTS_ONLY);
      delete appdata.osm;

      appendf(context.log, O2G_NULLPTR, _("Loading OSM ...\n"));
      appdata.osm = appdata.project->parse_osm(appdata.icons);
      appendf(context.log, O2G_NULLPTR, _("Applying diff ...\n"));
      diff_restore(appdata);
      appendf(context.log, O2G_NULLPTR, _("Painting ...\n"));
      appdata.map->paint();
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
