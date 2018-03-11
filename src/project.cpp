/*
 * Copyright (C) 2008-2009 Till Harbaum <till@harbaum.org>.
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

#include "project.h"
#include "project_p.h"

#include "appdata.h"
#include "area_edit.h"
#include "diff.h"
#include "map.h"
#include "misc.h"
#include "osm2go_platform.h"
#include "settings.h"
#include "track.h"
#include "wms.h"
#include "uicontrol.h"
#include "xml_helpers.h"

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <memory>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

#include "osm2go_annotations.h"
#include <osm2go_cpp.h>
#include "osm2go_stl.h"
#include <osm2go_i18n.h>

#if !defined(LIBXML_TREE_ENABLED) || !defined(LIBXML_OUTPUT_ENABLED)
#error "libxml doesn't support required tree or output"
#endif

/* ------------ project file io ------------- */

std::string project_filename(const project_t *project) {
  return project->path + project->name + ".proj";
}

bool project_read(const std::string &project_file, project_t *project,
                  const std::string &defaultserver, int basefd) {
  fdguard projectfd(basefd, project_file.c_str(), O_RDONLY);
  std::unique_ptr<xmlDoc, xmlDocDelete> doc(xmlReadFd(projectfd, project_file.c_str(), O2G_NULLPTR, XML_PARSE_NONET));

  /* parse the file and get the DOM */
  if(unlikely(!doc)) {
    printf("error: could not parse file %s\n", project_file.c_str());
    return false;
  }

  bool hasProj = false;
  for (xmlNode *cur_node = xmlDocGetRootElement(doc.get()); cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if(strcmp(reinterpret_cast<const char *>(cur_node->name), "proj") == 0) {
        hasProj = true;
        project->data_dirty = xml_get_prop_bool(cur_node, "dirty");
        project->isDemo = xml_get_prop_bool(cur_node, "demo");

        for(xmlNode *node = cur_node->children; node != O2G_NULLPTR; node = node->next) {
          if(node->type != XML_ELEMENT_NODE)
            continue;

          if(strcmp(reinterpret_cast<const char *>(node->name), "desc") == 0) {
            xmlString desc(xmlNodeListGetString(doc.get(), node->children, 1));
            project->desc = reinterpret_cast<char *>(desc.get());
            printf("desc = %s\n", desc.get());
          } else if(strcmp(reinterpret_cast<const char *>(node->name), "server") == 0) {
            xmlString str(xmlNodeListGetString(doc.get(), node->children, 1));
            project->adjustServer(reinterpret_cast<char *>(str.get()), defaultserver);
            printf("server = %s\n", project->server(defaultserver).c_str());
          } else if(strcmp(reinterpret_cast<const char *>(node->name), "map") == 0) {
            xmlString str(xmlGetProp(node, BAD_CAST "zoom"));
            if(str)
              project->map_state.zoom = xml_parse_float(str);

            str.reset(xmlGetProp(node, BAD_CAST "detail"));
            if(str)
              project->map_state.detail = xml_parse_float(str);

            str.reset(xmlGetProp(node, BAD_CAST "scroll-offset-x"));
            if(str)
              project->map_state.scroll_offset.x = strtoul(reinterpret_cast<char *>(str.get()), O2G_NULLPTR, 10);

            str.reset(xmlGetProp(node, BAD_CAST "scroll-offset-y"));
            if(str)
              project->map_state.scroll_offset.y = strtoul(reinterpret_cast<char *>(str.get()), O2G_NULLPTR, 10);
          } else if(strcmp(reinterpret_cast<const char *>(node->name), "wms") == 0) {
            xmlString str(xmlGetProp(node, BAD_CAST "server"));
            if(str)
              project->wms_server = reinterpret_cast<char *>(str.get());

            str.reset(xmlGetProp(node, BAD_CAST "path"));
            if(str)
              project->wms_path = reinterpret_cast<char *>(str.get());

            str.reset(xmlGetProp(node, BAD_CAST "x-offset"));
            if(str)
              project->wms_offset.x = strtoul(reinterpret_cast<char *>(str.get()), O2G_NULLPTR, 10);

            str.reset(xmlGetProp(node, BAD_CAST "y-offset"));
            if(str)
              project->wms_offset.y = strtoul(reinterpret_cast<char *>(str.get()), O2G_NULLPTR, 10);
          } else if(strcmp(reinterpret_cast<const char *>(node->name), "osm") == 0) {
            xmlString str(xmlNodeListGetString(doc.get(), node->children, 1));
            if(likely(str)) {
              printf("osm = %s\n", str.get());

              /* make this a relative path if possible */
              /* if the project path actually is a prefix of this, */
              /* then just remove this prefix */
              if(str.get()[0] == '/' &&
                strlen(reinterpret_cast<char *>(str.get())) > project->path.size() &&
                !strncmp(reinterpret_cast<char *>(str.get()), project->path.c_str(), project->path.size())) {

                project->osmFile = reinterpret_cast<char *>(str.get() + project->path.size());
                printf("osm name converted to relative %s\n", project->osmFile.c_str());
              } else
                project->osmFile = reinterpret_cast<char *>(str.get());
            }
          } else if(strcmp(reinterpret_cast<const char *>(node->name), "min") == 0) {
            project->bounds.min = pos_t::fromXmlProperties(node);
          } else if(strcmp(reinterpret_cast<const char *>(node->name), "max") == 0) {
            project->bounds.max = pos_t::fromXmlProperties(node);
          }
        }
      }
    }
  }

  if(!hasProj)
    return false;

  // no explicit filename was given, guess the default ones
  if(project->osmFile.empty()) {
    std::string fname = project->name + ".osm.gz";
    struct stat st;
    if(fstatat(project->dirfd, fname.c_str(), &st, 0) != 0 || !S_ISREG(st.st_mode))
      fname.erase(fname.size() - 3);
    project->osmFile = fname;
  }

  return true;
}

