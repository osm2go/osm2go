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

  josm_elemstyles_colorize_node(style, node);

  g_assert_false(style->node_icons.empty());
  g_assert_nonnull(style->node_icons[node->id]);
  g_assert_nonnull(*(style->iconP));
  g_assert(oldicon != style->node_icons[node->id]);
  g_assert_cmpfloat(oldzoom * 1.9, <, node->zoom_max);

  delete style;

  xmlCleanupParser();

  return 0;
}
