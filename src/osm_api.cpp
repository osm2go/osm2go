/*
 * SPDX-FileCopyrightText: 2008 Till Harbaum <till@harbaum.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "osm_api.h"
#include "osm_api_p.h"

#include "appdata.h"
#include "diff.h"
#include "map.h"
#include "misc.h"
#include "net_io.h"
#include "notifications.h"
#include "osm.h"
#include "osm2go_platform.h"
#include "project.h"
#include "settings.h"
#include "uicontrol.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <curl/curl.h>
#include <curl/easy.h>
#include <filesystem>
#include <map>
#include <memory>
#include <sys/stat.h>
#include <unistd.h>

#include "osm2go_annotations.h"
#include <osm2go_cpp.h>
#include <osm2go_i18n.h>
#include <osm2go_stl.h>

#define COLOR_ERR  "red"
#define COLOR_OK   "darkgreen"

bool osm_download(osm2go_platform::Widget *parent, project_t *project)
{
  printf("download osm for %s ...\n", project->name.c_str());
  settings_t::ref settings = settings_t::instance();
  const std::string &defaultServer = settings->server;

  if(unlikely(!project->rserver.empty())) {
    if(api_adjust(project->rserver)) {
      message_dlg(_("Server changed"),
                  trstring("It seems your current project uses an outdated "
                           "server/protocol. It has thus been changed to:\n\n%1").arg(project->rserver),
                  parent);
    }

    /* server url should not end with a slash */
    if(unlikely(ends_with(project->rserver, '/'))) {
      printf("removing trailing slash\n");
      project->rserver.erase(project->rserver.size() - 1);
    }

    if(project->rserver == defaultServer)
      project->rserver.clear();
  }

  const std::string url = project->server(defaultServer) + "/map?bbox=" +
                          project->bounds.print();

  /* Download the new file to a new name. If something goes wrong then the
   * old file will still be in place to be opened. */
  const char *updatefn = "update.osm";
  const std::string update = project->path + updatefn;
  unlinkat(project->dirfd, updatefn, 0);

  if(unlikely(!net_io_download_file(parent, url, update, project->name, true)))
    return false;

  if(unlikely(!std::filesystem::is_regular_file(update)))
    return false;

  // if the project's gzip setting and the download one don't match change the project
  const bool wasGzip = ends_with(project->osmFile, ".gz");

  // check the contents of the new file
  osm2go_platform::MappedFile osmData(update);
  if(unlikely(!osmData)) {
    error_dlg(trstring("Error accessing the downloaded file:\n\n%1").arg(update), parent);
    unlink(update.c_str());
    return false;
  }

  const bool isGzip = check_gzip(osmData.data(), osmData.length());
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
    if(newfname.compare(0, project->path.size(), project->path) == 0)
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

namespace {

struct curl_data_t {
  explicit curl_data_t(char *p = nullptr, curl_off_t l = 0)
    : ptr(p), len(l) {}
  char *ptr;
  long len;
};

size_t
read_callback(void *ptr, size_t size, size_t nmemb, void *stream)
{
  curl_data_t *p = static_cast<curl_data_t *>(stream);

//   printf("request to read %zu items of size %zu, pointer = %p\n",
//      nmemb, size, p->ptr);

  if(nmemb * size > static_cast<size_t>(p->len))
    nmemb = p->len/size;

  memcpy(ptr, p->ptr, size*nmemb);
  p->ptr += size*nmemb;
  p->len -= size*nmemb;

  return nmemb;
}

size_t
write_callback(void *ptr, size_t size, size_t nmemb, void *stream)
{
  std::string *p = static_cast<std::string *>(stream);

  p->append(static_cast<char *>(ptr), size * nmemb);
  return nmemb;
}

#define MAX_TRY 5

CURL *
curl_custom_setup(const std::string &credentials)
{
  /* get a curl handle */
  CURL *curl = curl_easy_init();
  if(curl == nullptr)
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

bool
osm_update_item(osm_upload_context_t &context, xmlChar *xml_str,
                            const char *url, item_id_t *id)
{
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

  std::unique_ptr<curl_slist, curl_slist_deleter> slist(curl_slist_append(nullptr, "Expect:"));
  curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, slist.get());

  curl_data_t read_data_init(reinterpret_cast<char *>(xml_str));
  read_data_init.len = read_data_init.ptr != nullptr ? strlen(read_data_init.ptr) : 0;

  curl_data_t read_data = read_data_init;
  std::string write_data;

  /* now specify which file to upload */
  curl_easy_setopt(curl.get(), CURLOPT_READDATA, &read_data);

  /* provide the size of the upload */
  curl_easy_setopt(curl.get(), CURLOPT_INFILESIZE, read_data.len);

  /* we pass our 'chunk' struct to the callback function */
  curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &write_data);

