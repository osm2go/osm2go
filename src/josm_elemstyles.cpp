/*
 * Copyright (C) 2008 Till Harbaum <till@harbaum.org>.
 *
 * This file is part of OSM2Go.
 *
 * OSM2Go is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OSM2Go is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OSM2Go.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
  http://josm.openstreetmap.de/svn/trunk/styles/standard/elemstyles.xml
*/

#include "josm_elemstyles.h"
#include "josm_elemstyles_p.h"

#include "appdata.h"
#include "josm_presets.h"
#include "map.h"
#include "misc.h"
#include "style.h"

#include <algorithm>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <map>
#include <strings.h>

#ifndef LIBXML_TREE_ENABLED
#error "Tree not enabled in libxml"
#endif

class StyleSax {
  xmlSAXHandler handler;

  enum State {
    DocStart,
    TagRules,
    TagRule,
    TagCondition,
    TagLine,
    TagLineMod,
    TagArea,
    TagIcon,
    TagScaleMin,
    TagScaleMax
  };

  State state;

  // custom find to avoid memory allocations for std::string
  struct tag_find {
    const char * const name;
    tag_find(const xmlChar *n) : name(reinterpret_cast<const char *>(n)) {}
    bool operator()(const std::pair<const char *, std::pair<State, State> > &p) {
      return (strcmp(p.first, name) == 0);
    }
  };

public:
  StyleSax();

  bool parse(const std::string &filename);

  std::vector<elemstyle_t *> styles;

private:
  std::map<const char *, std::pair<State, State> > tags;

  void characters(const char *ch, int len);
  static void cb_characters(void *ts, const xmlChar *ch, int len) {
    static_cast<StyleSax *>(ts)->characters(reinterpret_cast<const char *>(ch), len);
  }
  void startElement(const xmlChar *name, const xmlChar **attrs);
  static void cb_startElement(void *ts, const xmlChar *name, const xmlChar **attrs) {
    static_cast<StyleSax *>(ts)->startElement(name, attrs);
  }
  void endElement(const xmlChar *name);
  static void cb_endElement(void *ts, const xmlChar *name) {
    static_cast<StyleSax *>(ts)->endElement(name);
  }
};

// ratio conversions

// Scaling constants. Our "zoom" is a screenpx:canvasunit ratio, and the figure
// given by an elemstyles.xml is the denominator of a screen:real ratio.

#define N810_PX_PER_METRE (800 / 0.09)
    // XXX should probably ask the windowing system for DPI and
    // work from that instead

float scaledn_to_zoom(const float scaledn) {
  return N810_PX_PER_METRE / scaledn;
}

float zoom_to_scaledn(const float zoom) {
  return N810_PX_PER_METRE / zoom;
}


/* --------------------- elemstyles.xml parsing ----------------------- */

static bool parse_color(const xmlChar *color_str, elemstyle_color_t &color) {
  bool ret = false;

  /* if the color name contains a # it's a hex representation */
  /* we parse this directly since gdk_color_parse doesn't cope */
  /* with the alpha channel that may be present */
  if(*color_str == '#' && strlen((const char*)color_str) == 9) {
    char *err;

    color = strtoul((const char*)color_str + 1, &err, 16);

    ret = (*err == '\0') ? true : false;
  } else {
    GdkColor gdk_color;
    if(gdk_color_parse((const gchar*)color_str, &gdk_color)) {
      color =
            ((gdk_color.red   << 16) & 0xff000000) |
            ((gdk_color.green <<  8) & 0xff0000) |
            ((gdk_color.blue       ) & 0xff00) |
             (0xff);

      ret = true;
    }
  }

  return ret;
}

bool parse_color(xmlNode *a_node, const char *name,
		     elemstyle_color_t &color) {
  xmlChar *color_str = xmlGetProp(a_node, BAD_CAST name);
  bool ret = false;

  if(color_str) {
    ret = parse_color(color_str, color);
    xmlFree(color_str);
  }
  return ret;
}

