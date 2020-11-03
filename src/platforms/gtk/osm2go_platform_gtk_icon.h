/*
 * SPDX-FileCopyrightText: 2020 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <icon.h>

typedef struct _GtkWidget GtkWidget;

class gtk_platform_icon_t : public icon_t {
protected:
  inline gtk_platform_icon_t() {}
public:
  static gtk_platform_icon_t &instance();

  GtkWidget *widget_load(const std::string &name, int limit = -1);
};
