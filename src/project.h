/*
 * SPDX-FileCopyrightText: 2008 Till Harbaum <till@harbaum.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "fdguard.h"
#include "map_state.h"
#include "pos.h"

#include <libxml/parser.h>
#include <memory>
#include <string>

#include <osm2go_platform.h>
#include <osm2go_stl.h>

struct appdata_t;
class osm_t;

struct project_t {
  typedef const std::unique_ptr<project_t> &ref;

  /**
   * @constructor
   */
  project_t(const std::string &n, const std::string &base_path);

  /**
   * @brief sort-of move constructor
   */
  project_t(project_t &other);

  /**
   * @brief return the server URL
   * @param def the default server
   *
   * The default server URL is shared accross all projects, so they only
   * know that they use the default. In that case the default needs to
   * be passed in.
   */
  inline const std::string &server(const std::string &def) const noexcept
  { return rserver.empty() ? def : rserver; }

  /**
   * @brief set a new server value
   *
   * This will either copy nserver or clear the stored value if nserver == def
   */
  void adjustServer(const char *nserver, const std::string &def);

  struct {
    int x, y;
  } wms_offset;

  map_state_t map_state;

  pos_area bounds;

  std::string name;
  std::string path; ///< the project directory on disk (ends with '/')
  std::string desc;
  std::string osmFile;
  std::string rserver;

  std::string wms_server;

  bool data_dirty;     // needs to download new data
  bool isDemo;         // if this is the demo project
  fdguard dirfd;       // filedescriptor of path

  std::unique_ptr<osm_t> osm;          ///< the OSM data

  /**
   * @brief parse the OSM data file
   * @returns if the loading was successful
   */
  bool parse_osm();

  /**
   * @brief save the current project to disk
   * @param parent parent window for dialogs
   */
  bool save(osm2go_platform::Widget *parent = nullptr);

  /**
   * @brief rename the project
   * @param nname the new name (must be valid)
   * @param parent parent window for dialogs
   * @param global the currently active project
   *
   * If global matches the current project it will be updated with the
   * changed values as well. It must not be the same pointer as this
   * object.
   */
  bool rename(const std::string &nname, project_t::ref global, osm2go_platform::Widget *parent = nullptr);

  /**
   * @brief check if the current project is the demo project
   * @param parent parent window for dialogs
   */
  bool check_demo(osm2go_platform::Widget *parent = nullptr) const;

  /**
   * @brief check if OSM data is present for the given project
   * @return if OSM data file was found
   */
  bool osm_file_exists() const noexcept;

  /**
   * @brief remove the file with the changes
   *
   * Does not affect the loaded data.
   */
  void diff_remove_file() const;

  /**
   * @brief checks if a file with changes exists
   */
  bool diff_file_present() const;

  /**
   * @brief save the changed data to storage
   */
  void diff_save() const;

  /**
   * @brief restore changes from storage
   * @returns status values from enum diff_restore_results
   */
  unsigned int diff_restore();

  /**
   * @brief create an empty project and save it to disk
   * @param name name of the new project
   * @param base_path project directory base
   * @param parent parent widget for possible save errors
   * @returns the new project
   * @retval nullptr saving the project failed
   */
  static project_t *create(const std::string &name, const std::string &base_path, osm2go_platform::Widget *parent) __attribute__((warn_unused_result));

  struct projectStatus {
    inline projectStatus(bool isNew) noexcept
      : valid(!isNew)
      , errorColor(false)
    {
    }

    trstring message; ///< human readable status text
    trstring::native_type compressedMessage;  ///< caption string for map data
    bool valid; ///< if the data is valid and the dialog may be accepted
    bool errorColor;  ///< if the status message should get highlighting
  };

  projectStatus status(bool isNew) const;

  /**
   * @brief check if a diff is present or the project is active and has unsaved changes
   */
  bool activeOrDirty(const appdata_t &appdata) const;

  /**
   * @brief human readable message if changes are pending
   */
  trstring::native_type pendingChangesMessage(const appdata_t& appdata) const;
};

bool project_load(appdata_t &appdata, const std::string &name);
bool project_load(appdata_t &appdata, std::unique_ptr<project_t> &project);