bool project_t::save(osm2go_platform::Widget *parent) {
  char str[32];
  const std::string &project_file = project_filename(this);

  printf("saving project to %s\n", project_file.c_str());

  /* check if project path exists */
  if(unlikely(!dirfd.valid())) {
    /* make sure project base path exists */
    if(unlikely(g_mkdir_with_parents(path.c_str(), S_IRWXU) != 0)) {
      errorf(parent, _("Unable to create project path %s"), path.c_str());
      return false;
    }
    fdguard nfd(path.c_str());
    if(unlikely(!nfd.valid())) {
      errorf(parent, _("Unable to open project path %s"), path.c_str());
      return false;
    }
    dirfd.swap(nfd);
  }

  std::unique_ptr<xmlDoc, xmlDocDelete> doc(xmlNewDoc(BAD_CAST "1.0"));
  xmlNodePtr node, root_node = xmlNewNode(O2G_NULLPTR, BAD_CAST "proj");
  xmlNewProp(root_node, BAD_CAST "name", BAD_CAST name.c_str());
  if(data_dirty)
    xmlNewProp(root_node, BAD_CAST "dirty", BAD_CAST "true");
  if(isDemo)
    xmlNewProp(root_node, BAD_CAST "demo", BAD_CAST "true");

  xmlDocSetRootElement(doc.get(), root_node);

  if(!rserver.empty())
    xmlNewChild(root_node, O2G_NULLPTR, BAD_CAST "server", BAD_CAST rserver.c_str());

  if(!desc.empty())
    xmlNewChild(root_node, O2G_NULLPTR, BAD_CAST "desc", BAD_CAST desc.c_str());

  const std::string defaultOsm = name + ".osm";
  if(unlikely(!osmFile.empty() && osmFile != defaultOsm + ".gz" && osmFile != defaultOsm))
    xmlNewChild(root_node, O2G_NULLPTR, BAD_CAST "osm", BAD_CAST osmFile.c_str());

  node = xmlNewChild(root_node, O2G_NULLPTR, BAD_CAST "min", O2G_NULLPTR);
  bounds.min.toXmlProperties(node);

  node = xmlNewChild(root_node, O2G_NULLPTR, BAD_CAST "max", O2G_NULLPTR);
  bounds.max.toXmlProperties(node);

  node = xmlNewChild(root_node, O2G_NULLPTR, BAD_CAST "map", O2G_NULLPTR);
  g_ascii_formatd(str, sizeof(str), "%.04f", map_state.zoom);
  xmlNewProp(node, BAD_CAST "zoom", BAD_CAST str);
  g_ascii_formatd(str, sizeof(str), "%.04f", map_state.detail);
  xmlNewProp(node, BAD_CAST "detail", BAD_CAST str);
  snprintf(str, sizeof(str), "%d", map_state.scroll_offset.x);
  xmlNewProp(node, BAD_CAST "scroll-offset-x", BAD_CAST str);
  snprintf(str, sizeof(str), "%d", map_state.scroll_offset.y);
  xmlNewProp(node, BAD_CAST "scroll-offset-y", BAD_CAST str);

  if(wms_offset.x != 0 || wms_offset.y != 0 ||
     !wms_server.empty() || !wms_path.empty()) {
    node = xmlNewChild(root_node, O2G_NULLPTR, BAD_CAST "wms", O2G_NULLPTR);
    if(!wms_server.empty())
      xmlNewProp(node, BAD_CAST "server", BAD_CAST wms_server.c_str());
    if(!wms_path.empty())
      xmlNewProp(node, BAD_CAST "path", BAD_CAST wms_path.c_str());
    snprintf(str, sizeof(str), "%d", wms_offset.x);
    xmlNewProp(node, BAD_CAST "x-offset", BAD_CAST str);
    snprintf(str, sizeof(str), "%d", wms_offset.y);
    xmlNewProp(node, BAD_CAST "y-offset", BAD_CAST str);
  }

  xmlSaveFormatFileEnc(project_file.c_str(), doc.get(), "UTF-8", 1);

  return true;
}

