/*
 * SPDX-FileCopyrightText: 2008 Till Harbaum <till@harbaum.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/*
  http://josm.openstreetmap.de/svn/trunk/styles/standard/elemstyles.xml
*/

#include "josm_elemstyles.h"
#include "josm_elemstyles_p.h"

#include "color.h"
#include "icon.h"
#include "josm_presets.h"
#include "map.h"
#include "misc.h"
#include "SaxParser.h"
#include "style.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <limits>
#include <strings.h>
#include <unordered_map>

#include "osm2go_annotations.h"
#include <osm2go_cpp.h>
#include <osm2go_platform.h>
#include "osm2go_stl.h"

#ifndef LIBXML_TREE_ENABLED
#error "Tree not enabled in libxml"
#endif

// make use of the value cache which is NOT cleared on project switch, so
// the elements inserted here will not get lost
elemstyle_condition_t::elemstyle_condition_t(const char *k, const char *v, bool inv)
  : key(tag_t::mapToCache(k))
  , value(tag_t::mapToCache(v))
  , invert(inv)
{
}

elemstyle_condition_t::elemstyle_condition_t(const char *k, bool b)
  : key(tag_t::mapToCache(k))
  , value(b)
  , invert(false)
{
}

// ratio conversions

// Scaling constants. Our "zoom" is a screenpx:canvasunit ratio, and the figure
// given by an elemstyles.xml is the denominator of a screen:real ratio.

#define N810_PX_PER_METRE (800.0f / 0.09f)
    // XXX should probably ask the windowing system for DPI and
    // work from that instead

float scaledn_to_zoom(const float scaledn) {
  return N810_PX_PER_METRE / scaledn;
}

namespace {

typedef std::unordered_map<std::string, color_t> ColorMap;

class StyleSax : public SaxParser {
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

  struct StateChange {
    StateChange(const char *nm, State os, State ns)
      : name(nm), oldState(os), newState(ns) {}
    const char *name;
    State oldState;
    State newState;
  };

  typedef std::vector<StateChange> StateMap;
  StateMap tags;
  // custom find to avoid memory allocations for std::string
  struct tag_find {
    const char * const name;
    explicit tag_find(const char *n) : name(n) {}
    inline bool operator()(const StateMap::value_type &p) const
    {
      return (strcmp(p.name, name) == 0);
    }
  };

public:
  explicit StyleSax();

  bool parse(const std::string &filename);

  std::vector<elemstyle_t *> styles;

private:
  ColorMap colors;

  void characters(const char *ch, int len) override;
  void startElement(const char *name, const char **attrs) override;
  void endElement(const char *name) override;
};

/* --------------------- elemstyles.xml parsing ----------------------- */

bool
parse_color(const char *col, color_t &color, ColorMap &colors)
{
  bool ret = false;

  /* if the color name contains a # it's a hex representation */
  const char * const hash = strchr(col, '#');
  std::string colname;
  if(hash != nullptr) {
    std::optional<color_t> parsed = osm2go_platform::parse_color_string(hash);
    if (parsed) {
      ret = true;
      color = *parsed;
      if(hash != col)
        colname.assign(col, hash - col);
    }
  } else {
    colname = col;
  }

  if(!colname.empty()) {
    const ColorMap::const_iterator it = colors.find(colname);
    if(it == colors.end()) {
      if(unlikely(!hash)) {
        printf("found invalid colour name reference '%s'\n", col);
      } else {
        colors[colname] = color;
      }
    } else {
      if(hash == col) {
        color = it->second;
        ret = true;
      } else if(hash != nullptr)
        // check that the colors are the same if the key is specified multiple times
        assert_cmpnum(it->second, color);
    }
  }

  return ret;
}

} // namespace

bool parse_color(xmlNode *a_node, const char *name, color_t &color)
{
  xmlString color_str(xmlGetProp(a_node, BAD_CAST name));
  bool ret = false;

  if(!color_str.empty()) {
    ColorMap dummy;
    ret = parse_color(color_str, color, dummy);
  }
  return ret;
}