static double parse_scale(const char *val_str, int len) {
  char buf[G_ASCII_DTOSTR_BUF_SIZE];
  if(G_UNLIKELY(static_cast<unsigned int>(len) >= sizeof(buf))) {
    return 0.0;
  } else {
    memcpy(buf, val_str, len);
    buf[len] = '\0';
    return scaledn_to_zoom(strtod(buf, 0));
  }
}

static gboolean parse_gboolean(const xmlChar *bool_str) {
  static const char *true_str[]  = { "1", "yes", "true", 0 };
  gboolean ret = FALSE;
  for (int i = 0; true_str[i]; ++i) {
    if (strcasecmp((const char*)bool_str, true_str[i])==0) {
      ret = TRUE;
      break;
    }
  }
  return ret;
}

StyleSax::StyleSax()
  : state(DocStart)
{
  memset(&handler, 0, sizeof(handler));
  handler.characters = cb_characters;
  handler.startElement = cb_startElement;
  handler.endElement = cb_endElement;

  tags["rules"] = std::pair<State, State>(DocStart, TagRules);
  tags["rule"] = std::pair<State, State>(TagRules, TagRule);
  tags["condition"] = std::pair<State, State>(TagRule, TagCondition);
  tags["line"] = std::pair<State, State>(TagRule, TagLine);
  tags["linemod"] = std::pair<State, State>(TagRule, TagLineMod);
  tags["area"] = std::pair<State, State>(TagRule, TagArea);
  tags["icon"] = std::pair<State, State>(TagRule, TagIcon);
  tags["scale_min"] = std::pair<State, State>(TagRule, TagScaleMin);
  tags["scale_max"] = std::pair<State, State>(TagRule, TagScaleMax);
}

bool StyleSax::parse(const std::string &filename)
{
  if (xmlSAXUserParseFile(&handler, this, filename.c_str()) != 0)
    return false;

  return !styles.empty();
}

void StyleSax::characters(const char *ch, int len)
{
  std::string buf;

  switch(state) {
  case TagScaleMin:
    // currently ignored, only check syntax
    (void)parse_scale(ch, len);
    break;
  case TagScaleMax: {
    elemstyle_t * const elemstyle = styles.back();
    switch (elemstyle->type) {
    case ES_TYPE_LINE:
      elemstyle->line->zoom_max = parse_scale(ch, len);
      break;
    case ES_TYPE_AREA:
      elemstyle->area->zoom_max = parse_scale(ch, len);
      break;
    default:
      if (G_LIKELY(elemstyle->icon.filename))
        elemstyle->icon.zoom_max = parse_scale(ch, len);
      else
        printf("scale_max for unhandled elemstyletype=0x%02x, rule %zu\n",
               elemstyle->type, styles.size());
      break;
    }
    break;
  }
  default:
    for(int pos = 0; pos < len; pos++)
      if(!isspace(ch[pos])) {
        printf("unhandled character data: %*.*s state %i\n", len, len, ch, state);
        break;
      }
  }
}

/* parse "+123", "-123" and "123%" */
static void parse_width_mod(const char *mod_str, elemstyle_width_mod_t &value) {
  if(strlen(mod_str) > 0) {
    if(mod_str[0] == '+') {
      value.mod = ES_MOD_ADD;
      value.width = strtoul(mod_str+1, NULL, 10);
    } else if(mod_str[0] == '-') {
      value.mod = ES_MOD_SUB;
      value.width = strtoul(mod_str+1, NULL, 10);
    } else if(mod_str[strlen(mod_str)-1] == '%') {
      value.mod = ES_MOD_PERCENT;
      value.width = strtoul(mod_str, NULL, 10);
    } else
      printf("warning: unable to parse modifier %s\n", mod_str);
  }
}