/* ------------ project selection dialog ------------- */

/**
 * @brief check if a project with the given name exists
 * @param base_path root path for projects
 * @param name project name
 * @returns path of project file relative to base_path or empty
 */
std::string project_exists(int base_path, const char *name) {
  std::string ret = std::string(name) + '/' + name + ".proj";
  struct stat st;

  /* check for project file */
  if(fstatat(base_path, ret.c_str(), &st, 0) != 0 || !S_ISREG(st.st_mode))
    ret.clear();
  return ret;
}

std::vector<project_t *> project_scan(map_state_t &ms, const std::string &base_path, int base_path_fd, const std::string &server) {
  std::vector<project_t *> projects;

  /* scan for projects */
  dirguard dir(base_path_fd);
  if(!dir.valid())
    return projects;

  dirent *d;
  while((d = dir.next()) != O2G_NULLPTR) {
    if(d->d_type != DT_DIR && d->d_type != DT_UNKNOWN)
      continue;

    if(unlikely(strcmp(d->d_name, ".") == 0 || strcmp(d->d_name, "..") == 0))
      continue;

    std::string fullname = project_exists(base_path_fd, d->d_name);
    if(!fullname.empty()) {
      printf("found project %s\n", d->d_name);

      /* try to read project and append it to chain */
      std::unique_ptr<project_t> n(new project_t(ms, d->d_name, base_path));

      if(likely(project_read(fullname, n.get(), server, base_path_fd)))
        projects.push_back(n.release());
    }
  }

  return projects;
}

/* ------------------------- create a new project ---------------------- */

void project_close(appdata_t &appdata) {
  printf("closing current project\n");

  /* Save track and turn off the handler callback */
  track_save(appdata.project, appdata.track.track);
  appdata.track_clear();

  appdata.map->clear(map_t::MAP_LAYER_ALL);

  if(appdata.osm) {
    diff_save(appdata.project, appdata.osm);
    delete appdata.osm;
    appdata.osm = O2G_NULLPTR;
  }

  /* remember in settings that no project is open */
  appdata.settings->project.clear();

  /* update project file on disk */
  appdata.project->save();

  delete appdata.project;
  appdata.project = O2G_NULLPTR;
}

void project_delete(project_t *project) {
  printf("deleting project \"%s\"\n", project->name.c_str());

  /* remove entire directory from disk */
  dirguard dir(project->path.c_str());
  if(likely(dir.valid())) {
    int dfd = dir.dirfd();
    dirent *d;
    while ((d = dir.next()) != O2G_NULLPTR) {
      if(unlikely(d->d_type == DT_DIR ||
                    (unlinkat(dfd, d->d_name, 0) == -1 && errno == EISDIR)))
        unlinkat(dfd, d->d_name, AT_REMOVEDIR);
    }

    /* remove the projects directory */
    rmdir(project->path.c_str());
  }

  /* free project structure */
  delete project;
}

void projects_to_bounds::operator()(const project_t* project)
{
  if (!project->bounds.valid())
    return;

  pbounds.push_back(project->bounds);
}

bool project_t::check_demo(osm2go_platform::Widget *parent) const {
  if(isDemo)
    messagef(parent, _("Demo project"),
             _("This is a preinstalled demo project. This means that the "
               "basic project parameters cannot be changed and no data can "
               "be up- or downloaded via the OSM servers.\n\n"
               "Please setup a new project to do these things."));

  return isDemo;
}