namespace {

float
parse_scale_buf(const char *buf)
{
  return scaledn_to_zoom(osm2go_platform::string_to_double(buf));
}

float
parse_scale(const char *val_str, int len)
{
  char buf[32];
  if(unlikely(static_cast<unsigned int>(len) >= sizeof(buf))) {
    // just for the case someone is really excessive with trailing or leading zeroes
    const std::string sbuf(val_str, len);
    return parse_scale_buf(sbuf.c_str());
  } else {
    memcpy(buf, val_str, len);
    buf[len] = '\0';
    return parse_scale_buf(buf);
  }
}

const std::array<const char *, 3> true_values = { { "1", "yes", "true" } };
const std::array<const char *, 3> false_values = { { "0", "no", "false" } };

class case_match {
  const char * const needle;
public:
  explicit inline case_match(const char *n) : needle(n) {}
  inline bool operator()(const char *v) const
  {
    return strcasecmp(needle, v) == 0;
  }
};

bool
parse_boolean(const char *bool_str, const std::array<const char *, 3> &value_strings)
{
  return std::any_of(value_strings.begin(), value_strings.end(), case_match(bool_str));
}

StyleSax::StyleSax()
  : SaxParser()
  , state(DocStart)
{
  tags.push_back(StateMap::value_type("rules", DocStart, TagRules));
  tags.push_back(StateMap::value_type("rule", TagRules, TagRule));
  tags.push_back(StateMap::value_type("condition", TagRule, TagCondition));
  tags.push_back(StateMap::value_type("line", TagRule, TagLine));
  tags.push_back(StateMap::value_type("linemod", TagRule, TagLineMod));
  tags.push_back(StateMap::value_type("area", TagRule, TagArea));
  tags.push_back(StateMap::value_type("icon", TagRule, TagIcon));
  tags.push_back(StateMap::value_type("scale_min", TagRule, TagScaleMin));
  tags.push_back(StateMap::value_type("scale_max", TagRule, TagScaleMax));
}

bool StyleSax::parse(const std::string &filename)
{
  if (!parseFile(filename))
    return false;

  return !styles.empty();
}

void StyleSax::characters(const char *ch, int len)
{
  std::string buf;

  switch(state) {
  case TagScaleMin:
    // currently ignored
    break;
  case TagScaleMax:
    styles.back()->zoom_max = parse_scale(ch, len);
    break;
  default:
    for(int pos = 0; pos < len; pos++)
      if(!isspace(ch[pos])) {
        printf("unhandled character data: %*.*s state %i\n", len, len, ch, state);
        break;
      }
  }
}

/* parse "+123", "-123" and "123%" */
void
parse_width_mod(const char *mod_str, elemstyle_width_mod_t &value)
{
  if(*mod_str != '\0') {
    if(mod_str[0] == '+') {
      value.mod = ES_MOD_ADD;
      value.width = strtoul(mod_str+1, nullptr, 10);
    } else if(mod_str[0] == '-') {
      value.mod = ES_MOD_SUB;
      value.width = strtoul(mod_str+1, nullptr, 10);
    } else if(mod_str[strlen(mod_str)-1] == '%') {
      value.mod = ES_MOD_PERCENT;
      value.width = strtoul(mod_str, nullptr, 10);
    } else
      printf("warning: unable to parse modifier %s\n", mod_str);
  }
}

int
parse_priority(const char *attr)
{
  char *endch;
  long prio = strtol(attr, &endch, 10);
  if(likely(*endch == '\0'))
    return prio;
  else
    return 0;
}

void StyleSax::startElement(const char *name, const char **attrs)
{
  StateMap::const_iterator it = std::find_if(tags.begin(), tags.end(), tag_find(name));

  if(unlikely(it == tags.end())) {
    fprintf(stderr, "found unhandled element %s\n", name);
    return;
  }

  if(unlikely(state != it->oldState)) {
    fprintf(stderr, "found element %s in state %i, but expected %i\n",
            name, state, it->oldState);
    return;
  }

  state = it->newState;

  elemstyle_t * const elemstyle = styles.empty() ? nullptr : styles.back();
  if (state != TagRule && state != TagRules && state != DocStart)
    assert(elemstyle != nullptr);

  switch(state){
  case TagRule:
    styles.push_back(new elemstyle_t());
    break;
  case TagCondition: {
    const char *k = nullptr, *v = nullptr;
    const char *b = nullptr;
    bool invert = false;

    for(unsigned int i = 0; attrs[i] != nullptr; i += 2) {
      if(strcmp(attrs[i], "k") == 0)
        k = attrs[i + 1];
      else if(strcmp(attrs[i], "v") == 0)
        v = attrs[i + 1];
      else if(strcmp(attrs[i], "b") == 0)
        b = attrs[i + 1];
      else if(strcmp(attrs[i], "invert") == 0) {
        invert = parse_boolean(attrs[i + 1], true_values);
      }
    }
    if(unlikely(k == nullptr)) {
      printf("WARNING: found condition without k(ey) attribute\n");
      break;
    }
    if(unlikely(invert && v == nullptr)) {
      printf("WARNING: found condition without v(alue) attribute, but with invert\n");
      break;
    }

    elemstyle_condition_t cond = b == nullptr ? elemstyle_condition_t(k, v, invert) :
                                 elemstyle_condition_t(k, parse_boolean(b, true_values));
    styles.back()->conditions.push_back(cond);
    break;
  }
  case TagLine: {
    assert_cmpnum(elemstyle->type & (ES_TYPE_LINE | ES_TYPE_LINE_MOD), 0);
    elemstyle->type |= ES_TYPE_LINE;

    bool hasBgWidth = false, hasBgColor = false;
    /* these have to be present */
    bool hasColor = false, hasWidth = false;
    elemstyle->line.reset(new elemstyle_line_t());
    elemstyle_line_t *line = elemstyle->line.get();

    for(unsigned int i = 0; attrs[i] != nullptr; i += 2) {
      if(strcmp(attrs[i], "colour") == 0) {
        hasColor = parse_color(attrs[i + 1], line->color, colors);
      } else if(strcmp(attrs[i], "width") == 0) {
        char *endch;
        line->width = strtoul(attrs[i + 1], &endch, 10);
        hasWidth = (*endch == '\0');
      } else if(strcmp(attrs[i], "realwidth") == 0) {
        char *endch;
        line->real.width = strtoul(attrs[i + 1], &endch, 10);
        line->real.valid = (*endch == '\0');
      } else if(strcmp(attrs[i], "width_bg") == 0) {
        char *endch;
        line->bg.width = strtoul(attrs[i + 1], &endch, 10);
        hasBgWidth = (*endch == '\0');
      } else if(strcmp(attrs[i], "colour_bg") == 0) {
        hasBgColor = parse_color(attrs[i + 1], line->bg.color, colors);
      } else if(strcmp(attrs[i], "dashed") == 0) {
        const char * const dval = attrs[i + 1];
        if(parse_boolean(dval, true_values)) {
          line->dash_length_on = DEFAULT_DASH_LENGTH;
          line->dash_length_off = DEFAULT_DASH_LENGTH;
        } else if (parse_boolean(dval, false_values)) {
          line->dash_length_on = 0;
          line->dash_length_off = 0;
        } else {
          char *end;
          line->dash_length_on = strtoul(dval, &end, 10);
          if(*end == ',')
            line->dash_length_off = strtoul(end + 1, &end, 10);
          else
            line->dash_length_off = line->dash_length_on;
          if(unlikely(*end != '\0')) {
            printf("WARNING: invalid value '%s' for dashed\n", dval);
            line->dash_length_on = 0;
            line->dash_length_off = 0;
          }
        }
      } else if(strcmp(attrs[i], "priority") == 0) {
        line->priority = parse_priority(attrs[i + 1]);
      }
    }

    line->bg.valid = hasBgColor && hasBgWidth;

    assert(hasColor);
    assert(hasWidth);

    break;
  }
  case TagLineMod: {
    assert_cmpnum(elemstyle->type & (ES_TYPE_LINE | ES_TYPE_LINE_MOD), 0);
    elemstyle->type |= ES_TYPE_LINE_MOD;

    elemstyle_line_mod_t &line_mod = elemstyle->line_mod;

    for(unsigned int i = 0; attrs[i] != nullptr; i += 2) {
      if(strcmp(attrs[i], "colour") == 0) {
        parse_color(attrs[i + 1], line_mod.color, colors);
      } else if(strcmp(attrs[i], "width") == 0)
        parse_width_mod(attrs[i + 1], line_mod.line);
      else if(strcmp(attrs[i], "width_bg") == 0)
        parse_width_mod(attrs[i + 1], line_mod.bg);
    }
    break;
  }
  case TagArea: {
    assert_cmpnum(elemstyle->type & ES_TYPE_AREA, 0);
    elemstyle->type |= ES_TYPE_AREA;

    bool hasColor = false;
    for(unsigned int i = 0; attrs[i] != nullptr && !hasColor; i += 2) {
      if(likely(strcmp(attrs[i], "colour") == 0))
        hasColor = parse_color(attrs[i + 1], elemstyle->area.color, colors);
    }

    /* this has to be present */
    assert(hasColor);
    break;
  }
  case TagIcon:
    for(unsigned int i = 0; attrs[i] != nullptr; i += 2) {
      if(strcmp(attrs[i], "annotate") == 0)
        elemstyle->icon.annotate = (strcmp(attrs[i + 1], "true") != 0);
      else if(strcmp(attrs[i], "src") == 0)
        elemstyle->icon.filename = josm_icon_name_adjust(attrs[i + 1]);
      else if(likely(strcmp(attrs[i], "priority") == 0))
        elemstyle->icon.priority = parse_priority(attrs[i + 1]);
    }

    assert(!elemstyle->icon.filename.empty());

    break;
  default:
    break;
  }
}

void StyleSax::endElement(const char *name)
{
  StateMap::const_iterator it = std::find_if(tags.begin(), tags.end(), tag_find(name));

  assert(it != tags.end());
  assert(state == it->newState);

  if(unlikely(state == TagRule && styles.back()->conditions.empty())) {
    printf("Rule %zu has no conditions\n", styles.size());
    delete styles.back();
    styles.pop_back();
  }

  state = it->oldState;
}

} // namespace

