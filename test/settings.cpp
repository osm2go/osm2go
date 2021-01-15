/*
 * SPDX-FileCopyrightText: 2021 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <settings.h>

#include <wms.h>

#include <algorithm>
#include <cassert>

#include <osm2go_annotations.h>
#include <osm2go_test.h>

namespace {

class emptySettings : public settings_t {
  emptySettings() {}
public:
  static std::shared_ptr<settings_t> get()
  {
    std::shared_ptr<settings_t> ret(new emptySettings());

    return ret;
  }

  static void clearServers(std::vector<wms_server_t *> &srvs)
  {
    std::for_each(srvs.begin(), srvs.end(), std::default_delete<wms_server_t>());
    srvs.clear();
  }

  void ld()
  {
    load();
  }

  void defs()
  {
    setDefaults();
  }
};

void testRef()
{
  settings_t::ref s = settings_t::instance();
  assert(s.get() == settings_t::instance().get());
}

void testDefaults()
{
  settings_t::ref settings = emptySettings::get();

  setenv("OSM_USER", "ouser", 1);
  setenv("OSM_PASS", "secret123", 1);

  static_cast<emptySettings *>(settings.get())->defs();

  assert_cmpstr(settings->username, "ouser");
  assert_cmpstr(settings->password, "secret123");
  assert_cmpstr(settings->style, DEFAULT_STYLE);

  std::vector<wms_server_t *> defservers = wms_server_get_default();
  assert_cmpnum(defservers.size(), settings->wms_server.size());

  for (size_t i = 0; i < defservers.size(); i++) {
    assert_cmpstr(defservers.at(i)->name, settings->wms_server.at(i)->name);
    assert_cmpstr(defservers.at(i)->server, settings->wms_server.at(i)->server);
  }

  emptySettings::clearServers(defservers);
  emptySettings::clearServers(settings->wms_server);

  // now load whatever is on disk so that the save() call in the base destructor
  // will hopefully not change anything
  static_cast<emptySettings *>(settings.get())->ld();
}

} // namespace

int main(int argc, char **argv)
{
  OSM2GO_TEST_INIT(argc, argv);

  testRef();
  testDefaults();

  return 0;
}

#include "dummy_appdata.h"
