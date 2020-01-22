#include <josm_presets.h>
#include <josm_presets_p.h>

#include <fdguard.h>
#include <osm.h>
#include <osm_objects.h>

#include <algorithm>
#include <cassert>
#include <iostream>
#include <libxml/parser.h>

#include <osm2go_annotations.h>
#include <osm2go_cpp.h>

int main(void)
{
  xmlInitParser();

  std::unique_ptr<presets_items_internal> presets(static_cast<presets_items_internal *>(presets_items::load()));

  if(!presets) {
    std::cerr << "failed to load presets" << std::endl;
    return 1;
  }

  if(presets->items.empty()) {
    std::cerr << "no items found" << std::endl;
    return 1;
  }

  assert_cmpnum_op(presets->items.size(), >, 0);

  const presets_item_t *p = presets->items.back();
  assert(!p->isItem());
  const presets_item_group *gr = dynamic_cast<const presets_item_group *>(p);
  assert(gr != nullptr);
  assert_cmpstr(gr->name, "OSM2go XY");

  assert_cmpnum(p->type, presets_item_t::TY_GROUP | presets_item_t::TY_MULTIPOLYGON);

  assert_cmpnum(gr->items.size(), 2);
  p = gr->items.front();
  assert(p->isItem());
  assert_cmpnum(p->type, presets_item_t::TY_MULTIPOLYGON);
  assert_cmpstr(gr->icon, std::string());

  const presets_item *item = dynamic_cast<const presets_item *>(p);
  assert(item != nullptr);
  assert_cmpnum(item->roles.size(), 0);
  assert_cmpnum(item->widgets.size(), 3);
  assert_cmpstr(item->link, std::string());
  assert(!item->addEditName);

  assert_cmpnum(item->widgets.front()->type, WIDGET_TYPE_KEY);
  const presets_element_key *el_key = dynamic_cast<const presets_element_key *>(item->widgets.front());
  assert(el_key != nullptr);
  assert_cmpstr(el_key->key, "OSM2go test");
  assert_cmpstr(el_key->value, "passed");

  assert_cmpnum(item->widgets.at(1)->type, WIDGET_TYPE_LABEL);
  const presets_element_label *el_lb = dynamic_cast<const presets_element_label *>(item->widgets.at(1));
  assert(el_lb != nullptr);
  assert_cmpstr(el_lb->text, "xy label");

  assert_cmpnum(item->widgets.back()->type, WIDGET_TYPE_CHECK);
  const presets_element_checkbox *el_chk = dynamic_cast<const presets_element_checkbox *>(item->widgets.back());
  assert(el_chk != nullptr);
  assert_cmpstr(el_chk->text, "xy Chk");

  p = gr->items.at(1);
  assert(!p->isItem());
  assert_cmpnum(p->type, presets_item_t::TY_SEPARATOR);

  xmlCleanupParser();

  return 0;
}

#include "appdata_dummy.h"