bool josm_elemstyle::load_elemstyles(const char *fname)
{
  printf("Loading JOSM elemstyles %s ...\n", fname);

  const std::string &filename = find_file(fname);
  if(unlikely(filename.empty())) {
    printf("elemstyle file not found\n");
    return false;
  }

  StyleSax sx;
  if(unlikely(!sx.parse(filename))) {
    fprintf(stderr, "error parsing elemstyles\n");
    return false;
  } else {
    elemstyles.swap(sx.styles);
    return true;
  }
}

/* ----------------------- cleaning up --------------------- */

josm_elemstyle::~josm_elemstyle()
{
  std::for_each(elemstyles.begin(), elemstyles.end(), std::default_delete<elemstyle_t>());
}

#define WIDTH_SCALE (1)

bool elemstyle_condition_t::matches(const base_object_t &obj) const {
  if(key != nullptr) {
    const char *v = obj.tags.get_value(key);
    if(std::holds_alternative<bool>(value)) {
      if(v != nullptr) {
         const std::array<const char *, 3> &value_strings = std::get<bool>(value) ? true_values : false_values;
         return parse_boolean(v, value_strings);
      } else {
        return false;
      }
    } else {
      if(v == nullptr)
        return false;
      // The "v != val" term is a shortcut: when the case matches exact, which is the
      // usual case, the pointers should be the same, as both come from the value cache.
      // This compare is faster than the later term and helps avoiding the string compare
      // often enough. If it fails it's just a single compare of 2 values already in the
      // CPU registers, so it wont hurt much anyway.
      const char *val = std::get<const char *>(value);
      if(val != nullptr) {
        if ((v != val && strcasecmp(v, val) != 0) != invert)
          return false;
      }
    }
  }
  return true;
}

