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
 * along with OSM2Go.  If not, see <https://www.gnu.org/licenses/>.
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
#include "xml_helpers.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <curl/curl.h>
#include <curl/easy.h>
#include <map>
#include <memory>
#include <sys/stat.h>
#include <unistd.h>

#ifdef FREMANTLE
#include <hildon/hildon-text-view.h>
#endif

#include "osm2go_annotations.h"
#include <osm2go_cpp.h>
#include <osm2go_i18n.h>
#include <osm2go_stl.h>
#include <osm2go_platform_gtk.h>

#define COLOR_ERR  "red"
#define COLOR_OK   "darkgreen"

class osm_upload_context_t {
  GtkTextBuffer * const logbuffer;
public:
  osm_upload_context_t(appdata_t &a, osm_t *o, project_t *p, const char *c, const char *s)
    : logbuffer(gtk_text_buffer_new(O2G_NULLPTR))
    , appdata(a)
    , osm(o)
    , project(p)
    , urlbasestr(p->server(a.settings->server) + "/")
    , logview(GTK_TEXT_VIEW(gtk_text_view_new_with_buffer(logbuffer)))
    , comment(c)
    , src(s ? s : std::string())
  {}

  appdata_t &appdata;
  osm_t * const osm;
  project_t * const project;
  const std::string urlbasestr; ///< API base URL, will always end in '/'

  GtkTextView * const logview;

  std::string changeset;

  std::string comment;
  const std::string src;
  std::unique_ptr<CURL, curl_deleter> curl;

  void appendf(const char *colname, const char *fmt, ...) __attribute__((format (printf, 3, 4)));
};