void StyleSax::startElement(const xmlChar *name, const xmlChar **attrs)
{
  std::map<const char *, std::pair<State, State> >::const_iterator it =
          std::find_if(tags.begin(), tags.end(), tag_find(name));

  if(it == tags.end()) {
    fprintf(stderr, "found unhandled element %s\n", name);
    return;
  }

  if(state != it->second.first) {
    fprintf(stderr, "found element %s in state %i, but expected %i\n",
            name, state, it->second.first);
    return;
  }

  state = it->second.second;

  elemstyle_t * const elemstyle = styles.empty() ? 0 : styles.back();

  switch(state){
  case TagRule:
    styles.push_back(new elemstyle_t());
    break;
  case TagCondition: {
    xmlChar *k = 0, *v = 0;
    for(unsigned int i = 0; attrs[i]; i += 2) {
      if(strcmp(reinterpret_cast<const char *>(attrs[i]), "k") == 0)
        k = xmlStrdup(attrs[i + 1]);
      else if(strcmp(reinterpret_cast<const char *>(attrs[i]), "v") == 0)
        v = xmlStrdup(attrs[i + 1]);
    }
    if(k || v)
      styles.back()->conditions.push_back(elemstyle_condition_t(k, v));
    /* todo: add support for "b" (boolean) value */
    break;
  }
  case TagLine: {
    g_assert(elemstyle->type == ES_TYPE_NONE);
    elemstyle->type = ES_TYPE_LINE;

    bool hasBgWidth = false, hasBgColor = false;
    /* these have to be present */
    bool hasColor = false, hasWidth = false;
    elemstyle_line_t *line = elemstyle->line = g_new0(elemstyle_line_t, 1);
    line->dash_length = DEFAULT_DASH_LENGTH;

    for(unsigned int i = 0; attrs[i]; i += 2) {
      if(strcmp(reinterpret_cast<const char *>(attrs[i]), "colour") == 0) {
        hasColor = parse_color(attrs[i + 1], line->color);
      } else if(strcmp(reinterpret_cast<const char *>(attrs[i]), "width") == 0) {
        line->width = strtoul(reinterpret_cast<const char*>(attrs[i + 1]), NULL, 10);
        hasWidth = true;
      } else if(strcmp(reinterpret_cast<const char *>(attrs[i]), "realwidth") == 0) {
        line->real.width = strtoul(reinterpret_cast<const char*>(attrs[i + 1]), NULL, 10);
        line->real.valid = true;
      } else if(strcmp(reinterpret_cast<const char *>(attrs[i]), "width_bg") == 0) {
        line->bg.width = strtoul(reinterpret_cast<const char*>(attrs[i + 1]), NULL, 10);
        hasBgWidth = true;
      } else if(strcmp(reinterpret_cast<const char *>(attrs[i]), "colour_bg") == 0) {
        line->bg.color = strtoul(reinterpret_cast<const char*>(attrs[i + 1]), NULL, 10);
        hasBgColor = true;
      } else if(strcmp(reinterpret_cast<const char *>(attrs[i]), "dashed") == 0) {
        line->dashed = parse_gboolean(attrs[i + 1]);
      } else if(strcmp(reinterpret_cast<const char *>(attrs[i]), "dash_length") == 0) {
        line->dash_length = strtoul(reinterpret_cast<const char*>(attrs[i + 1]), NULL, 10);
      }
    }

    line->bg.valid = hasBgColor && hasBgWidth;

    g_assert(hasColor);
    g_assert(hasWidth);

    break;
  }
  case TagLineMod: {
    g_assert(elemstyle->type == ES_TYPE_NONE);
    elemstyle->type = ES_TYPE_LINE_MOD;

    elemstyle_line_mod_t &line_mod = elemstyle->line_mod;

    for(unsigned int i = 0; attrs[i]; i += 2) {
      if(strcmp(reinterpret_cast<const char *>(attrs[i]), "width") == 0)
        parse_width_mod(reinterpret_cast<const char *>(attrs[i + 1]), line_mod.line);
      else if(strcmp(reinterpret_cast<const char *>(attrs[i]), "width_bg") == 0)
        parse_width_mod(reinterpret_cast<const char *>(attrs[i + 1]), line_mod.bg);
    }
    break;
  }
  case TagArea: {
    g_assert(elemstyle->type == ES_TYPE_NONE);
    elemstyle->type = ES_TYPE_AREA;
    elemstyle->area = g_new0(elemstyle_area_t, 1);

    bool hasColor = false;
    for(unsigned int i = 0; attrs[i]; i += 2) {
      if(strcmp(reinterpret_cast<const char *>(attrs[i]), "colour") == 0)
        hasColor = parse_color(attrs[i + 1], elemstyle->area->color);
    }

    /* this has to be present */
    g_assert(hasColor);
    break;
  }
  case TagIcon:
    for(unsigned int i = 0; attrs[i]; i += 2) {
      if(strcmp(reinterpret_cast<const char *>(attrs[i]), "annotate") == 0)
        elemstyle->icon.annotate = strcmp(reinterpret_cast<const char *>(attrs[i + 1]), "true");
      else if(strcmp(reinterpret_cast<const char *>(attrs[i]), "src") == 0)
        elemstyle->icon.filename = josm_icon_name_adjust(reinterpret_cast<char *>(xmlStrdup(attrs[i + 1])));
    }

    g_assert(elemstyle->icon.filename);

    break;
  default:
    break;
  }
}