namespace {

struct condition_not_matches_obj {
  const base_object_t &obj;
  explicit condition_not_matches_obj(const base_object_t *o) : obj(*o) {}
  bool operator()(const elemstyle_condition_t &cond) {
    return !cond.matches(obj);
  }
};

void
node_icon_unref(const style_t *style, const node_t *node, icon_t &icons)
{
  style_t::IconCache::iterator it = style->node_icons.find(node->id);
  if(it != style->node_icons.end()) {
    icons.icon_free(it->second);
    style->node_icons.erase(it);
  }
}

struct colorize_node {
  const style_t * const style;
  node_t * const node;
  icon_t &icons;
  bool &somematch;
  int priority;
  colorize_node(const style_t *s, node_t *n, bool &m, icon_t &i)
    : style(s), node(n), icons(i), somematch(m)
    , priority(std::numeric_limits<typeof(priority)>::min()) {}
  void operator()(const elemstyle_t *elemstyle);
};

void colorize_node::operator()(const elemstyle_t *elemstyle)
{
  if(elemstyle->icon.filename.empty())
    return;

  if(priority >= elemstyle->icon.priority)
    return;

  // if any condition mismatches->rule mismatches
  const std::vector<elemstyle_condition_t>::const_iterator itEnd = elemstyle->conditions.end();
  if(std::any_of(elemstyle->conditions.begin(), itEnd, condition_not_matches_obj(node)))
    return;

  somematch = true;

  assert(!style->icon.path_prefix.empty());
  std::string name = "styles/";
  // the final size is now known, avoid too big allocations
  name.reserve(name.size() + style->icon.path_prefix.size() + 1 + elemstyle->icon.filename.size());
  name += style->icon.path_prefix;
  name += '/';
  name += elemstyle->icon.filename;

  icon_item *buf = icons.load(name);

  /* Free old icon if there's one present, but only after loading (not
   * assigning!) the new one. In case the old and new icon are the same
   * this ensures it still is in the icon cache if this is the only user,
   * avoiding needless image processing. */
  node_icon_unref(style, node, icons);

  if(buf != nullptr)
    style->node_icons[node->id] = buf;

  if (elemstyle->zoom_max > 0)
    node->zoom_max = elemstyle->zoom_max;

  priority = elemstyle->icon.priority;
}

} // namespace