bool osm_download(GtkWidget *parent, settings_t *settings, project_t *project)
{
  printf("download osm for %s ...\n", project->name.c_str());

  if(unlikely(!project->rserver.empty())) {
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

    if(project->rserver == settings->server)
      project->rserver.clear();
  }

  const std::string url = project->server(settings->server) + "/map?bbox=" +
                          project->bounds.print(',');

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
  const bool wasGzip = project->osmFile.size() > 3 && strcmp(project->osmFile.c_str() + project->osmFile.size() - 3, ".gz") == 0;

  // check the contents of the new file
  g_mapped_file osmData(g_mapped_file_new(update.c_str(), FALSE, O2G_NULLPTR));
  if(unlikely(!osmData)) {
    messagef(parent, _("Download error"),
             _("Error accessing the downloaded file:\n\n%s"), update.c_str());
    unlink(update.c_str());
    return false;
  }

  const bool isGzip = check_gzip(g_mapped_file_get_contents(osmData.get()),
                              g_mapped_file_get_length(osmData.get()));
  osmData.reset();

  /* if there's a new file use this from now on */
  printf("download ok, replacing previous file\n");

  if(wasGzip != isGzip) {
    const std::string oldfname = (project->osmFile[0] == '/' ? std::string() : project->path) +
                                 project->osmFile;
    std::string newfname = oldfname;
    if(wasGzip)
      newfname.erase(newfname.size() - 3);
    else
      newfname += ".gz";
    rename(update.c_str(), newfname.c_str());
    // save the project before deleting the old file so that a valid file is always found
    if(newfname.substr(0, project->path.size()) == project->path)
      newfname.erase(0, project->path.size());
    project->osmFile = newfname;
    project->save(parent);

    // now remove the old file
    unlink(oldfname.c_str());
  } else {
    if(project->osmFile[0] == '/') {
      rename(update.c_str(), project->osmFile.c_str());
    } else {
      const std::string fname = project->path + project->osmFile;
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

void osm_upload_context_t::appendf(const char *colname, const char *fmt, ...) {
  va_list args;
  va_start( args, fmt );
  g_string buf(g_strdup_vprintf(fmt, args));
  va_end( args );

  printf("%s", buf.get());

  GtkTextIter end;
  gtk_text_buffer_get_end_iter(logbuffer, &end);
  if(colname) {
    GtkTextTag *tag = gtk_text_buffer_create_tag(logbuffer, O2G_NULLPTR,
                                                 "foreground", colname,
                                                 O2G_NULLPTR);
    gtk_text_buffer_insert_with_tags(logbuffer, &end, buf.get(), -1, tag, O2G_NULLPTR);
  } else
    gtk_text_buffer_insert(logbuffer, &end, buf.get(), -1);

  gtk_text_view_scroll_to_iter(logview, &end, 0.0, FALSE, 0, 0);

  osm2go_platform::process_events();
}

#define MAX_TRY 5

static CURL *curl_custom_setup(const std::string &credentials)
{
  /* get a curl handle */
  CURL *curl = curl_easy_init();
  if(!curl)
    return curl;

  /* we want to use our own write function */
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);

  /* some servers don't like requests that are made without a user-agent
     field, so we provide one */
  curl_easy_setopt(curl, CURLOPT_USERAGENT, PACKAGE "-libcurl/" VERSION);

  /* set user name and password for the authentication */
  curl_easy_setopt(curl, CURLOPT_USERPWD, credentials.c_str());

#ifndef CURL_SSLVERSION_MAX_DEFAULT
#define CURL_SSLVERSION_MAX_DEFAULT 0
#endif
  curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1 |
                   CURL_SSLVERSION_MAX_DEFAULT);

  return curl;
}

static bool osm_update_item(osm_upload_context_t &context, xmlChar *xml_str,
                            const char *url, item_id_t *id) {
  char buffer[CURL_ERROR_SIZE];

  std::unique_ptr<CURL, curl_deleter> &curl = context.curl;
  CURLcode res;

  /* specify target URL, and note that this URL should include a file
     name, not only a directory */
  curl_easy_setopt(curl.get(), CURLOPT_URL, url);

  /* enable uploading */
  curl_easy_setopt(curl.get(), CURLOPT_UPLOAD, 1L);

  /* we want to use our own read function */
  curl_easy_setopt(curl.get(), CURLOPT_READFUNCTION, read_callback);

  std::unique_ptr<curl_slist, curl_slist_deleter> slist(curl_slist_append(O2G_NULLPTR, "Expect:"));
  curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, slist.get());

  curl_data_t read_data_init(reinterpret_cast<char *>(xml_str));
  read_data_init.len = read_data_init.ptr ? strlen(read_data_init.ptr) : 0;

  for(int retry = MAX_TRY; retry >= 0; retry--) {
    if(retry != MAX_TRY)
      context.appendf(O2G_NULLPTR, _("Retry %d/%d "), MAX_TRY-retry, MAX_TRY-1);

    curl_data_t read_data = read_data_init;
    curl_data_t write_data;

    /* now specify which file to upload */
    curl_easy_setopt(curl.get(), CURLOPT_READDATA, &read_data);

    /* provide the size of the upload */
    curl_easy_setopt(curl.get(), CURLOPT_INFILESIZE, read_data.len);

    /* we pass our 'chunk' struct to the callback function */
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &write_data);

    curl_easy_setopt(curl.get(), CURLOPT_ERRORBUFFER, buffer);

    /* Now run off and do what you've been told! */
    res = curl_easy_perform(curl.get());

    long response;
    curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &response);

    if(unlikely(res != 0)) {
      context.appendf(COLOR_ERR, _("failed: %s\n"), buffer);
    } else if(unlikely(response != 200)) {
      context.appendf(COLOR_ERR, _("failed, code: %ld %s\n"), response,
              http_message(response));
      /* if it's neither "ok" (200), nor "internal server error" (500) */
      /* then write the message to the log */
      if(response != 500 && write_data.ptr) {
        context.appendf(O2G_NULLPTR, _("Server reply: "));
        context.appendf(COLOR_ERR, _("%s\n"), write_data.ptr);
      }
    } else if(unlikely(!id)) {
      context.appendf(COLOR_OK, _("ok\n"));
    } else {
      /* this will return the id on a successful create */
      printf("request to parse successful reply as an id\n");
      *id = strtoull(write_data.ptr, O2G_NULLPTR, 10);
      context.appendf(COLOR_OK, _("ok: #" ITEM_ID_FORMAT "\n"), *id);
    }

    g_free(write_data.ptr);

    /* don't retry unless we had an "internal server error" */
    if(response != 500)
      return((res == 0)&&(response == 200));
  }

  return false;
}

