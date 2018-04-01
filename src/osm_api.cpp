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
#include "osm_api_p.h"

#include "appdata.h"
#include "diff.h"
#include "map.h"
#include "notifications.h"
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

#include "osm2go_annotations.h"
#include <osm2go_cpp.h>
#include <osm2go_i18n.h>
#include <osm2go_stl.h>

#define COLOR_ERR  "red"
#define COLOR_OK   "darkgreen"

bool osm_download(osm2go_platform::Widget *parent, project_t *project)
{
  printf("download osm for %s ...\n", project->name.c_str());
  const std::string &defaultServer = settings_t::instance()->server;

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

    if(project->rserver == defaultServer)
      project->rserver.clear();
  }

  const std::string url = project->server(defaultServer) + "/map?bbox=" +
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
  osm2go_platform::MappedFile osmData(update.c_str());
  if(unlikely(!osmData)) {
    messagef(parent, _("Download error"),
             _("Error accessing the downloaded file:\n\n%s"), update.c_str());
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
  explicit curl_data_t(char *p = nullptr, curl_off_t l = 0)
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
  std::string *p = static_cast<std::string *>(stream);

  p->append(static_cast<char *>(ptr), size * nmemb);
  return nmemb;
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

  std::unique_ptr<curl_slist, curl_slist_deleter> slist(curl_slist_append(nullptr, "Expect:"));
  curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, slist.get());

  curl_data_t read_data_init(reinterpret_cast<char *>(xml_str));
  read_data_init.len = read_data_init.ptr ? strlen(read_data_init.ptr) : 0;

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
      context.appendf(nullptr, _("Retry %d/%d "), MAX_TRY-retry, MAX_TRY-1);

    read_data = read_data_init;
    write_data.clear();

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
      if(response != 500 && !write_data.empty()) {
        context.append_str(_("Server reply: "));
        context.append_str(write_data.c_str(), COLOR_ERR);
        context.append_str("\n");
      }
    } else if(unlikely(!id)) {
      context.append_str(_("ok\n"), COLOR_OK);
    } else {
      /* this will return the id on a successful create */
      printf("request to parse successful reply '%s' as an id\n", write_data.c_str());
      *id = strtoull(write_data.c_str(), nullptr, 10);
      context.appendf(COLOR_OK, _("ok: #" ITEM_ID_FORMAT "\n"), *id);
    }

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

  std::unique_ptr<curl_slist, curl_slist_deleter> slist(curl_slist_append(nullptr, "Expect:"));
  curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, slist.get());

  curl_easy_setopt(curl.get(), CURLOPT_ERRORBUFFER, buffer);

  std::string write_data;

  /* we pass our 'chunk' struct to the callback function */
  curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &write_data);

  for(int retry = MAX_TRY; retry >= 0; retry--) {
    if(retry != MAX_TRY)
      context.appendf(nullptr, _("Retry %d/%d "), MAX_TRY-retry, MAX_TRY-1);

    write_data.clear();

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
      context.append_str(_("ok\n"), COLOR_OK);

    /* if it's neither "ok" (200), nor "internal server error" (500) */
    /* then write the message to the log */
    if((response != 200) && (response != 500) && !write_data.empty()) {
      context.append_str(_("Server reply: "));
      context.append_str(write_data.c_str(), COLOR_ERR);
      context.append_str("\n");
    }

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
    context.appendf(nullptr, _("New %s "), obj->apiString());
  } else {
    url += obj->id_string();
    context.appendf(nullptr, _("Modified %s #" ITEM_ID_FORMAT " "), obj->apiString(), obj->id);
  }

  /* upload this object */
  xmlString xml_str(obj->generate_xml(context.changeset));
  if(xml_str) {
    printf("uploading %s " ITEM_ID_FORMAT " to %s\n", obj->apiString(), obj->id, url.c_str());

    item_id_t tmp;
    if(osm_update_item(context, xml_str.get(), url.c_str(), obj->isNew() ? &obj->id : &tmp)) {
      if(obj->isNew())
        obj->version = tmp;
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

static void log_deletion(osm_upload_context_t &context, const base_object_t *obj) {
  assert(obj->flags & OSM_FLAG_DELETED);

  context.appendf(nullptr, _("Deleted %s #" ITEM_ID_FORMAT " (version %u)\n"),
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

  context.append_str(_("Uploading object deletions "));

  const std::string url = context.urlbasestr + "changeset/" + context.changeset + "/upload";

  xmlChar *xml_str = nullptr;
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
  context.append_str(_("Create changeset "));

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

static bool osm_close_changeset(osm_upload_context_t &context) {
  assert(!context.changeset.empty());

  /* make sure gui gets updated */
  osm2go_platform::process_events();

  const std::string url = context.urlbasestr + "changeset/" + context.changeset +
                          "/close";
  context.append_str(_("Close changeset "));

  return osm_update_item(context, nullptr, url.c_str(), nullptr);
}

void osm_do_upload(osm_upload_context_t &context, const osm_t::dirty_t &dirty)
{
  context.appendf(nullptr, _("Log generated by %s v%s using API 0.6\n"),
	  PACKAGE, VERSION);
  context.appendf(nullptr, _("User comment: %s\n"), context.comment.c_str());

  project_t * const project = context.project;
  settings_t * const settings = settings_t::instance();

  if(api_adjust(project->rserver)) {
    context.appendf(nullptr, _("Server URL adjusted to %s\n"),
            project->rserver.c_str());
    if(likely(project->rserver == settings->server))
      project->rserver.clear();
  }

  context.appendf(nullptr, _("Uploading to %s\n"),
          project->server(settings->server).c_str());

  /* get a curl handle */
  context.curl.reset(curl_custom_setup(settings->username + ":" + settings->password));

  if(unlikely(!context.curl)) {
    context.append_str(_("CURL init error\n"));
  } else if(likely(osm_create_changeset(context))) {
    /* check for dirty entries */
    if(!dirty.nodes.modified.empty()) {
      context.append_str(_("Uploading nodes:\n"));
      std::for_each(dirty.nodes.modified.begin(), dirty.nodes.modified.end(),
                    upload_objects<node_t>(context, context.osm->nodes));
    }
    if(!dirty.ways.modified.empty()) {
      context.append_str(_("Uploading ways:\n"));
      std::for_each(dirty.ways.modified.begin(), dirty.ways.modified.end(),
                    upload_objects<way_t>(context, context.osm->ways));
    }
    if(!dirty.relations.modified.empty()) {
      context.append_str(_("Uploading relations:\n"));
      std::for_each(dirty.relations.modified.begin(), dirty.relations.modified.end(),
                    upload_objects<relation_t>(context, context.osm->relations));
    }
    if(!dirty.relations.deleted.empty() || !dirty.ways.deleted.empty() || !dirty.nodes.deleted.empty()) {
      context.append_str(_("Deleting objects:\n"));
      xmlDocPtr doc = osmchange_init();
      xmlNodePtr del_node = xmlNewChild(xmlDocGetRootElement(doc), nullptr, BAD_CAST "delete", nullptr);
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

    context.append_str(_("Upload done.\n"));
  }
}