void
josm_elemstyle::colorize(node_t *n) const
{
  n->zoom_max = node.zoom_max;

  bool somematch = false;
  icon_t &icons = icon_t::instance();
  if(icon.enable) {
    colorize_node fc(this, n, somematch, icons);
    std::for_each(elemstyles.begin(), elemstyles.end(), fc);
  }

  /* clear icon for node if not matched at least one rule and has an icon attached */
  if(!somematch)
    node_icon_unref(this, n, icons);
}

namespace {

unsigned int
line_mod_apply_width(unsigned int width, const elemstyle_width_mod_t *mod)
{
  switch(mod->mod) {
  case ES_MOD_NONE:
  default:
    return width;

  case ES_MOD_ADD:
    return width + mod->width;

  case ES_MOD_SUB:
    if (unlikely(mod->width >= width))
      return 1;
    else
      return width - mod->width;

  case ES_MOD_PERCENT:
    return 100 * width / mod->width;
  }
}

struct apply_condition {
  const style_t * const style;
  way_t * const way;
  /* during the elemstyle search a line_mod may be found. save it here */
  const elemstyle_line_mod_t **line_mod;
  int priority;
  bool way_is_closed;
  apply_condition(const style_t *s, way_t *w, const elemstyle_line_mod_t **l)
    : style(s), way(w), line_mod(l)
    , priority(std::numeric_limits<typeof(priority)>::min())
    , way_is_closed(way->is_closed()) {}
  void operator()(const elemstyle_t *elemstyle);
};

void apply_condition::operator()(const elemstyle_t* elemstyle)
{
  /* this entry does not contain line or area descriptions and is */
  /* likely just an icon. ignore this as it doesn't make much sense */
  /* for a way */
  if(elemstyle->type == ES_TYPE_NONE)
    return;

  if(std::any_of(elemstyle->conditions.begin(), elemstyle->conditions.end(),
                 condition_not_matches_obj(way)))
    return;

  if(elemstyle->type & ES_TYPE_LINE_MOD) {
    /* just save the fact that a line mod was found for later */
    *line_mod = &elemstyle->line_mod;
  }

  if(!way_is_closed && elemstyle->type & ES_TYPE_LINE) {
    if(priority >= elemstyle->line->priority)
      return;
    priority = elemstyle->line->priority;

    way->draw.color = elemstyle->line->color;
    way->draw.width =  WIDTH_SCALE * elemstyle->line->width;
    if(elemstyle->line->bg.valid) {
      way->draw.flags |= OSM_DRAW_FLAG_BG;
      way->draw.bg.color = elemstyle->line->bg.color;
      way->draw.bg.width =  WIDTH_SCALE * elemstyle->line->bg.width;
    }
    if (elemstyle->zoom_max > 0)
      way->zoom_max = elemstyle->zoom_max;
    else
      way->zoom_max = style->way.zoom_max;

    way->draw.dash_length_on = elemstyle->line->dash_length_on;
    way->draw.dash_length_off = elemstyle->line->dash_length_off;
  } else if(way_is_closed && elemstyle->type & ES_TYPE_AREA) {
    // something has already matched
    if (priority > 0)
      return;
    priority = 1;

    way->draw.flags |= OSM_DRAW_FLAG_AREA;
    /* comment the following line for grey border around all areas */
    /* (potlatch style) */

    if(style->area.has_border_color)
      way->draw.color = style->area.border_color;
    else
      way->draw.color = elemstyle->area.color;

    way->draw.width =  WIDTH_SCALE * style->area.border_width;
    /* apply area alpha */
    way->draw.area.color = elemstyle->area.color.combine_alpha(style->area.color);
    if (elemstyle->zoom_max > 0)
      way->zoom_max = elemstyle->zoom_max;
    else
      way->zoom_max = style->area.zoom_max;
  }
}

} // namespace

