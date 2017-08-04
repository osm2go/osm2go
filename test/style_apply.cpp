#include <josm_elemstyles.h>
#include <josm_elemstyles_p.h>

#include <appdata.h>
#include <icon.h>
#include <style.h>

#include <osm2go_cpp.h>

#include <algorithm>
#include <cstdlib>
#include <iostream>

appdata_t::appdata_t()
  : window(O2G_NULLPTR)
#ifdef USE_HILDON
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

  josm_elemstyles_colorize_node(style, node);

  g_assert_true(style->node_icons.empty());

  osm_t::TagMap tags;
  tags.insert(osm_t::TagMap::value_type("barrier", "bollard"));
  node->tags.replace(tags);

  josm_elemstyles_colorize_node(style, node);

  g_assert_true(style->node_icons.empty());

  // this should actually apply
  tags.insert(osm_t::TagMap::value_type("access", "no"));
  node->tags.replace(tags);

  josm_elemstyles_colorize_node(style, node);

  g_assert_false(style->node_icons.empty());
  g_assert_nonnull(style->node_icons[node->id]);

  GdkPixbuf *oldicon = style->node_icons[node->id];
  float oldzoom = node->zoom_max;

  // this should change the icon and zoom_max
  tags.insert(osm_t::TagMap::value_type("addr:housenumber", "42"));
  node->tags.replace(tags);

  josm_elemstyles_colorize_world(style, &osm);

  g_assert_false(style->node_icons.empty());
  g_assert_nonnull(style->node_icons[node->id]);
  g_assert(oldicon != style->node_icons[node->id]);
  g_assert_cmpfloat(oldzoom * 1.9, <, node->zoom_max);

  way_t * const way = new way_t(0);
  osm.way_attach(way);

  josm_elemstyles_colorize_world(style, &osm);
  // default values for all ways set in test1.style
  way_t w0;
  w0.draw.width = 3;
  w0.draw.color = 0x999999ff;
  g_assert_cmpint(memcmp(&(way->draw), &(w0.draw), sizeof(w0.draw)), ==, 0);

  // apply a way style (linemod)
  tags.clear();
  tags.insert(osm_t::TagMap::value_type("bridge", "yes"));
  way->tags.replace(tags);
  josm_elemstyles_colorize_way(style, way);
  g_assert_cmpint(memcmp(&(way->draw), &(w0.draw), sizeof(w0.draw)), !=, 0);
  g_assert_cmpuint(way->draw.color, ==, 0x00008080);
  g_assert_cmpint(way->draw.width, ==, 7);

  // 2 colliding linemods
  // only the last one should be used
  tags.insert(osm_t::TagMap::value_type("bridge", "yes"));
  tags.insert(osm_t::TagMap::value_type("access", "no"));
  way->tags.replace(tags);
  josm_elemstyles_colorize_way(style, way);
  g_assert_cmpuint(way->draw.color, ==, 0xff8080ff);
  g_assert_cmpint(way->draw.width, ==, 5);

  // apply way style (line)
  tags.clear();
  tags.insert(osm_t::TagMap::value_type("highway", "residential"));
  way->tags.replace(tags);
  josm_elemstyles_colorize_way(style, way);
  g_assert_cmpuint(way->draw.color, ==, 0xc0c0c0ff);
  g_assert_cmpint(way->draw.width, ==, 2);

  // apply way style (line, area style not matching)
  tags.clear();
  tags.insert(osm_t::TagMap::value_type("highway", "platform"));
  way->tags.replace(tags);
  josm_elemstyles_colorize_way(style, way);
  g_assert_cmpuint(way->draw.color, ==, 0x809bc0ff);
  g_assert_cmpint(way->draw.width, ==, 1);

  // apply styling
  tags.clear();
  tags.insert(osm_t::TagMap::value_type("public_transport", "platform"));
  node->tags.replace(tags);
  way->tags.replace(tags);

  oldicon = style->node_icons[node->id];
  oldzoom = node->zoom_max;

  josm_elemstyles_colorize_world(style, &osm);
  g_assert_cmpuint(way->draw.color, ==, 0xccccccff);
  g_assert_cmpint(way->draw.area.color, ==, 0);
  g_assert_cmpint(way->draw.width, ==, 1);

  g_assert_false(style->node_icons.empty());
  g_assert_nonnull(style->node_icons[node->id]);
  g_assert(oldicon != style->node_icons[node->id]);
  g_assert_cmpfloat(oldzoom, !=, node->zoom_max);

  // check priorities
  tags.insert(osm_t::TagMap::value_type("train", "yes"));
  node->tags.replace(tags);
  way->tags.replace(tags);

  josm_elemstyles_colorize_world(style, &osm);
  g_assert_cmpuint(way->draw.color, ==, 0xaaaaaaff);
  g_assert_cmpint(way->draw.area.color, ==, 0);
  g_assert_cmpint(way->draw.width, ==, 2);

  g_assert_false(style->node_icons.empty());
  g_assert_nonnull(style->node_icons[node->id]);
  g_assert(oldicon != style->node_icons[node->id]);
  g_assert_cmpfloat(oldzoom, !=, node->zoom_max);

  delete style;

  xmlCleanupParser();

  return 0;
}
