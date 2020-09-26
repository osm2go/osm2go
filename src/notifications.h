/*
 * SPDX-FileCopyrightText: 2008 Till Harbaum <till@harbaum.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <osm2go_i18n.h>
#include <osm2go_platform.h>

void error_dlg(trstring::arg_type msg, osm2go_platform::Widget *parent = nullptr);
void warning_dlg(trstring::arg_type msg, osm2go_platform::Widget *parent = nullptr);
void message_dlg(trstring::arg_type title, trstring::arg_type msg, osm2go_platform::Widget *parent = nullptr);