void StyleSax::endElement(const xmlChar *name)
{
  std::map<const char *, std::pair<State, State> >::const_iterator it =
          std::find_if(tags.begin(), tags.end(), tag_find(name));

  g_assert(it != tags.end());
  g_assert(state == it->second.second);

  if(state == TagRule && styles.back()->conditions.empty())
    printf("Rule %zu has no conditions\n", styles.size());

  state = it->second.first;
}

std::vector<elemstyle_t *> josm_elemstyles_load(const char *name) {
  printf("Loading JOSM elemstyles ...\n");

  const std::string &filename = find_file(name);
  if(filename.empty()) {
    printf("elemstyle file not found\n");
    return std::vector<elemstyle_t *>();
  }

  StyleSax sx;
  if(!sx.parse(filename))
    fprintf(stderr, "error parsing elemstyles\n");

  return sx.styles;
}

/* ----------------------- cleaning up --------------------- */

static void free_line(elemstyle_line_t *line) {
  g_free(line);
}

static void free_area(elemstyle_area_t *area) {
  g_free(area);
}

static void free_condition(elemstyle_condition_t &cond) {
  xmlFree(cond.key);
  xmlFree(cond.value);
}

static inline void elemstyle_free(elemstyle_t *elemstyle) {
  delete elemstyle;
}

void josm_elemstyles_free(std::vector<elemstyle_t *> &elemstyles) {
  std::for_each(elemstyles.begin(), elemstyles.end(), elemstyle_free);
  elemstyles.clear();
}

#define WIDTH_SCALE (1.0)

struct condition_not_matches_obj {
  const base_object_t * const obj;
  condition_not_matches_obj(const base_object_t *o) : obj(o) {}
  bool operator()(const elemstyle_condition_t &cond);
};

bool condition_not_matches_obj::operator()(const elemstyle_condition_t &cond) {
  if(cond.key) {
    const char *value = obj->get_value((char*)cond.key);
    if(!value || (cond.value && strcasecmp(value, (char*)cond.value) != 0))
      return true;
  } else if(cond.value) {
    if(!obj->has_value((char*)cond.value))
      return true;
  }
  return false;
}

struct colorize_node {
  const style_t * const style;
  node_t * const node;
  bool &somematch;
  colorize_node(const style_t *s, node_t *n, bool &m) : style(s), node(n), somematch(m) {}
  void operator()(elemstyle_t *elemstyle);
};

