/*
 * SPDX-FileCopyrightText: 2008 Till Harbaum <till@harbaum.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "osm.h"

#include <osm2go_platform.h>

struct appdata_t;
struct project_t;
class settings_t;

bool osm_download(osm2go_platform::Widget *parent, project_t *project);
void osm_upload(appdata_t &appdata);

void osm_modified_info(const osm_t::dirty_t &context, osm2go_platform::Widget *parent);