static bool osm_delete_item(osm_upload_context_t &context, xmlChar *xml_str,
                            int len, const char *url) {
  char buffer[CURL_ERROR_SIZE];

  std::unique_ptr<CURL, curl_deleter> &curl = context.curl;
  CURLcode res;

  /* specify target URL, and note that this URL should include a file
     name, not only a directory */
  curl_easy_setopt(curl.get(), CURLOPT_URL, url);

  /* no read function required */
  curl_easy_setopt(curl.get(), CURLOPT_POST, 1);

  curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDS, xml_str);
  curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDSIZE, len);

  std::unique_ptr<curl_slist, curl_slist_deleter> slist(curl_slist_append(O2G_NULLPTR, "Expect:"));
  curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, slist.get());

  curl_easy_setopt(curl.get(), CURLOPT_ERRORBUFFER, buffer);

  for(int retry = MAX_TRY; retry >= 0; retry--) {
    if(retry != MAX_TRY)
      context.appendf(O2G_NULLPTR, _("Retry %d/%d "), MAX_TRY-retry, MAX_TRY-1);

    curl_data_t write_data;

    /* we pass our 'chunk' struct to the callback function */
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &write_data);

    /* Now run off and do what you've been told! */
    res = curl_easy_perform(curl.get());

    long response;
    curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &response);

    if(res != 0)
      context.appendf(COLOR_ERR, _("failed: %s\n"), buffer);
    else if(response != 200)
      context.appendf(COLOR_ERR, _("failed, code: %ld %s\n"),
	      response, http_message(response));
    else
      context.appendf(COLOR_OK, _("ok\n"));

    /* if it's neither "ok" (200), nor "internal server error" (500) */
    /* then write the message to the log */
    if((response != 200) && (response != 500) && write_data.ptr) {
      context.appendf(O2G_NULLPTR, _("Server reply: "));
      context.appendf(COLOR_ERR, _("%s\n"), write_data.ptr);
    }

    g_free(write_data.ptr);

    /* don't retry unless we had an "internal server error" */
    if(response != 500)
      return((res == 0)&&(response == 200));
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
    context.appendf(O2G_NULLPTR, _("New %s "), obj->apiString());
  } else {
    url += obj->id_string();
    context.appendf(O2G_NULLPTR, _("Modified %s #" ITEM_ID_FORMAT " "), obj->apiString(), obj->id);
  }

  /* upload this object */
  xmlString xml_str(obj->generate_xml(context.changeset));
  if(xml_str) {
    printf("uploading %s " ITEM_ID_FORMAT " to %s\n", obj->apiString(), obj->id, url.c_str());

    if(osm_update_item(context, xml_str.get(), url.c_str(), obj->isNew() ? &obj->id : &obj->version)) {
      obj->flags ^= OSM_FLAG_DIRTY;
      context.project->data_dirty = true;
    }
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

static GtkWidget *table_attach_label_c(GtkWidget *table, const char *str,
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
  char str[G_ASCII_DTOSTR_BUF_SIZE];
  snprintf(str, sizeof(str), "%d", num);
  return table_attach_label_c(table, str, x1, x2, y1, y2);
}

static void log_deletion(osm_upload_context_t &context, const base_object_t *obj) {
  assert(obj->flags & OSM_FLAG_DELETED);

  context.appendf(O2G_NULLPTR, _("Deleted %s #" ITEM_ID_FORMAT " (version " ITEM_ID_FORMAT ")\n"),
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

  context.appendf(O2G_NULLPTR, _("Uploading object deletions "));

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
  context.appendf(O2G_NULLPTR, _("Create changeset "));

  /* create changeset request */
  xmlString xml_str(osm_generate_xml_changeset(context.comment, context.src));
  if(xml_str) {
    printf("creating changeset %s from address %p\n", url.c_str(), xml_str.get());

    item_id_t changeset;
    if(osm_update_item(context, xml_str.get(), url.c_str(), &changeset)) {
      char str[32];
      snprintf(str, sizeof(str), ITEM_ID_FORMAT, changeset);
      printf("got changeset id %s\n", str);
      context.changeset = str;
      result = true;
    }
  }

  return result;
}

static bool osm_close_changeset(osm_upload_context_t &context) {
  assert(!context.changeset.empty());

  /* make sure gui gets updated */
  osm2go_platform::process_events();

  const std::string url = context.urlbasestr + "changeset/" + context.changeset +
                          "/close";
  context.appendf(O2G_NULLPTR, _("Close changeset "));

  return osm_update_item(context, O2G_NULLPTR, url.c_str(), O2G_NULLPTR);
}

/* comment buffer has been edited, allow upload if the buffer is not empty */
static void callback_buffer_modified(GtkTextBuffer *buffer, GtkDialog *dialog) {
  GtkTextIter start, end;
  gtk_text_buffer_get_start_iter(buffer, &start);
  gtk_text_buffer_get_end_iter(buffer, &end);
  char *text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
  gtk_dialog_set_response_sensitive(dialog, GTK_RESPONSE_ACCEPT,
                                    (text && strlen(text) > 0) ? TRUE : FALSE);
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
  g_widget dialog(gtk_dialog_new_with_buttons(_("Changeset details"),
                                              GTK_WINDOW(parent), GTK_DIALOG_MODAL,
                                              GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                              O2G_NULLPTR));

  dialog_size_hint(GTK_WINDOW(dialog.get()), MISC_DIALOG_SMALL);
  gtk_dialog_set_default_response(GTK_DIALOG(dialog.get()), GTK_RESPONSE_CANCEL);

  details_table(dialog.get(), context);
  gtk_widget_show_all(dialog.get());
  gtk_dialog_run(GTK_DIALOG(dialog.get()));
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

  g_widget dialog(gtk_dialog_new_with_buttons(_("Upload to OSM"),
                                              GTK_WINDOW(appdata.window),
                                              GTK_DIALOG_MODAL,
#ifdef FREMANTLE
                                              _("More"), GTK_RESPONSE_HELP,
#endif
                                              GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
                                              GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                                              O2G_NULLPTR));

  dialog_size_hint(GTK_WINDOW(dialog.get()), MISC_DIALOG_MEDIUM);

#ifndef FREMANTLE
  details_table(dialog.get(), dirty);

  /* ------------------------------------------------------ */

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog.get())->vbox),
		     gtk_hseparator_new(), FALSE, FALSE, 0);