void colorize_node::operator()(elemstyle_t *elemstyle)
{
  // Rule without conditions matches everything (should it?)
  // For rule with conditions, if any condition mismatches->rule mismatches
  bool match = std::find_if(elemstyle->conditions.begin(),
                            elemstyle->conditions.end(),
                            condition_not_matches_obj(node)) == elemstyle->conditions.end();

  somematch |= match;

  if(match && elemstyle->icon.filename) {
    char *name = g_strjoin("/", "styles", style->icon.path_prefix,
                           elemstyle->icon.filename, 0);

    /* free old icon if there's one present */
    if(node->icon_buf) {
      icon_free(style->iconP, node->icon_buf);
      node->icon_buf = NULL;
    }

    node->icon_buf = icon_load(style->iconP, name);
    g_free(name);

    if (elemstyle->icon.zoom_max > 0)
      node->zoom_max = elemstyle->icon.zoom_max;
  }
}

void josm_elemstyles_colorize_node(const style_t *style, node_t *node) {
  node->zoom_max = style->node.zoom_max;

  bool somematch = false;
  colorize_node fc(style, node, somematch);
  std::for_each(style->elemstyles.begin(), style->elemstyles.end(), fc);

  /* clear icon for node if not matched at least one rule and has an icon attached */
  if (!somematch && node->icon_buf) {
    icon_free(style->iconP, node->icon_buf);
    node->icon_buf = NULL;
  }
}

struct josm_elemstyles_colorize_node_functor {
  const style_t * const styles;
  josm_elemstyles_colorize_node_functor(const style_t *s) : styles(s) {}
  void operator()(std::pair<item_id_t, node_t *> pair);
};

void josm_elemstyles_colorize_node_functor::operator()(std::pair<item_id_t, node_t *> pair) {
  node_t * const node = pair.second;
  /* remove all icon references that may still be there from */
  /* an old style */
  if(node->icon_buf) {
    icon_free(styles->iconP, node->icon_buf);
    node->icon_buf = NULL;
  }

  josm_elemstyles_colorize_node(styles, node);
}

static void line_mod_apply(gint *width, const elemstyle_width_mod_t *mod) {
  switch(mod->mod) {
  case ES_MOD_NONE:
    break;

  case ES_MOD_ADD:
    *width += mod->width;
    break;

  case ES_MOD_SUB:
    *width -= mod->width;
    break;

  case ES_MOD_PERCENT:
    *width = 100 * *width / mod->width;
    break;
  }
}

struct josm_elemstyles_colorize_way_functor {
  const style_t * const style;
  josm_elemstyles_colorize_way_functor(const style_t *s) : style(s) {}
  void operator()(way_t *way);
  void operator()(std::pair<item_id_t, way_t *> pair) {
    operator()(pair.second);
  }

  struct apply_condition {
    const style_t * const style;
    way_t * const way;
    /* during the elemstyle search a line_mod may be found. save it here */
    const elemstyle_line_mod_t **line_mod;
    bool way_processed;
    bool way_is_closed;
    apply_condition(const style_t *s, way_t *w, const elemstyle_line_mod_t **l)
      : style(s), way(w), line_mod(l), way_processed(false)
      , way_is_closed(way->is_closed()) {}
    void operator()(const elemstyle_t *elemstyle);
  };
};