static bool project_open(appdata_t &appdata, const std::string &name) {
  std::unique_ptr<project_t> project;
  std::string project_file;

  assert(!name.empty());
  std::string::size_type sl = name.rfind('/');
  if(unlikely(sl != std::string::npos)) {
    // load with absolute or relative path, usually only done for demo
    project_file = name;
    std::string pname = name.substr(sl + 1);
    if(likely(pname.substr(pname.size() - 5) == ".proj"))
      pname.erase(pname.size() - 5);
    // usually that ends in /foo/foo.proj
    if(name.substr(sl - pname.size() - 1, pname.size() + 1) == '/' + pname)
      sl -= pname.size();
    project.reset(new project_t(appdata.map_state, pname, name.substr(0, sl)));
  } else {
    project.reset(new project_t(appdata.map_state, name, appdata.settings->base_path));

    project_file = project_filename(project.get());
  }
  project->map_state.reset();

  printf("project file = %s\n", project_file.c_str());
  if(unlikely(!project_read(project_file, project.get(), appdata.settings->server,
                            appdata.settings->base_path_fd))) {
    printf("error reading project file\n");
    return false;
  }

  /* --------- project structure ok: load its OSM file --------- */

  printf("project_open: loading osm %s\n", project->osmFile.c_str());
  appdata.osm = project->parse_osm();
  appdata.project = project.release();

  return appdata.osm != O2G_NULLPTR;
}

static bool project_load_inner(appdata_t &appdata, const std::string &name) {
  char banner_txt[64];
  snprintf(banner_txt, sizeof(banner_txt), _("Loading %s"), name.c_str());
  appdata.uicontrol->showNotification(banner_txt, MainUi::Busy);

  /* close current project */
  osm2go_platform::process_events();

  if(appdata.project)
    project_close(appdata);

  /* open project itself */
  osm2go_platform::process_events();

  if(unlikely(!project_open(appdata, name))) {
    printf("error opening requested project\n");

    snprintf(banner_txt, sizeof(banner_txt),
	     _("Error opening %s"), name.c_str());
    appdata.uicontrol->showNotification(banner_txt, MainUi::Brief);

    return false;
  }

  if(unlikely(appdata_t::window == O2G_NULLPTR))
    return false;

  /* check if OSM data is valid */
  osm2go_platform::process_events();
  const char *errmsg = appdata.osm->sanity_check();
  if(unlikely(errmsg != O2G_NULLPTR)) {
    errorf(O2G_NULLPTR, "%s", errmsg);
    printf("project/osm sanity checks failed, unloading project\n");

    snprintf(banner_txt, sizeof(banner_txt),
	     _("Error opening %s"), name.c_str());
    appdata.uicontrol->showNotification(banner_txt, MainUi::Brief);

    return false;
  }

  /* load diff possibly preset */
  osm2go_platform::process_events();
  if(unlikely(appdata_t::window == O2G_NULLPTR))
    return false;

  diff_restore(appdata);

  /* prepare colors etc, draw data and adjust scroll/zoom settings */
  osm2go_platform::process_events();
  if(unlikely(appdata_t::window == O2G_NULLPTR))
    return false;

  appdata.map->init();

  /* restore a track */
  osm2go_platform::process_events();
  if(unlikely(appdata_t::window == O2G_NULLPTR))
    return false;

  appdata.track_clear();
  if(track_restore(appdata))
    appdata.map->track_draw(appdata.settings->trackVisibility, *appdata.track.track);

  /* finally load a background if present */
  osm2go_platform::process_events();
  if(unlikely(appdata_t::window == O2G_NULLPTR))
    return false;
  wms_load(appdata);

  /* save the name of the project for the perferences */
  appdata.settings->project = appdata.project->name;

  appdata.uicontrol->showNotification(O2G_NULLPTR, MainUi::Busy);
  appdata.uicontrol->showNotification(O2G_NULLPTR);

  return true;
}

bool project_load(appdata_t &appdata, const std::string &name) {
  bool ret = project_load_inner(appdata, name);
  if(unlikely(!ret)) {
    printf("project loading interrupted by user\n");

    delete appdata.project;
    appdata.project = O2G_NULLPTR;
    delete appdata.osm;
    appdata.osm = O2G_NULLPTR;
  }
  return ret;
}

osm_t *project_t::parse_osm() const {
  return osm_t::parse(path, osmFile);
}

project_t::project_t(map_state_t &ms, const std::string &n, const std::string &base_path)
  : map_state(ms)
  , name(n)
  , path(base_path +  name + '/')
  , data_dirty(false)
  , isDemo(false)
  , dirfd(path.c_str())
{
  memset(&wms_offset, 0, sizeof(wms_offset));
  memset(&bounds, 0, sizeof(bounds));
}

project_t::~project_t()
{
}

void project_t::adjustServer(const char *nserver, const std::string &def)
{
  if(nserver == O2G_NULLPTR || !*nserver || def == nserver)
    rserver.clear();
  else
    rserver = nserver;
}
