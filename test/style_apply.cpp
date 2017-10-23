#include <josm_elemstyles.h>
#include <josm_elemstyles_p.h>

#include <appdata.h>
#include <icon.h>
#include <misc.h>
#include <style.h>

#include <osm2go_annotations.h>
#include <osm2go_cpp.h>

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <iostream>

appdata_t::appdata_t()
  : window(O2G_NULLPTR)
#ifdef FREMANTLE
  , osso_context(O2G_NULLPTR)
#endif
  , statusbar(O2G_NULLPTR)
  , settings(O2G_NULLPTR)
  , gps_state(O2G_NULLPTR)
{
}

appdata_t::~appdata_t()
{
}

int main(int argc, char **argv)
{
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " style.xml" << std::endl;
    return 1;
  }

#if !GLIB_CHECK_VERSION(2,36,0)
  g_type_init();
#endif

  xmlInitParser();
  misc_init();

  appdata_t appdata;

  style_t *style = style_load(argv[1], appdata.icons);

  assert(style->frisket.border.present);
  assert_cmpnum(style->frisket.border.color, 0xff0000c0);
  assert_cmpnum(style->frisket.border.width, 20.75);
  assert_cmpnum(style->frisket.color, 0x0f0f0fff);
  assert_cmpnum(style->frisket.mult, 3.5);
  assert_cmpnum(style->highlight.color, 0xffff00c0);
  assert_cmpnum(style->highlight.node_color, 0xff00000c);
  assert_cmpnum(style->highlight.touch_color, 0x0000ffc0);
  assert_cmpnum(style->highlight.arrow_color, 0xf0f0f0f0);
  assert_cmpnum(style->highlight.width, 2.5);
  assert_cmpnum(style->highlight.arrow_limit, 1.25);
  assert_cmpnum(style->track.width, 3.5);
  assert_cmpnum(style->track.color, 0x0000ff40);
  assert_cmpnum(style->track.gps_color, 0x00008040);
  assert_cmpnum(style->background.color, 0x00ff00ff);

  if(style == O2G_NULLPTR) {
    std::cerr << "failed to load styles" << std::endl;
    return 1;
  }

  assert(!style->elemstyles.empty());

  osm_t osm(appdata.icons);

  memset(&osm.rbounds, 0, sizeof(osm.rbounds));
  osm.bounds = &osm.rbounds;

  node_t * const node = osm.node_new(pos_t(0.0, 0.0));
  osm.node_attach(node);

  style->colorize_node(node);

  assert(style->node_icons.empty());

  osm_t::TagMap tags;
  tags.insert(osm_t::TagMap::value_type("barrier", "bollard"));
  node->tags.replace(tags);

  style->colorize_node(node);

  assert(style->node_icons.empty());

  // this should actually apply
  tags.insert(osm_t::TagMap::value_type("access", "no"));
  node->tags.replace(tags);

  style->colorize_node(node);

  assert(!style->node_icons.empty());
  assert(style->node_icons[node->id] != O2G_NULLPTR);

  icon_t::icon_item *oldicon = style->node_icons[node->id];
  float oldzoom = node->zoom_max;

  // this should change the icon and zoom_max
  tags.insert(osm_t::TagMap::value_type("addr:housenumber", "42"));
  node->tags.replace(tags);

  style->colorize_world(&osm);

  assert(!style->node_icons.empty());
  assert(style->node_icons[node->id] != O2G_NULLPTR);
  assert(oldicon != style->node_icons[node->id]);
  assert_cmpnum_op(oldzoom * 1.9, <, node->zoom_max);

  way_t * const way = new way_t(0);
  osm.way_attach(way);

  style->colorize_world(&osm);
  // default values for all ways set in test1.style
  way_t w0;
  w0.draw.width = 3;
  w0.draw.color = 0x999999ff;
  assert_cmpmem(&(way->draw), sizeof(way->draw), &(w0.draw), sizeof(w0.draw));

  // apply a way style (linemod)
  tags.clear();
  tags.insert(osm_t::TagMap::value_type("bridge", "yes"));
  way->tags.replace(tags);
  style->colorize_way(way);
  assert_cmpnum_op(memcmp(&(way->draw), &(w0.draw), sizeof(w0.draw)), !=, 0);
  assert_cmpnum(way->draw.color, 0x00008080);
  assert_cmpnum(way->draw.width, 7);

  // 2 colliding linemods
  // only the last one should be used
  tags.insert(osm_t::TagMap::value_type("bridge", "yes"));
  tags.insert(osm_t::TagMap::value_type("access", "no"));
  way->tags.replace(tags);
  style->colorize_way(way);
  assert_cmpnum(way->draw.color, 0xff8080ff);
  assert_cmpnum(way->draw.width, 5);

  // apply way style (line)
  tags.clear();
  tags.insert(osm_t::TagMap::value_type("highway", "residential"));
  way->tags.replace(tags);
  style->colorize_way(way);
  assert_cmpnum(way->draw.color, 0xc0c0c0ff);
  assert_cmpnum(way->draw.width, 2);

  // apply way style (line, area style not matching)
  tags.clear();
  tags.insert(osm_t::TagMap::value_type("highway", "platform"));
  way->tags.replace(tags);
  style->colorize_way(way);
  assert_cmpnum(way->draw.color, 0x809bc0ff);
  assert_cmpnum(way->draw.width, 1);

  way_t * const area = new way_t(1);
  area->append_node(node);
  node_t *tmpn = osm.node_new(pos_t(0.0, 1.0));
  osm.node_attach(tmpn);
  area->append_node(tmpn);
  tmpn = osm.node_new(pos_t(1.0, 1.0));
  osm.node_attach(tmpn);
  area->append_node(tmpn);
  osm.way_attach(area);

  assert(!area->is_closed());
  area->append_node(node);
  assert(area->is_closed());

  // apply styling
  tags.clear();
  tags.insert(osm_t::TagMap::value_type("public_transport", "platform"));
  area->tags.replace(tags);
  node->tags.replace(tags);
  way->tags.replace(tags);

  oldicon = style->node_icons[node->id];
  oldzoom = node->zoom_max;

  style->colorize_world(&osm);
  assert_cmpnum(way->draw.color, 0xccccccff);
  assert_cmpnum(way->draw.area.color, 0);
  assert_cmpnum(way->draw.width, 1);

  assert(!style->node_icons.empty());
  assert(style->node_icons[node->id] != O2G_NULLPTR);
  assert(oldicon != style->node_icons[node->id]);
  assert_cmpnum_op(oldzoom, !=, node->zoom_max);

  assert_cmpnum(area->draw.color, 0xccccccff);
  // test1.xml says color #ddd, test1.style says color 0x00000066
  assert_cmpnum(area->draw.area.color, 0xdddddd66);
  assert_cmpnum(area->draw.width, 1);

  // check priorities
  tags.insert(osm_t::TagMap::value_type("train", "yes"));
  area->tags.replace(tags);
  node->tags.replace(tags);
  way->tags.replace(tags);

  style->colorize_world(&osm);
  assert_cmpnum(way->draw.color, 0xaaaaaaff);
  assert_cmpnum(way->draw.area.color, 0);
  assert_cmpnum(way->draw.width, 2);

  assert(!style->node_icons.empty());
  assert(style->node_icons[node->id] != O2G_NULLPTR);
  assert(oldicon != style->node_icons[node->id]);
  assert_cmpnum_op(oldzoom, !=, node->zoom_max);

  assert_cmpnum(area->draw.color, 0xaaaaaaff);
  // test1.xml says color #bbb, test1.style says color 0x00000066
  assert_cmpnum(area->draw.area.color, 0xbbbbbb66);
  assert_cmpnum(area->draw.width, 2);

  delete style;

  xmlCleanupParser();

  return 0;
}
