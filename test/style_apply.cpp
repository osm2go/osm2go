#include <josm_elemstyles.h>
#include <josm_elemstyles_p.h>

#include <appdata.h>
#include <icon.h>
#include <settings.h>
#include <style.h>

#include <osm2go_cpp.h>

#include <algorithm>
#include <cstdlib>
#include <iostream>

appdata_t::appdata_t()
{
  memset(this, 0, sizeof(*this));
  settings = g_new0(settings_t, 1);
}

appdata_t::~appdata_t()
{
  icon_free_all(icon);
  g_free(settings);
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
  appdata.settings->style = argv[1];

  style_t *style = style_load(&appdata);

  if(style == O2G_NULLPTR) {
    std::cerr << "failed to load styles" << std::endl;
    return 1;
  }

  g_assert_false(style->elemstyles.empty());

  osm_t osm;

  memset(&osm.rbounds, 0, sizeof(osm.rbounds));
  osm.bounds = &osm.rbounds;

  node_t *node = osm.node_new(pos_t(0.0, 0.0));
  osm.node_attach(node);

  josm_elemstyles_colorize_node(style, node);

  g_assert_true(style->node_icons.empty());
  g_assert_null(*(style->iconP));

  osm_t::TagMap tags;
  tags.insert(osm_t::TagMap::value_type("barrier", "bollard"));
  node->tags.replace(tags);

  josm_elemstyles_colorize_node(style, node);

  g_assert_true(style->node_icons.empty());
  g_assert_null(*(style->iconP));

  // this should actually apply
  tags.insert(osm_t::TagMap::value_type("access", "no"));
  node->tags.replace(tags);

  josm_elemstyles_colorize_node(style, node);

  g_assert_false(style->node_icons.empty());
  g_assert_nonnull(style->node_icons[node->id]);
  g_assert_nonnull(*(style->iconP));

  GdkPixbuf *oldicon = style->node_icons[node->id];
  float oldzoom = node->zoom_max;

  // this should change the icon and zoom_max
  tags.insert(osm_t::TagMap::value_type("addr:housenumber", "42"));
  node->tags.replace(tags);

  josm_elemstyles_colorize_world(style, &osm);

  g_assert_false(style->node_icons.empty());
  g_assert_nonnull(style->node_icons[node->id]);
  g_assert_nonnull(*(style->iconP));
  g_assert(oldicon != style->node_icons[node->id]);
  g_assert_cmpfloat(oldzoom * 1.9, <, node->zoom_max);

  way_t *way = new way_t(1);
  osm.way_attach(way);

  josm_elemstyles_colorize_world(style, &osm);
  // default values for all ways set in test1.style
  way_t w0;
  w0.draw.width = 3;
  w0.draw.color = 0x999999ff;
  g_assert_cmpint(memcmp(&(way->draw), &(w0.draw), sizeof(w0.draw)), ==, 0);

  // apply a way style
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
  g_assert_cmpint(memcmp(&(way->draw), &(w0.draw), sizeof(w0.draw)), !=, 0);
  g_assert_cmpuint(way->draw.color, ==, 0xff8080ff);
  g_assert_cmpint(way->draw.width, ==, 5);

  delete style;

  xmlCleanupParser();

  return 0;
}
