/*
 * SPDX-FileCopyrightText: 2008 Till Harbaum <till@harbaum.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "pos.h"

#include <glib.h>
#include <libosso.h>

typedef struct {
  struct pos_t pos;
  int zoom;
  gboolean valid;
} dbus_mm_pos_t;

#ifdef __cplusplus
extern "C" {
#endif

gboolean dbus_register(osso_context_t *ctx);
gboolean dbus_mm_set_position(dbus_mm_pos_t *mmp);

#ifdef __cplusplus
}
#endif