#endif

  /* ------- add username and password entries ------------ */

  GtkWidget *table = gtk_table_new(2, 2, FALSE);
  table_attach_label_l(table, _("Username:"), 0, 1, 0, 1);
  GtkWidget *uentry = entry_new(EntryFlagsNoAutoCap);
  const char *username = !appdata.settings->username.empty() ?
                         appdata.settings->username.c_str() :
                         _("<your osm username>");
#ifndef FREMANTLE
  gtk_entry_set_text(GTK_ENTRY(uentry), username);
#else
  if(appdata.settings->username.empty())
    hildon_gtk_entry_set_placeholder_text(GTK_ENTRY(uentry), username);
  else
    gtk_entry_set_text(GTK_ENTRY(uentry), username);
#endif
  gtk_table_attach_defaults(GTK_TABLE(table),  uentry, 1, 2, 0, 1);
  table_attach_label_l(table, _("Password:"), 0, 1, 1, 2);
  GtkWidget *pentry = entry_new(EntryFlagsNoAutoCap);
  if(!appdata.settings->password.empty())
    gtk_entry_set_text(GTK_ENTRY(pentry), appdata.settings->password.c_str());
  gtk_entry_set_visibility(GTK_ENTRY(pentry), FALSE);
  gtk_table_attach_defaults(GTK_TABLE(table),  pentry, 1, 2, 1, 2);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog.get())->vbox), table, FALSE, FALSE, 0);

  table_attach_label_l(table, _("Source:"), 0, 1, 2, 3);
  GtkWidget *sentry = entry_new(EntryFlagsNoAutoCap);
  gtk_table_attach_defaults(GTK_TABLE(table),  sentry, 1, 2, 2, 3);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog.get())->vbox), table, FALSE, FALSE, 0);

  GtkTextBuffer *buffer = gtk_text_buffer_new(O2G_NULLPTR);
  const char *placeholder_comment = _("Please add a comment");

  /* disable ok button until user edited the comment */
  gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog.get()), GTK_RESPONSE_ACCEPT, FALSE);

  g_signal_connect(buffer, "changed", G_CALLBACK(callback_buffer_modified), dialog.get());

  GtkTextView *view = GTK_TEXT_VIEW(
#ifndef FREMANTLE
                    gtk_text_view_new_with_buffer(buffer));
  gtk_text_buffer_set_text(buffer, placeholder_comment, -1);
