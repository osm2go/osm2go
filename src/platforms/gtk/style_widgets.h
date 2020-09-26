/*
 * SPDX-FileCopyrightText: 2008 Till Harbaum <till@harbaum.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gtk/gtk.h>
#include <string>

struct appdata_t;

#ifndef FREMANTLE
void style_select(appdata_t *appdata);
#else
GtkWidget *style_select_widget(const std::string &currentstyle);
void style_change(appdata_t &appdata, GtkWidget *widget);
#endif
