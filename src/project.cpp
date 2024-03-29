/*
 * SPDX-FileCopyrightText: 2008-2009 Till Harbaum <till@harbaum.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "project.h"
#include "project_p.h"

#include "appdata.h"
#include "area_edit.h"
#include "diff.h"
#include "map.h"
#include "misc.h"
#include "net_io.h"
#include "notifications.h"
#include "osm2go_platform.h"
#include "settings.h"
#include "track.h"
#include "uicontrol.h"
#include "wms.h"

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

#include <string_view.hpp>

#include "osm2go_annotations.h"
#include <osm2go_cpp.h>
#include <osm2go_i18n.h>
#include "osm2go_stl.h"

#if !defined(LIBXML_TREE_ENABLED) || !defined(LIBXML_OUTPUT_ENABLED)
#error "libxml doesn't support required tree or output"
#endif

/* ------------ project file io ------------- */

std::string project_filename(const project_t &project) {
  return project.path + project.name + ".proj";
}

/**
 * @brief parse the contents of the given project file
 *
 * Not marked static so it can be accessed by the testcases.
 */
bool project_read(const std::string &project_file, project_t::ref project,
                  const std::string &defaultserver, int basefd) {
  fdguard projectfd(basefd, project_file.c_str(), O_RDONLY);
  xmlDocGuard doc(xmlReadFd(projectfd, project_file.c_str(), nullptr, XML_PARSE_NONET));

  /* parse the file and get the DOM */
  if(unlikely(!doc)) {
    fprintf(stderr, "error: could not parse file %s\n", project_file.c_str());
    return false;
  }

  bool hasProj = false;
  for (xmlNode *cur_node = xmlDocGetRootElement(doc.get()); cur_node != nullptr;
       cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if(strcmp(reinterpret_cast<const char *>(cur_node->name), "proj") == 0) {
        hasProj = true;
        project->data_dirty = xml_get_prop_bool(cur_node, "dirty");
        project->isDemo = xml_get_prop_bool(cur_node, "demo");

        for(xmlNode *node = cur_node->children; node != nullptr; node = node->next) {
          if(node->type != XML_ELEMENT_NODE)
            continue;

          if(strcmp(reinterpret_cast<const char *>(node->name), "desc") == 0) {
            xmlString desc(xmlNodeListGetString(doc.get(), node->children, 1));
            project->desc = static_cast<const char *>(desc);
            printf("desc = %s\n", desc.get());
          } else if(strcmp(reinterpret_cast<const char *>(node->name), "server") == 0) {
            xmlString str(xmlNodeListGetString(doc.get(), node->children, 1));
            project->adjustServer(str, defaultserver);
            printf("server = %s\n", project->server(defaultserver).c_str());
          } else if(strcmp(reinterpret_cast<const char *>(node->name), "map") == 0) {
            xmlString str(xmlGetProp(node, BAD_CAST "zoom"));
            if(str)
              project->map_state.zoom = std::min(xml_parse_float(str), 50.0);

            str.reset(xmlGetProp(node, BAD_CAST "detail"));
            if(str)
              project->map_state.detail = xml_parse_float(str);

            str.reset(xmlGetProp(node, BAD_CAST "scroll-offset-x"));
            if(str)
              project->map_state.scroll_offset = osm2go_platform::screenpos(xml_parse_float(str), 0);

            str.reset(xmlGetProp(node, BAD_CAST "scroll-offset-y"));
            if(str)
              project->map_state.scroll_offset = osm2go_platform::screenpos(project->map_state.scroll_offset.x(), xml_parse_float(str));
          } else if(strcmp(reinterpret_cast<const char *>(node->name), "wms") == 0) {
            xmlString str(xmlGetProp(node, BAD_CAST "server"));
            if(!str.empty())
              project->wms_server = static_cast<const char *>(str);

            // upgrade old entries
            str.reset(xmlGetProp(node, BAD_CAST "path"));
            if(!str.empty())
              project->wms_server += static_cast<const char *>(str);

            str.reset(xmlGetProp(node, BAD_CAST "x-offset"));
            if(!str.empty())
              project->wms_offset.x = strtoul(str, nullptr, 10);

            str.reset(xmlGetProp(node, BAD_CAST "y-offset"));
            if(!str.empty())
              project->wms_offset.y = strtoul(str, nullptr, 10);
          } else if(strcmp(reinterpret_cast<const char *>(node->name), "osm") == 0) {
            xmlString str(xmlNodeListGetString(doc.get(), node->children, 1));
            if(likely(!str.empty())) {
              printf("osm = %s\n", str.get());

              /* make this a relative path if possible */
              /* if the project path actually is a prefix of this, */
              /* then just remove this prefix */
              if(str.get()[0] == '/' && strlen(str) > project->path.size() &&
                 !strncmp(str, project->path.c_str(), project->path.size())) {
                project->osmFile = reinterpret_cast<char *>(str.get() + project->path.size());
                printf("osm name converted to relative %s\n", project->osmFile.c_str());
              } else
                project->osmFile = static_cast<const char *>(str);
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

  if(!hasProj) {
    fprintf(stderr, "error: file %s does not contain <proj> element\n", project_file.c_str());
    return false;
  }

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
  char str[16];
  const std::string &project_file = project_filename(*this);

  printf("saving project to %s\n", project_file.c_str());

  /* check if project path exists */
  if(unlikely(!dirfd.valid())) {
    /* make sure project base path exists */
    if(unlikely(!osm2go_platform::create_directories(path))) {
      error_dlg(trstring("Unable to create project path %1").arg(path), parent);
      return false;
    }
    fdguard nfd(path.c_str());
    if(unlikely(!nfd.valid())) {
      error_dlg(trstring("Unable to open project path %1").arg(path), parent);
      return false;
    }
    dirfd.swap(nfd);
  }

  xmlDocGuard doc(xmlNewDoc(BAD_CAST "1.0"));
  xmlNodePtr root_node = xmlNewNode(nullptr, BAD_CAST "proj");
  xmlNewProp(root_node, BAD_CAST "name", BAD_CAST name.c_str());
  if(data_dirty)
    xmlNewProp(root_node, BAD_CAST "dirty", BAD_CAST "true");
  if(isDemo)
    xmlNewProp(root_node, BAD_CAST "demo", BAD_CAST "true");

  xmlDocSetRootElement(doc.get(), root_node);

  if(!rserver.empty())
    xmlNewChild(root_node, nullptr, BAD_CAST "server", BAD_CAST rserver.c_str());

  if(!desc.empty())
    xmlNewChild(root_node, nullptr, BAD_CAST "desc", BAD_CAST desc.c_str());

  const std::string defaultOsm = name + ".osm";
  if(unlikely(!osmFile.empty() && osmFile != defaultOsm + ".gz" && osmFile != defaultOsm))
    xmlNewChild(root_node, nullptr, BAD_CAST "osm", BAD_CAST osmFile.c_str());

  xmlNodePtr node = xmlNewChild(root_node, nullptr, BAD_CAST "min", nullptr);
  bounds.min.toXmlProperties(node);

  node = xmlNewChild(root_node, nullptr, BAD_CAST "max", nullptr);
  bounds.max.toXmlProperties(node);

  node = xmlNewChild(root_node, nullptr, BAD_CAST "map", nullptr);
  format_float(map_state.zoom, 4, str);
  xmlNewProp(node, BAD_CAST "zoom", BAD_CAST str);
  format_float(map_state.detail, 4, str);
  xmlNewProp(node, BAD_CAST "detail", BAD_CAST str);
  format_float(map_state.scroll_offset.x(), 4, str);
  xmlNewProp(node, BAD_CAST "scroll-offset-x", BAD_CAST str);
  format_float(map_state.scroll_offset.y(), 4, str);
  xmlNewProp(node, BAD_CAST "scroll-offset-y", BAD_CAST str);

  if(wms_offset.x != 0 || wms_offset.y != 0 || !wms_server.empty()) {
    node = xmlNewChild(root_node, nullptr, BAD_CAST "wms", nullptr);
    if(!wms_server.empty())
      xmlNewProp(node, BAD_CAST "server", BAD_CAST wms_server.c_str());
    snprintf(str, sizeof(str), "%d", wms_offset.x);
    xmlNewProp(node, BAD_CAST "x-offset", BAD_CAST str);
    snprintf(str, sizeof(str), "%d", wms_offset.y);
    xmlNewProp(node, BAD_CAST "y-offset", BAD_CAST str);
  }

  xmlSaveFormatFileEnc(project_file.c_str(), doc.get(), "UTF-8", 1);

  return true;
}

static void swap_project(project_t *one, project_t *other)
{
  one->path.swap(other->path);
  one->name.swap(other->name);
  one->osmFile.swap(other->osmFile);
  one->dirfd.swap(other->dirfd);
  one->rserver.swap(other->rserver);
}

bool project_t::rename(const std::string &nname, project_t::ref global, osm2go_platform::Widget *parent)
{
  const bool isGlobal = global && global->name == name;
  assert(global.get() != this);

  nonstd::string_view pathv = path;

  std::unique_ptr<project_t> tmpproj(std::make_unique<project_t>(nname, pathv.substr(0, pathv.size() - 1 /* slash */ - name.size())));
  tmpproj->map_state = map_state;
  const bool oldOsmExists = osm_file_exists();

  swap_project(tmpproj.get(), this);

  // if it is a local path create a local one again, otherwise just keep the old location
  if(tmpproj->osmFile.empty() || tmpproj->osmFile.find('/') != std::string::npos) {
    osmFile = tmpproj->osmFile;
  } else {
    osmFile = nname + ".osm";
    osm2go_platform::MappedFile osmData(tmpproj->path + tmpproj->osmFile);
    if(likely(osmData) && check_gzip(osmData.data(), osmData.length()))
      osmFile += ".gz";
  }

  // project file
  if(unlikely(!save(parent))) {
    swap_project(tmpproj.get(), this);
    return false;
  }

  // simply link the OSM data over, it is the same for both projects
  if(oldOsmExists && unlikely(linkat(tmpproj->dirfd, tmpproj->osmFile.c_str(), dirfd.fd, osmFile.c_str(), 0) != 0)) {
    error_dlg(trstring("Unable to link new OSM data file %1").arg(osmFile), parent);
    swap_project(tmpproj.get(), this);
    project_delete(tmpproj);
    return false;
  }

  // diff has project name in it
  if(tmpproj->diff_file_present() && !diff_rename(tmpproj, this)) {
    swap_project(tmpproj.get(), this);
    project_delete(tmpproj);
    return false;
  }

  // track
  if(linkat(tmpproj->dirfd, (tmpproj->name + ".trk").c_str(), dirfd.fd, (nname + ".trk").c_str(), 0) != 0 && errno != ENOENT) {
    error_dlg(_("Unable to link OSM track file"), parent);
    swap_project(tmpproj.get(), this);
    project_delete(tmpproj);
    return false;
  }

  // everything fine up until here, get rid of the old things

  // the project file first, that will prevent the file to be loaded again
  project_delete(tmpproj);

  if(isGlobal) {
    global->name = name;
    global->osmFile = osmFile;
    global->path = path;

    fdguard nfd(dup(dirfd));
    if(unlikely(!nfd.valid()))
      printf("Unable to dup project path fd, error %i", errno);

    global->dirfd.swap(nfd);
  }

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
  const std::string sname(name);
  std::string ret = sname + '/' + sname + ".proj";
  struct stat st;

  /* check for project file */
  if(fstatat(base_path, ret.c_str(), &st, 0) != 0 || !S_ISREG(st.st_mode))
    ret.clear();
  return ret;
}

std::vector<project_t *> project_scan(const std::string &base_path, int base_path_fd, const std::string &server)
{
  std::vector<project_t *> projects;

  /* scan for projects */
  dirguard dir(base_path_fd);
  if(!dir.valid())
    return projects;

  dirent *d;
  while((d = dir.next()) != nullptr) {
    if(d->d_type != DT_DIR && d->d_type != DT_UNKNOWN)
      continue;

    if(unlikely(strcmp(d->d_name, ".") == 0 || strcmp(d->d_name, "..") == 0))
      continue;

    std::string fullname = project_exists(base_path_fd, d->d_name);
    if(!fullname.empty()) {
      printf("found project %s\n", d->d_name);

      /* try to read project and append it to chain */
      std::unique_ptr<project_t> n(std::make_unique<project_t>(d->d_name, base_path));

      if(likely(project_read(fullname, n, server, base_path_fd)))
        projects.push_back(n.release());
    }
  }

  return projects;
}

/* ------------------------- create a new project ---------------------- */

void project_close(appdata_t &appdata) {
  printf("closing current project\n");

  /* Save track and turn off the handler callback */
  track_save(appdata.project, appdata.track.track.get());
  appdata.track_clear();

  appdata.map->clear(map_t::MAP_LAYER_ALL);
  std::unique_ptr<project_t> project;
  project.swap(appdata.project);

  project->diff_save();

  /* remember in settings that no project is open */
  settings_t::instance()->project.clear();

  /* update project file on disk */
  project->save();
}

void project_delete(std::unique_ptr<project_t> &project)
{
  printf("deleting project \"%s\"\n", project->name.c_str());

  /* remove entire directory from disk */
  dirguard dir(project->path);
  if(likely(dir.valid())) {
    int dfd = dir.dirfd();
    dirent *d;
    while ((d = dir.next()) != nullptr) {
      if(unlikely((d->d_type == DT_DIR ||
                    (unlinkat(dfd, d->d_name, 0) == -1 && errno == EISDIR)) &&
                    strcmp(d->d_name, ".") != 0 && strcmp(d->d_name, "..") != 0))
        unlinkat(dfd, d->d_name, AT_REMOVEDIR);
    }

    /* remove the projects directory */
    rmdir(project->path.c_str());
  }

  /* free project structure */
  project.reset();
}

void projects_to_bounds::operator()(const project_t* project)
{
  if (!project->bounds.valid())
    return;

  pbounds.push_back(project->bounds);
}

bool project_t::check_demo(osm2go_platform::Widget *parent) const {
  if(isDemo)
    message_dlg(_("Demo project"),
                   _("This is a preinstalled demo project. This means that the "
                     "basic project parameters cannot be changed and no data can "
                     "be up- or downloaded via the OSM servers.\n\n"
                     "Please setup a new project to do these things."), parent);

  return isDemo;
}

bool project_t::osm_file_exists() const noexcept
{
  struct stat st;
  return fstatat(dirfd, osmFile.c_str(), &st, 0) == 0 && S_ISREG(st.st_mode);
}

static project_t *project_open(const std::string &name)
{
  std::unique_ptr<project_t> project;
  std::string project_file;

  assert(!name.empty());
  settings_t::ref settings = settings_t::instance();
  std::string::size_type sl = name.rfind('/');
  if(unlikely(sl != std::string::npos)) {
    // load with absolute or relative path, usually only done for demo
    project_file = name;
    nonstd::string_view pname = name;

    pname = pname.substr(sl + 1);
    if(likely(pname.ends_with(".proj")))
      pname.remove_suffix(5); // strlen(".proj")

    // usually that ends in /foo/foo.proj
    if (sl > pname.size() + 1 && name[sl - pname.size() - 1] == '/') {
      // must be an extra view as old C++ versions can't compare std::string to a string_view
      nonstd::string_view namev = name;

      if(namev.compare(sl - pname.size(), pname.size(), pname) == 0)
        sl -= pname.size();
    }
    project.reset(new project_t(nonstd::to_string(pname), nonstd::to_string_view(name).substr(0, sl)));
  } else {
    project.reset(new project_t(name, settings->base_path));

    project_file = project_filename(*project);
  }

  if(unlikely(!project_read(project_file, project, settings->server, settings->base_path_fd)))
    return nullptr;

  return project.release();
}

static bool project_load_inner(appdata_t &appdata, std::unique_ptr<project_t> &project)
{
  std::swap(appdata.project, project);

  /* --------- project structure ok: load its OSM file --------- */

  if(!appdata.project->parse_osm()) {
    appdata.uicontrol->showNotification(trstring("Error opening %1").arg(appdata.project->osmFile), MainUi::Brief);
    return false;
  }

  if(unlikely(!appdata.project->bounds.valid())) {
    appdata.uicontrol->showNotification(trstring("Invalid project bounds in %1").arg(appdata.project->name), MainUi::Brief);

    return false;
  }

  if(unlikely(appdata_t::window == nullptr))
    return false;

  /* check if OSM data is valid */
  osm2go_platform::process_events();
  trstring::native_type errmsg = appdata.project->osm->sanity_check();
  if(unlikely(!errmsg.isEmpty())) {
    error_dlg(errmsg);
    printf("project/osm sanity checks failed (%s), unloading project\n", errmsg.toStdString().c_str());

    return false;
  }

  /* load diff possibly preset */
  osm2go_platform::process_events();
  if(unlikely(appdata_t::window == nullptr))
    return false;

  diff_restore(appdata.project, appdata.uicontrol.get());

  /* prepare colors etc, draw data and adjust scroll/zoom settings */
  osm2go_platform::process_events();
  if(unlikely(appdata_t::window == nullptr))
    return false;

  appdata.map->init();

  /* restore a track */
  osm2go_platform::process_events();
  if(unlikely(appdata_t::window == nullptr))
    return false;

  appdata.track_clear();
  if(track_restore(appdata))
    appdata.map->track_draw(settings_t::instance()->trackVisibility, *appdata.track.track);

  /* finally load a background if present */
  osm2go_platform::process_events();
  if(unlikely(appdata_t::window == nullptr))
    return false;

  std::string wmsfn = wms_find_file(appdata.project->path);
  if (!wmsfn.empty())
    appdata.map->set_bg_image(wmsfn,
                              osm2go_platform::screenpos(project->wms_offset.x, project->wms_offset.y));

  /* save the name of the project for the perferences */
  settings_t::instance()->project = appdata.project->name;

  appdata.uicontrol->clearNotification(MainUi::ClearBoth);

  return true;
}

static void project_replace_start(appdata_t &appdata, const std::string &name)
{
  appdata.uicontrol->showNotification(trstring("Loading %1").arg(name), MainUi::Busy);

  /* close current project */
  osm2go_platform::process_events();

  if(appdata.project)
    project_close(appdata);

  osm2go_platform::process_events();
}

bool project_load(appdata_t &appdata, const std::string &name)
{
  project_replace_start(appdata, name);

  /* open project itself */
  std::unique_ptr<project_t> project(project_open(name));

  if(unlikely(!project)) {
    appdata.uicontrol->showNotification(trstring("Error opening %1").arg(name), MainUi::Brief);

    return false;
  }

  bool ret = project_load_inner(appdata, project);
  if(unlikely(!ret))
    appdata.project.reset();

  return ret;
}

bool project_load(appdata_t &appdata, std::unique_ptr<project_t> &project)
{
  project_replace_start(appdata, project->name);

  bool ret = project_load_inner(appdata, project);
  if(unlikely(!ret))
    appdata.project.reset();

  return ret;
}

bool project_t::parse_osm() {
  osm.reset(osm_t::parse(path, osmFile));
  return static_cast<bool>(osm);
}

namespace {

std::string constructPath(nonstd::string_view base_path, const std::string &name)
{
  std::string path(base_path.size() + name.size() + 1 /* '/' + '\0' */, '/');

  /* in never versions of C++ data() returns non-const anyway */
  base_path.copy(const_cast<char *>(&path[0]), base_path.size());
  path.replace(base_path.size(), base_path.size() + name.size(), name);
  path += '/';

  return path;
}

} // namespace

project_t::project_t(const std::string &n, nonstd::string_view base_path)
  : bounds(pos_t(0, 0), pos_t(0, 0))
  , name(n)
  , path(constructPath(base_path, name))
  , data_dirty(false)
  , isDemo(false)
  , dirfd(path.c_str())
{
  memset(&wms_offset, 0, sizeof(wms_offset));
}

project_t::project_t(project_t &other)
  : wms_offset(other.wms_offset)
  , map_state(other.map_state)
  , bounds(other.bounds)
  , data_dirty(other.data_dirty)
  , isDemo(other.isDemo)
  , dirfd(-1)
{
  swap_project(this, &other);
}

void project_t::adjustServer(const char *nserver, const std::string &def)
{
  if(nserver == nullptr || *nserver == '\0' || def == nserver)
    rserver.clear();
  else
    rserver = nserver;
}

project_t *project_t::create(const std::string &name, const std::string &base_path, osm2go_platform::Widget *parent)
{
  std::unique_ptr<project_t> project(std::make_unique<project_t>(name, base_path));

  /* no data downloaded yet */
  project->data_dirty = true;

  /* build project osm file name */
  project->osmFile = project->name + ".osm.gz";

  project->bounds.min = pos_t(NAN, NAN);
  project->bounds.max = pos_t(NAN, NAN);

  /* create project file on disk */
  if(!project->save(parent)) {
    project_delete(project);
    return nullptr;
  }

  return project.release();
}

project_t::projectStatus project_t::status(bool isNew) const
{
  projectStatus ret(isNew);

  ret.compressedMessage = _("Map data:");

  struct stat st;
  errno = 0; // make sure that the next check works if S_ISREG() returns false
  bool stret = fstatat(dirfd, osmFile.c_str(), &st, 0) == 0 &&
               S_ISREG(st.st_mode);
  if(!stret && errno == ENOENT) {
    ret.message = trstring("Not downloaded!");
    ret.errorColor = true;
  } else {
    if(!data_dirty) {
      if(stret) {
        struct tm loctime;
        localtime_r(&st.st_mtim.tv_sec, &loctime);
        char time_str[32];
        strftime(time_str, sizeof(time_str), "%x %X", &loctime);
        ret.message = trstring("%1 bytes present\nfrom %2").arg(st.st_size).arg(time_str);

        if(ends_with(osmFile, ".gz"))
          ret.compressedMessage = _("Map data:\n(compressed)");
        ret.valid = true;
      } else {
        ret.message = trstring("Error testing data file");
      }
    } else
      ret.message = trstring("Outdated, please download!");
  }

  return ret;
}

bool project_t::activeOrDirty(const appdata_t& appdata) const
{
  if (diff_file_present())
    return true;

  if(appdata.project && appdata.project->osm && appdata.project->name == name)
    return !appdata.project->osm->is_clean(true);

  return false;
}

trstring::native_type project_t::pendingChangesMessage(const appdata_t& appdata) const
{
  if(activeOrDirty(appdata))
    /* this should prevent the user from changing the area */
    return _("unsaved changes pending");
  else
    return _("no pending changes");
}
