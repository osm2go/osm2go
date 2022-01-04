#include <josm_elemstyles.h>
#include <josm_elemstyles_p.h>

#include <appdata.h>
#include <gps_state.h>
#include <icon.h>
#include <iconbar.h>
#include <josm_presets.h>
#include <map.h>
#include <project.h>
#include <style.h>
#include <uicontrol.h>

#include <osm2go_annotations.h>
#include <osm2go_cpp.h>
#include <osm2go_test.h>

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <memory>

appdata_t::appdata_t()
  : uicontrol(nullptr)
  , map(nullptr)
  , icons(icon_t::instance())
{
}

namespace {

struct colorizer {
  const style_t * const style;
  explicit inline colorizer(const style_t *s) : style(s) {}
  inline void operator()(const std::pair<const item_id_t, node_t *> &pair) const
  {
    style->colorize(pair.second);
  }
  inline void operator()(const std::pair<const item_id_t, way_t *> &pair) const
  {
    style->colorize(pair.second);
  }
};

} // namespace

int main(int argc, char **argv)
{
  OSM2GO_TEST_INIT(argc, argv);

  if (argc != 1) {
    std::cerr << "Usage: " << argv[0] << std::endl;
    return 1;
  }

  xmlInitParser();

  appdata_t appdata;

  std::unique_ptr<josm_elemstyle> style(static_cast<josm_elemstyle *>(style_t::load("mapnik")));

  if(!style) {
    std::cerr << "failed to load styles" << std::endl;
    return 1;
  }

  assert(!style->elemstyles.empty());

  std::unique_ptr<osm_t> osm(std::make_unique<osm_t>());

  osm->bounds.min = lpos_t(0, 0);
  osm->bounds.max = lpos_t(0, 0);

  osm_t::TagMap tags;
  way_t * const way = osm->attach(new way_t());

  for (int i = 0; i < 4; i++) {
    node_t * const node = osm->node_new(pos_t(i, i));
    osm->attach(node);
    way->append_node(node);
  }

  // test priority, first without collisions
  tags.clear();
  tags.insert(osm_t::TagMap::value_type("railway", "abandoned"));
  way->tags.replace(tags);
  style->colorize(way);
  assert_cmpnum(way->draw.color, 0xf2eee8ff);
  assert_cmpnum(way->draw.width, 4);
  assert_cmpnum(way->draw.dash_length_on, 4);
  assert_cmpnum(way->draw.dash_length_off, 4);
  assert_cmpnum(way->draw.bg.color, 0xccccccff);
  assert_cmpnum(way->draw.bg.width, 6);

  // this one should take priority
  tags.insert(osm_t::TagMap::value_type("highway", "primary"));
  way->tags.replace(tags);
  style->colorize(way);
  assert_cmpnum(way->draw.color, 0xeb9898ff);
  assert_cmpnum(way->draw.width, 9);
  assert_cmpnum(way->draw.dash_length_on, 0);
  assert_cmpnum(way->draw.dash_length_off, 0);
  assert_cmpnum(way->draw.bg.color, 0xc48080ff);
  assert_cmpnum(way->draw.bg.width, 11);

  xmlCleanupParser();

  return 0;
}

#include "dummy_appdata.h"
