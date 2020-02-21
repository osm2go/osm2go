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
#include <osm2go_test.h>

int main(int argc, char **argv)
{
  OSM2GO_TEST_INIT(argc, argv);

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

  assert_cmpnum(gr->items.size(), 3);
  p = gr->items.front();
  assert(p->isItem());
  assert_cmpnum(p->type, presets_item_t::TY_MULTIPOLYGON);
  assert_cmpstr(gr->icon, std::string());

  const presets_item *item = dynamic_cast<const presets_item *>(p);
  assert(item != nullptr);
  assert_cmpnum(item->roles.size(), 0);
  assert_cmpnum(item->widgets.size(), 6);
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

  assert_cmpnum(item->widgets.at(2)->type, WIDGET_TYPE_CHECK);
  const presets_element_checkbox *el_chk = dynamic_cast<const presets_element_checkbox *>(item->widgets.at(2));
  assert(el_chk != nullptr);
  assert_cmpstr(el_chk->text, "xy Chk");

  assert_cmpnum(item->widgets.at(3)->type, WIDGET_TYPE_MULTISELECT);
  const presets_element_multiselect *el_ms = dynamic_cast<const presets_element_multiselect *>(item->widgets.at(3));
  assert(el_ms != nullptr);
  assert_cmpnum(el_ms->rows_height, 2);
  assert_cmpnum(el_ms->values.size(), 4);
  assert_cmpnum(el_ms->display_values.size(), 0);

  assert_cmpnum(item->widgets.at(4)->type, WIDGET_TYPE_COMBO);
  const presets_element_combo *el_cmb = dynamic_cast<const presets_element_combo *>(item->widgets.at(4));
  assert(el_cmb != nullptr);
  assert_cmpstr(el_cmb->text, "combo");
  assert_cmpnum(el_cmb->values.size(), 2);
  assert_cmpnum(el_cmb->display_values.size(), 2);
  assert_cmpstr(el_cmb->values.front(), "cval");
  assert_cmpstr(el_cmb->values.back(), "cval2");
  assert_cmpstr(el_cmb->display_values.front(), "cval");
  assert_cmpstr(el_cmb->display_values.back(), "second cval");

  assert_cmpnum(item->widgets.at(5)->type, WIDGET_TYPE_LINK);
  const presets_element_link *el_lnk = dynamic_cast<const presets_element_link *>(item->widgets.at(5));
  assert(el_lnk != nullptr);
  assert(el_lnk->item == gr->items.back());

  p = gr->items.at(1);
  assert(!p->isItem());
  assert_cmpnum(p->type, presets_item_t::TY_SEPARATOR);

  p = gr->items.at(2);
  assert(p->isItem());
  assert_cmpnum(p->type, presets_item_t::TY_NONE);
  assert_cmpstr(gr->icon, std::string());

  xmlCleanupParser();

  return 0;
}

#include "dummy_appdata.h"