void josm_elemstyle::colorize(way_t *w) const
{
  /* use dark grey/no stroke/not filled for everything unknown */
  w->draw.color = way.color;
  w->draw.width = way.width;
  w->draw.flags = 0;
  w->zoom_max = 0;   // draw at all zoom levels

  /* during the elemstyle search a line_mod may be found. save it here */
  const elemstyle_line_mod_t *line_mod = nullptr;
  apply_condition fc(this, w, &line_mod);

  std::for_each(elemstyles.begin(), elemstyles.end(), fc);

  // If this is an area the previous run has done the area style. Run again
  // for the line style of the outer way.
  if(fc.way_is_closed) {
    fc.priority = std::numeric_limits<typeof(fc.priority)>::min();
    fc.way_is_closed = false;
    std::for_each(elemstyles.begin(), elemstyles.end(), fc);
  }

  /* apply the last line mod entry that has been found during search */
  if(line_mod != nullptr) {
    w->draw.width = line_mod_apply_width(w->draw.width, &line_mod->line);

    /* special case: the way does not have a background, but it is to */
    /* be modified */
    if(line_mod->bg.mod != ES_MOD_NONE && !(w->draw.flags & OSM_DRAW_FLAG_BG)) {
      /* add a background in black color */
      w->draw.flags |= OSM_DRAW_FLAG_BG;
      w->draw.bg.color = color_t::black();
      w->draw.bg.width =  w->draw.width;
    }

    w->draw.bg.width = line_mod_apply_width(w->draw.bg.width, &line_mod->bg);
    if(!line_mod->color.is_transparent())
      w->draw.color = line_mod->color;
  }
}
