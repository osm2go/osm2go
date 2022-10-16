/*
 * SPDX-FileCopyrightText: 2017 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

struct appdata_t;
class style_t;

#include <map>
#include <string>

#include <string_view.hpp>

style_t *style_load_fname(const std::string &filename);
std::map<std::string, std::string> style_scan();
nonstd::string_view style_basename(nonstd::string_view name);
void style_change(appdata_t &appdata, const std::string &style_path);
