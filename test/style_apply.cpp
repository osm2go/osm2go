#include <josm_elemstyles.h>
#include <josm_elemstyles_p.h>

#include <appdata.h>
#include <icon.h>
#include <misc.h>
#include <style.h>

#include <osm2go_cpp.h>

#include <algorithm>
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

  appdata_t appdata;

  style_t *style = style_load(argv[1], appdata.icons);

  g_assert_true(style->frisket.border.present);
  g_assert_cmpuint(style->frisket.border.color, ==, 0xff0000c0);
  g_assert_cmpfloat(style->frisket.border.width, ==, 20.75);
  g_assert_cmpuint(style->frisket.color, ==, 0x0f0f0fff);
  g_assert_cmpfloat(style->frisket.mult, ==, 3.5);
  g_assert_cmpuint(style->highlight.color, ==, 0xffff00c0);
  g_assert_cmpuint(style->highlight.node_color, ==, 0xff00000c);
  g_assert_cmpuint(style->highlight.touch_color, ==, 0x0000ffc0);
  g_assert_cmpuint(style->highlight.arrow_color, ==, 0xf0f0f0f0);
  g_assert_cmpfloat(style->highlight.width, ==, 2.5);
  g_assert_cmpfloat(style->highlight.arrow_limit, ==, 1.25);
  g_assert_cmpfloat(style->track.width, ==, 3.5);
  g_assert_cmpuint(style->track.color, ==, 0x0000ff40);
  g_assert_cmpuint(style->track.gps_color, ==, 0x00008040);
  g_assert_cmpuint(style->background.color, ==, 0x00ff00ff);

  if(style == O2G_NULLPTR) {
    std::cerr << "failed to load styles" << std::endl;
    return 1;
  }

  g_assert_false(style->elemstyles.empty());

  osm_t osm(appdata.icons);

  memset(&osm.rbounds, 0, sizeof(osm.rbounds));
  osm.bounds = &osm.rbounds;

  node_t * const node = osm.node_new(pos_t(0.0, 0.0));
  osm.node_attach(node);

  style->colorize_node(node);

  g_assert_true(style->node_icons.empty());

  osm_t::TagMap tags;
  tags.insert(osm_t::TagMap::value_type("barrier", "bollard"));
  node->tags.replace(tags);

  style->colorize_node(node);

  g_assert_true(style->node_icons.empty());

  // this should actually apply
  tags.insert(osm_t::TagMap::value_type("access", "no"));
  node->tags.replace(tags);

  style->colorize_node(node);

  g_assert_false(style->node_icons.empty());
  g_assert_nonnull(style->node_icons[node->id]);

  GdkPixbuf *oldicon = style->node_icons[node->id];
  float oldzoom = node->zoom_max;

  // this should change the icon and zoom_max
  tags.insert(osm_t::TagMap::value_type("addr:housenumber", "42"));
  node->tags.replace(tags);

  style->colorize_world(&osm);

  g_assert_false(style->node_icons.empty());
  g_assert_nonnull(style->node_icons[node->id]);
  g_assert(oldicon != style->node_icons[node->id]);
  g_assert_cmpfloat(oldzoom * 1.9, <, node->zoom_max);

  way_t * const way = new way_t(0);
  osm.way_attach(way);

  style->colorize_world(&osm);
  // default values for all ways set in test1.style
  way_t w0;
  w0.draw.width = 3;
  w0.draw.color = 0x999999ff;
  g_assert_cmpmem(&(way->draw), sizeof(way->draw), &(w0.draw), sizeof(w0.draw));

  // apply a way style (linemod)
  tags.clear();
  tags.insert(osm_t::TagMap::value_type("bridge", "yes"));
  way->tags.replace(tags);
  style->colorize_way(way);
  g_assert_cmpint(memcmp(&(way->draw), &(w0.draw), sizeof(w0.draw)), !=, 0);
  g_assert_cmpuint(way->draw.color, ==, 0x00008080);
  g_assert_cmpint(way->draw.width, ==, 7);

  // 2 colliding linemods
  // only the last one should be used
  tags.insert(osm_t::TagMap::value_type("bridge", "yes"));
  tags.insert(osm_t::TagMap::value_type("access", "no"));
  way->tags.replace(tags);
  style->colorize_way(way);
  g_assert_cmpuint(way->draw.color, ==, 0xff8080ff);
  g_assert_cmpint(way->draw.width, ==, 5);

  // apply way style (line)
  tags.clear();
  tags.insert(osm_t::TagMap::value_type("highway", "residential"));
  way->tags.replace(tags);
  style->colorize_way(way);
  g_assert_cmpuint(way->draw.color, ==, 0xc0c0c0ff);
  g_assert_cmpint(way->draw.width, ==, 2);

  // apply way style (line, area style not matching)
  tags.clear();
  tags.insert(osm_t::TagMap::value_type("highway", "platform"));
  way->tags.replace(tags);
  style->colorize_way(way);
  g_assert_cmpuint(way->draw.color, ==, 0x809bc0ff);
  g_assert_cmpint(way->draw.width, ==, 1);

  way_t * const area = new way_t(1);
  area->append_node(node);
  node_t *tmpn = osm.node_new(pos_t(0.0, 1.0));
  osm.node_attach(tmpn);
  area->append_node(tmpn);
  tmpn = osm.node_new(pos_t(1.0, 1.0));
  osm.node_attach(tmpn);
  area->append_node(tmpn);
  osm.way_attach(area);

  g_assert_false(area->is_closed());
  area->append_node(node);
  g_assert_true(area->is_closed());

  // apply styling
  tags.clear();
  tags.insert(osm_t::TagMap::value_type("public_transport", "platform"));
  area->tags.replace(tags);
  node->tags.replace(tags);
  way->tags.replace(tags);

  oldicon = style->node_icons[node->id];
  oldzoom = node->zoom_max;

  style->colorize_world(&osm);
  g_assert_cmpuint(way->draw.color, ==, 0xccccccff);
  g_assert_cmpint(way->draw.area.color, ==, 0);
  g_assert_cmpint(way->draw.width, ==, 1);

  g_assert_false(style->node_icons.empty());
  g_assert_nonnull(style->node_icons[node->id]);
  g_assert(oldicon != style->node_icons[node->id]);
  g_assert_cmpfloat(oldzoom, !=, node->zoom_max);

  g_assert_cmpuint(area->draw.color, ==, 0xccccccff);
  // test1.xml says color #ddd, test1.style says color 0x00000066
  g_assert_cmpuint(area->draw.area.color, ==, 0xdddddd66);
  g_assert_cmpint(area->draw.width, ==, 1);

  // check priorities
  tags.insert(osm_t::TagMap::value_type("train", "yes"));
  area->tags.replace(tags);
  node->tags.replace(tags);
  way->tags.replace(tags);

  style->colorize_world(&osm);
  g_assert_cmpuint(way->draw.color, ==, 0xaaaaaaff);
  g_assert_cmpint(way->draw.area.color, ==, 0);
  g_assert_cmpint(way->draw.width, ==, 2);

  g_assert_false(style->node_icons.empty());
  g_assert_nonnull(style->node_icons[node->id]);
  g_assert(oldicon != style->node_icons[node->id]);
  g_assert_cmpfloat(oldzoom, !=, node->zoom_max);

  g_assert_cmpuint(area->draw.color, ==, 0xaaaaaaff);
  // test1.xml says color #bbb, test1.style says color 0x00000066
  g_assert_cmpuint(area->draw.area.color, ==, 0xbbbbbb66);
  g_assert_cmpint(area->draw.width, ==, 2);

  delete style;

  xmlCleanupParser();

  return 0;
}