  curl_easy_setopt(curl.get(), CURLOPT_ERRORBUFFER, buffer);

  for(int retry = MAX_TRY; retry >= 0; retry--) {
    if(retry != MAX_TRY)
      context.append(trstring("Retry %1/%2 ").arg(MAX_TRY - retry).arg(MAX_TRY - 1));

    read_data = read_data_init;
    write_data.clear();

    /* Now run off and do what you've been told! */
    res = curl_easy_perform(curl.get());

    long response;
    curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &response);

    if(unlikely(res != 0)) {
      context.append(trstring("failed: %1\n").arg(buffer), COLOR_ERR);
    } else if(unlikely(response != 200)) {
      context.append(trstring("failed, code: %1 %2\n").arg(response).arg(http_message(response)),
                     COLOR_ERR);
      /* if it's neither "ok" (200), nor "internal server error" (500) */
      /* then write the message to the log */
      if(response != 500 && !write_data.empty()) {
        context.append(_("Server reply: "));
        context.append_str(write_data.c_str(), COLOR_ERR);
        context.append_str("\n");
      }
    } else if(unlikely(!id)) {
      context.append(_("ok\n"), COLOR_OK);
    } else {
      /* this will return the id on a successful create */
      printf("request to parse successful reply '%s' as an id\n", write_data.c_str());
      *id = strtoull(write_data.c_str(), nullptr, 10);
      context.append(trstring("ok: #%1\n").arg(*id), COLOR_OK);
    }

    /* don't retry unless we had an "internal server error" */
    if(response != 500)
      return((res == 0)&&(response == 200));
  }

  return false;
}

bool
osm_post_xml(osm_upload_context_t &context, const xmlString &xml_str, int len,
             const char *url, std::string &write_data)
{
  char buffer[CURL_ERROR_SIZE];

  std::unique_ptr<CURL, curl_deleter> &curl = context.curl;
  CURLcode res;

  // drop now unneeded values from the previous transfers
  curl_easy_setopt(curl.get(), CURLOPT_READFUNCTION, nullptr);
  curl_easy_setopt(curl.get(), CURLOPT_READDATA, nullptr);
  curl_easy_setopt(curl.get(), CURLOPT_INFILESIZE, -1);
  curl_easy_setopt(curl.get(), CURLOPT_UPLOAD, 0);

  /* specify target URL, and note that this URL should include a file
     name, not only a directory */
  curl_easy_setopt(curl.get(), CURLOPT_URL, url);

  /* no read function required */
  curl_easy_setopt(curl.get(), CURLOPT_POST, 1);

  curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDS, xml_str.get());
  curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDSIZE, len);

  std::unique_ptr<curl_slist, curl_slist_deleter> slist(curl_slist_append(nullptr, "Expect:"));
  curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, slist.get());

  curl_easy_setopt(curl.get(), CURLOPT_ERRORBUFFER, buffer);

  /* we pass our 'chunk' struct to the callback function */
  curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &write_data);

  for(int retry = MAX_TRY; retry >= 0; retry--) {
    if(retry != MAX_TRY)
      context.append(trstring("Retry %1/%2 ").arg(MAX_TRY - retry).arg(MAX_TRY - 1));

    write_data.clear();

    /* Now run off and do what you've been told! */
    res = curl_easy_perform(curl.get());

    long response;
    curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &response);

    if(unlikely(res != 0))
      context.append(trstring("failed: %1\n").arg(buffer), COLOR_ERR);
    else if(unlikely(response != 200))
      context.append(trstring("failed, code: %1 %2\n").arg(response).arg(http_message(response)),
                     COLOR_ERR);
    else
      context.append(_("ok\n"), COLOR_OK);

    /* don't retry unless we had an "internal server error" */
    if(response != 500)
      return((res == 0)&&(response == 200));
  }

  return false;
}

/**
 * @brief upload one object to the OSM server
 * @returns if object has been updated
 */
