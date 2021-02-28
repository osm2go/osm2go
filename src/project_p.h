/*
 * SPDX-FileCopyrightText: 2008 Till Harbaum <till@harbaum.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "project.h"

#include <memory>
#include <string>
#include <vector>

std::string project_filename(const project_t &project);
bool project_read(const std::string &project_file, project_t::ref project,
                  const std::string &defaultserver, int basefd);
void project_close(appdata_t &appdata);
std::vector<project_t *> project_scan(const std::string &base_path,
                                      int base_path_fd, const std::string &server);
std::string project_exists(int base_path, const char *name) __attribute__((nonnull(2)));
void project_delete(std::unique_ptr<project_t> &project);

class projects_to_bounds {
  std::vector<pos_area> &pbounds;
public:
  explicit inline projects_to_bounds(std::vector<pos_area> &b) : pbounds(b) {}
  inline ~projects_to_bounds() {}
  void operator()(const project_t *project);
};