void josm_elemstyles_colorize_way_functor::apply_condition::operator()(const elemstyle_t* elemstyle)
{
  /* this entry does not contain line or area descriptions and is */
  /* likely just an icon. ignore this as it doesn't make much sense */
  /* for a way */
  if(elemstyle->type == ES_TYPE_NONE)
    return;

  bool match = std::find_if(elemstyle->conditions.begin(),
                            elemstyle->conditions.end(),
                            condition_not_matches_obj(way)) == elemstyle->conditions.end();

  if(!match)
    return;

  switch(elemstyle->type) {
  case ES_TYPE_NONE:
    // already handled above
    g_assert_not_reached();
    break;

  case ES_TYPE_LINE:
    if(!way_processed) {
      way->draw.color = elemstyle->line->color;
      way->draw.width =  WIDTH_SCALE * elemstyle->line->width;
      if(elemstyle->line->bg.valid) {
        way->draw.flags |= OSM_DRAW_FLAG_BG;
        way->draw.bg.color = elemstyle->line->bg.color;
        way->draw.bg.width =  WIDTH_SCALE * elemstyle->line->bg.width;
      }
      if (elemstyle->line->zoom_max > 0)
        way->draw.zoom_max = elemstyle->line->zoom_max;
      else
        way->draw.zoom_max = style->way.zoom_max;

      way->draw.dashed = elemstyle->line->dashed;
      way->draw.dash_length = elemstyle->line->dash_length;
      way_processed = true;
    }
    break;

  case ES_TYPE_LINE_MOD:
    /* just save the fact that a line mod was found for later */
    *line_mod = &elemstyle->line_mod;
    break;

  case ES_TYPE_AREA:
    if(way_is_closed && !way_processed) {
      way->draw.flags |= OSM_DRAW_FLAG_AREA;
      /* comment the following line for grey border around all areas */
      /* (potlatch style) */

      if(style->area.has_border_color)
        way->draw.color = style->area.border_color;
      else
        way->draw.color = elemstyle->area->color;

      way->draw.width =  WIDTH_SCALE * style->area.border_width;
      /* apply area alpha */
      way->draw.area.color =
      RGBA_COMBINE(elemstyle->area->color, style->area.color);
      if (elemstyle->area->zoom_max > 0)
        way->draw.zoom_max = elemstyle->area->zoom_max;
      else
        way->draw.zoom_max = style->area.zoom_max;

      way_processed = true;
    }
    break;
  }
}

void josm_elemstyles_colorize_way_functor::operator()(way_t *way) {
  /* use dark grey/no stroke/not filled for everything unknown */
  way->draw.color = style->way.color;
  way->draw.width = style->way.width;
  way->draw.flags = 0;
  way->draw.zoom_max = 0;   // draw at all zoom levels

  /* during the elemstyle search a line_mod may be found. save it here */
  const elemstyle_line_mod_t *line_mod = 0;
  apply_condition fc(style, way, &line_mod);

  std::for_each(style->elemstyles.begin(), style->elemstyles.end(), fc);

  /* apply the last line mod entry that has been found during search */
  if(line_mod) {
    printf("applying last matching line mod to way #" ITEM_ID_FORMAT "\n",
	   way->id);
    line_mod_apply(&way->draw.width, &line_mod->line);

    /* special case: the way does not have a background, but it is to */
    /* be modified */
    if((line_mod->bg.mod != ES_MOD_NONE) &&
       (!(way->draw.flags & OSM_DRAW_FLAG_BG))) {
      printf("forcing background\n");

      /* add a background in black color */
      way->draw.flags |= OSM_DRAW_FLAG_BG;
      way->draw.bg.color = (0) | 0xff;
      way->draw.bg.width =  way->draw.width;
    }

    line_mod_apply(&way->draw.bg.width, &line_mod->bg);
  }
}

void josm_elemstyles_colorize_way(const style_t *style, way_t *way) {
  josm_elemstyles_colorize_way_functor f(style);
  f(way);
}

void josm_elemstyles_colorize_world(const style_t *styles, osm_t *osm) {

  printf("preparing colors\n");

  /* colorize ways */
  std::for_each(osm->ways.begin(), osm->ways.end(),
      josm_elemstyles_colorize_way_functor(styles));

  /* icons */
  std::for_each(osm->nodes.begin(), osm->nodes.end(),
      josm_elemstyles_colorize_node_functor(styles));
}

elemstyle_t::~elemstyle_t()
{
  std::for_each(conditions.begin(), conditions.end(), free_condition);

  switch(type) {
  case ES_TYPE_NONE:
    break;
  case ES_TYPE_LINE:
    free_line(line);
    break;
  case ES_TYPE_AREA:
    free_area(area);
    break;
  case ES_TYPE_LINE_MOD:
    break;
  }
}

// vim:et:ts=8:sw=2:sts=2:ai