bool
upload_object(osm_upload_context_t &context, base_object_t *obj, bool is_new)
{
  /* make sure gui gets updated */
  osm2go_platform::process_events();

  assert(obj->flags & OSM_FLAG_DIRTY);

  std::string url = context.urlbasestr + obj->apiString() + '/';

  if(is_new) {
    url += "create";
    context.append(trstring("New %1 ").arg(obj->apiString()));
  } else {
    url += obj->id_string();
    context.append(trstring("Modified %1 #%2 ").arg(obj->apiString()).arg(obj->id));
  }

  /* upload this object */
  xmlString xml_str(obj->generate_xml(context.changeset));
  if(likely(xml_str)) {
    printf("uploading %s " ITEM_ID_FORMAT " to %s\n", obj->apiString(), obj->id, url.c_str());

    item_id_t tmp;
    if(osm_update_item(context, xml_str.get(), url.c_str(), is_new ? &obj->id : &tmp)) {
      if(!is_new)
        obj->version = tmp;
      context.project->data_dirty = true;
      return true;
    }
  }

  return false;
}

template<typename T>
struct upload_objects {
  osm_upload_context_t &context;
  std::map<item_id_t, T *> &map;
  const bool is_new;
  upload_objects(osm_upload_context_t &co, std::map<item_id_t, T*> &m, bool n)
    : context(co), map(m), is_new(n) {}
  void operator()(T *obj);
};

template<typename T>
void upload_objects<T>::operator()(T *obj)
{
  item_id_t oldid = obj->id;
  if (upload_object(context, obj, is_new))
    context.osm->unmark_dirty(obj);
  if(oldid != obj->id) {
    map.erase(oldid);
    map[obj->id] = obj;
  }
}

template<typename T>
static void upload_modified(osm_upload_context_t &co, trstring::native_type_arg header,
                            std::map<item_id_t, T*> &m, const osm_t::dirty_t::counter<T> &counter)
{
  if(counter.changed.empty() && counter.added.empty())
    return;

  co.append(header);
  std::for_each(counter.added.begin(), counter.added.end(),
                upload_objects<T>(co, m, true));
  std::for_each(counter.changed.begin(), counter.changed.end(),
                upload_objects<T>(co, m, false));
}

void
log_deletion(osm_upload_context_t &context, const base_object_t *obj)
{
  assert(obj->isDeleted());

  context.append(trstring("Deleted %1 #%2 (version %3)\n").arg(obj->apiString()).arg(obj->id)
                                                         .arg(obj->version));
}

struct osm_delete_objects_final {
  osm_upload_context_t &context;
  explicit osm_delete_objects_final(osm_upload_context_t &c)
    : context(c) {}
  template<typename T>
  void operator()(T *o)
  {
    log_deletion(context, o);
    context.osm->wipe(o);
  }
};

/**
 * @brief upload the given osmChange document
 * @param context the context pointer
 * @param doc the document to upload
 * @param server_reply the data returned from the server will be stored here
 * @returns if the operation was successful
 */
bool
osmchange_upload(osm_upload_context_t &context, xmlDocGuard &doc, std::string &server_reply)
{
  /* make sure gui gets updated */
  osm2go_platform::process_events();

  const std::string url = context.urlbasestr + "changeset/" + context.changeset + "/upload";

  xmlChar *xml_str = nullptr;
  int len = 0;

  xmlDocDumpFormatMemoryEnc(doc.get(), &xml_str, &len, "UTF-8", 1);
  xmlString xml(xml_str);

  bool ret = osm_post_xml(context, xml, len, url.c_str(), server_reply);
  if(ret)
    context.project->data_dirty = true;

  return ret;
}

bool
osm_create_changeset(osm_upload_context_t &context)
{
  bool result = false;

  /* make sure gui gets updated */
  osm2go_platform::process_events();

  const std::string url = context.urlbasestr + "changeset/create";
  context.append(_("Create changeset "));

  /* create changeset request */
  xmlString xml_str(osm_generate_xml_changeset(context.comment, context.src));
  if(xml_str) {
    printf("creating changeset %s from address %p\n", url.c_str(), xml_str.get());

    item_id_t changeset;
    if(osm_update_item(context, xml_str.get(), url.c_str(), &changeset)) {
      context.changeset = std::to_string(changeset);
      result = true;
    }
  }

  return result;
}

bool
osm_close_changeset(osm_upload_context_t &context)
{
  assert(!context.changeset.empty());

  /* make sure gui gets updated */
  osm2go_platform::process_events();

  const std::string url = context.urlbasestr + "changeset/" + context.changeset +
                          "/close";
  context.append(_("Close changeset "));

  return osm_update_item(context, nullptr, url.c_str(), nullptr);
}

} // namespace

