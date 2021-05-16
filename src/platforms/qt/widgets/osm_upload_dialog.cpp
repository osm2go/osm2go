/*
 * SPDX-FileCopyrightText: 2020 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <osm_api.h>
#include <osm_api_p.h>

#include <settings.h>

#include <osm2go_annotations.h>
#include "osm2go_i18n.h"

#define COLOR_ERR  "red"
#define COLOR_OK   "darkgreen"

void
osm_upload_dialog(appdata_t &, const osm_t::dirty_t &)
{
  abort();
}

osm_upload_context_t::osm_upload_context_t(appdata_t &a, project_t::ref p, const char *c, const char *s)
  : appdata(a)
  , osm(p->osm)
  , project(p)
  , urlbasestr(p->server(settings_t::instance()->server) + "/")
  , comment(c)
  , src(s == nullptr ? s : std::string())
{
}

void
osm_upload_context_t::append_str(const char *msg, const char *colorname)
{
  append(trstring(msg), colorname);
}

void
osm_upload_context_t::append(const trstring &, const char *)
{
  abort();
}