#else
                    hildon_text_view_new());
  gtk_text_view_set_buffer(view, buffer);
  hildon_gtk_text_view_set_placeholder_text(view, placeholder_comment);
#endif

  gtk_text_view_set_wrap_mode(view, GTK_WRAP_WORD);
  gtk_text_view_set_editable(view, TRUE);
  gtk_text_view_set_left_margin(view, 2 );
  gtk_text_view_set_right_margin(view, 2 );

  g_object_set_data(G_OBJECT(view), "first_click", GINT_TO_POINTER(TRUE));
  g_signal_connect(view, "focus-in-event", G_CALLBACK(cb_focus_in), buffer);

  gtk_box_pack_start_defaults(GTK_BOX(GTK_DIALOG(dialog.get())->vbox),
                              osm2go_platform::scrollable_container(GTK_WIDGET(view)));
  gtk_widget_show_all(dialog.get());

  bool done = false;
  while(!done) {
    switch(gtk_dialog_run(GTK_DIALOG(dialog.get()))) {
#ifdef FREMANTLE
    case GTK_RESPONSE_HELP:
      info_more(dirty, dialog.get());
      break;
#endif
    case GTK_RESPONSE_ACCEPT:
      done = true;
      break;
    default:
      printf("upload cancelled\n");
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

  dialog.reset();
  project->save(appdata.window);

  dialog.reset(gtk_dialog_new_with_buttons(_("Uploading"), GTK_WINDOW(appdata.window),
                                           GTK_DIALOG_MODAL,
                                           GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                                           O2G_NULLPTR));

  dialog_size_hint(GTK_WINDOW(dialog.get()), MISC_DIALOG_LARGE);
  gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog.get()),
				    GTK_RESPONSE_CLOSE, FALSE);

  /* ------- main ui element is this text view --------------- */

  /* create a scrolled window */
  GtkScrolledWindow *scrolled_window = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(O2G_NULLPTR,
                                                                                   O2G_NULLPTR));
  gtk_scrolled_window_set_policy(scrolled_window, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  gtk_scrolled_window_set_shadow_type(scrolled_window, GTK_SHADOW_IN);

  gtk_text_view_set_editable(context.logview, FALSE);
  gtk_text_view_set_cursor_visible(context.logview, FALSE);
  gtk_text_view_set_wrap_mode(context.logview, GTK_WRAP_WORD);

  gtk_container_add(GTK_CONTAINER(scrolled_window), GTK_WIDGET(context.logview));

  gtk_box_pack_start_defaults(GTK_BOX(GTK_DIALOG(dialog.get())->vbox), GTK_WIDGET(scrolled_window));
  gtk_widget_show_all(dialog.get());

  context.appendf(O2G_NULLPTR, _("Log generated by %s v%s using API 0.6\n"),
	  PACKAGE, VERSION);
  context.appendf(O2G_NULLPTR, _("User comment: %s\n"), context.comment.c_str());

  if(api_adjust(project->rserver)) {
    context.appendf(O2G_NULLPTR, _("Server URL adjusted to %s\n"),
            project->rserver.c_str());
    if(likely(project->rserver == context.appdata.settings->server))
      project->rserver.clear();
  }

  context.appendf(O2G_NULLPTR, _("Uploading to %s\n"),
          project->server(appdata.settings->server).c_str());

  /* get a curl handle */
  context.curl.reset(curl_custom_setup(appdata.settings->username + ":" +
                                       appdata.settings->password));

  if(unlikely(!context.curl)) {
    context.appendf(O2G_NULLPTR, _("CURL init error\n"));
  } else if(likely(osm_create_changeset(context))) {
    /* check for dirty entries */
    if(!dirty.nodes.modified.empty()) {
      context.appendf(O2G_NULLPTR, _("Uploading nodes:\n"));
      std::for_each(dirty.nodes.modified.begin(), dirty.nodes.modified.end(),
                    upload_objects<node_t>(context, osm->nodes));
    }
    if(!dirty.ways.modified.empty()) {
      context.appendf(O2G_NULLPTR, _("Uploading ways:\n"));
      std::for_each(dirty.ways.modified.begin(), dirty.ways.modified.end(),
                    upload_objects<way_t>(context, osm->ways));
    }
    if(!dirty.relations.modified.empty()) {
      context.appendf(O2G_NULLPTR, _("Uploading relations:\n"));
      std::for_each(dirty.relations.modified.begin(), dirty.relations.modified.end(),
                    upload_objects<relation_t>(context, osm->relations));
    }
    if(!dirty.relations.deleted.empty() || !dirty.ways.deleted.empty() || !dirty.nodes.deleted.empty()) {
      context.appendf(O2G_NULLPTR, _("Deleting objects:\n"));
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
    context.curl.reset();
  }

  context.appendf(O2G_NULLPTR, _("Upload done.\n"));

  bool reload_map = false;
  if(project->data_dirty) {
    context.appendf(O2G_NULLPTR, _("Server data has been modified.\n"
                                        "Downloading updated osm data ...\n"));

    if(osm_download(dialog.get(), appdata.settings, project)) {
      context.appendf(O2G_NULLPTR, _("Download successful!\n"
                                          "The map will be reloaded.\n"));
      project->data_dirty = false;
      reload_map = true;
    } else
      context.appendf(O2G_NULLPTR, _("Download failed!\n"));

    project->save(dialog.get());

    if(reload_map) {
      /* this kind of rather brute force reload is useful as the moment */
      /* after the upload is a nice moment to bring everything in sync again. */
      /* we basically restart the entire map with fresh data from the server */
      /* and the diff will hopefully be empty (if the upload was successful) */

      context.appendf(O2G_NULLPTR, _("Reloading map ...\n"));

      if(!appdata.osm->is_clean(false))
        context.appendf(COLOR_ERR, _("*** DIFF IS NOT CLEAN ***\n"
                                          "Something went wrong during upload,\n"
                                          "proceed with care!\n"));

      /* redraw the entire map by destroying all map items and redrawing them */
      context.appendf(O2G_NULLPTR, _("Cleaning up ...\n"));
      diff_save(appdata.project, appdata.osm);
      appdata.map->clear(map_t::MAP_LAYER_OBJECTS_ONLY);
      delete appdata.osm;

      context.appendf(O2G_NULLPTR, _("Loading OSM ...\n"));
      appdata.osm = appdata.project->parse_osm();
      context.appendf(O2G_NULLPTR, _("Applying diff ...\n"));
      diff_restore(appdata);
      context.appendf(O2G_NULLPTR, _("Painting ...\n"));
      appdata.map->paint();
      context.appendf(O2G_NULLPTR, _("Done!\n"));
    }
  }

  /* tell the user that he can stop waiting ... */
  context.appendf(O2G_NULLPTR, _("Process finished.\n"));

  gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog.get()),
				    GTK_RESPONSE_CLOSE, TRUE);

  gtk_dialog_run(GTK_DIALOG(dialog.get()));
}
