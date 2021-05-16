/*
 * SPDX-FileCopyrightText: 2020 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <wms_p.h>

std::string
wms_server_dialog(osm2go_platform::Widget *, const std::string &)
{
  abort();
}

std::string
wms_layer_dialog(osm2go_platform::Widget *, const pos_area &, const wms_layer_t::list &)
{
  abort();
}
