/*
 * SPDX-FileCopyrightText: 2008 Till Harbaum <till@harbaum.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "project.h"

#include <osm2go_platform.h>

#include <string>
#include <vector>

struct wms_server_t {
  explicit inline wms_server_t() {}
  explicit inline wms_server_t(const char *n, const char *s) __attribute__ ((nonnull(2,3)))
    : name(n), server(s) {}
  std::string name, server;
};

std::string wms_import(osm2go_platform::Widget *parent, project_t::ref project);
std::string wms_find_file(const std::string &project_path);
void wms_remove_file(project_t &project);

std::vector<wms_server_t *> wms_server_get_default(void);