void osm_upload_context_t::upload(const osm_t::dirty_t &dirty, osm2go_platform::Widget *parent)
{
  append(trstring("Log generated by %1 v%2 using API 0.6\n").arg(PACKAGE).arg(VERSION));
  append(trstring("User comment: %1\n").arg(comment));
  if (!src.empty())
    append(trstring("User source: %1\n").arg(src));

  settings_t::ref settings = settings_t::instance();

  if(api_adjust(project->rserver)) {
    append(trstring("Server URL adjusted to %1\n").arg(project->rserver));
    if(likely(project->rserver == settings->server))
      project->rserver.clear();
  }

  append(trstring("Uploading to %1\n").arg(project->server(settings->server)));

  /* get a curl handle */
  curl.reset(curl_custom_setup(settings->username + ':' + settings->password));

  if(unlikely(!curl)) {
    append(_("CURL init error\n"));
  } else if(likely(osm_create_changeset(*this))) {
    /* check for dirty entries */
    upload_modified(*this, _("Uploading nodes:\n"), osm->nodes, dirty.nodes);
    upload_modified(*this, _("Uploading ways:\n"), osm->ways, dirty.ways);
    upload_modified(*this, _("Uploading relations:\n"), osm->relations, dirty.relations);
    if(!dirty.relations.deleted.empty() || !dirty.ways.deleted.empty() || !dirty.nodes.deleted.empty()) {
      append(_("Deleting objects:\n"));
      xmlDocGuard doc(osmchange_init());
      osmchange_delete(dirty, xmlDocGetRootElement(doc.get()), changeset.c_str());

      printf("deleting objects on server\n");
      append(_("Uploading object deletions "));

      // deletion was successful, remove the objects
      std::string server_reply;
      if(osmchange_upload(*this, doc, server_reply)) {
        osm_delete_objects_final finfc(*this);
        std::for_each(dirty.relations.deleted.begin(), dirty.relations.deleted.end(), finfc);
        std::for_each(dirty.ways.deleted.begin(), dirty.ways.deleted.end(), finfc);
        std::for_each(dirty.nodes.deleted.begin(), dirty.nodes.deleted.end(), finfc);
      } else {
        append(_("Server reply: "));
        append_str(server_reply.c_str(), COLOR_ERR);
        append_str("\n");
      }
    }

    /* close changeset */
    osm_close_changeset(*this);
    curl.reset();

    append(_("Upload done.\n"));
  }

  if(project->data_dirty) {
    append(_("Server data has been modified.\nDownloading updated osm data ...\n"));

    bool reload_map = osm_download(parent, project.get());
    if(likely(reload_map)) {
      append(_("Download successful!\nThe map will be reloaded.\n"));
      project->data_dirty = false;
    } else
      append(_("Download failed!\n"));

    project->save(parent);

    if(likely(reload_map)) {
      /* this kind of rather brute force reload is useful as the moment */
      /* after the upload is a nice moment to bring everything in sync again. */
      /* we basically restart the entire map with fresh data from the server */
      /* and the diff will hopefully be empty (if the upload was successful) */

      append(_("Reloading map ...\n"));

      if(unlikely(!project->osm->is_clean(false)))
        append(_("*** DIFF IS NOT CLEAN ***\nSomething went wrong during "
                     "upload,\nproceed with care!\n"), COLOR_ERR);

      /* redraw the entire map by destroying all map items and redrawing them */
      append(_("Cleaning up ...\n"));
      project->diff_save();
      appdata.map->clear(map_t::MAP_LAYER_OBJECTS_ONLY);

      append(_("Loading OSM ...\n"));
      if(project->parse_osm()) {
        append(_("Applying diff ...\n"));
        diff_restore(project, appdata.uicontrol.get());
        append(_("Painting ...\n"));
        appdata.map->paint();
      } else {
        append(_("OSM data is empty\n"), COLOR_ERR);
      }
      append(_("Done!\n"));
    }
  }

  /* tell the user that he can stop waiting ... */
  append(_("Process finished.\n"));
}

void osm_upload(appdata_t &appdata)
{
  project_t::ref project = appdata.project;
  if(unlikely(project->osm->uploadPolicy == osm_t::Upload_Blocked)) {
    printf("Upload prohibited");
    return;
  }

  /* upload config and confirmation dialog */

  /* count objects */
  const osm_t::dirty_t &dirty = project->osm->modified();

  if(dirty.empty()) {
    appdata.uicontrol->showNotification(_("No changes present"), MainUi::Brief);
    return;
  }

  dirty.nodes.debug    ("nodes:    ");
  dirty.ways.debug     ("ways:     ");
  dirty.relations.debug("relations:");

  osm_upload_dialog(appdata, dirty);
}
