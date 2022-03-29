#include "dummy_map.h"

#include <map.h>

#include <appdata.h>
#include <gps_state.h>
#include <iconbar.h>
#include <josm_presets.h>
#include <osm.h>
#include <osm2go_annotations.h>
#include <style.h>
#include <uicontrol.h>
#include <osm2go_test.h>

#include <iostream>
#include <memory>
#include <unistd.h>

void test_map::test_function()
{
  way_add_begin();
  way_add_cancel();
}

namespace {

// set up flags for a call to expect a call to map_t::item_deselect()
void expectMapItemDeselect(MainUiDummy *ui)
{
  ui->clearFlags.push_back(MainUi::ClearNormal);
  ui->m_actions.insert(std::make_pair(MainUi::MENU_ITEM_MAP_HIDE_SEL, false));
}

void set_bounds(osm_t::ref o)
{
  bool b = o->bounds.init(pos_area(pos_t(52.2692786, 9.5750497), pos_t(52.2695463, 9.5755)));
  o->bounds.min.x = 0;
  o->bounds.min.y = 0;
  o->bounds.max.x = 64;
  o->bounds.max.y = 40;
  assert(b);
}

void test_map_delete()
{
  appdata_t a;
  std::unique_ptr<map_t> m(std::make_unique<test_map>(a));
}

void test_map_delete_items()
{
  appdata_t a;
  std::unique_ptr<map_t> m(std::make_unique<test_map>(a));
  std::unique_ptr<osm_t> o(std::make_unique<osm_t>());
  set_bounds(o);

  way_t *w = o->attach(new way_t());

  // keep it here, it ill only be reset, but not freed as that is done through the map
  std::unique_ptr<map_item_t> mi(new map_item_t(object_t(w), nullptr));
  w->map_item = mi.get();

  o->way_delete(w, m.get());

  lpos_t p(10, 10);
  node_t *n = o->node_new(p);
  o->attach(n);
  n->map_item = mi.get();

  o->node_delete(n);
}

void test_draw_deleted(const std::string &tmpdir)
{
  appdata_t a;
  a.project.reset(new project_t("foo", tmpdir));
  std::unique_ptr<map_t> m(std::make_unique<test_map>(a, nullptr, test_map::InvalidStyle));
  a.project->osm.reset(new osm_t());
  osm_t::ref o = a.project->osm;
  set_bounds(o);

  lpos_t p(10, 10);
  base_attributes ba(123);
  ba.version = 1;
  node_t *n = o->node_new(p.toPos(o->bounds), ba);
  o->insert(n);
  assert(!n->isDeleted());
  assert_cmpnum(n->flags, 0);
  o->node_delete(n);
  assert(n->isDeleted());

  // deleted nodes are not drawn
  m->draw(n);

  way_t *w = new way_t(ba);
  o->insert(w);
  assert(!w->isDeleted());
  assert_cmpnum(w->flags, 0);
  o->way_delete(w, m.get());
  assert(w->isDeleted());

  // deleted ways are not drawn
  m->draw(w);
}

void test_draw_hidden(const std::string &tmpdir)
{
  appdata_t a;
  a.project.reset(new project_t("foo", tmpdir));
  std::unique_ptr<map_t> m(std::make_unique<test_map>(a, nullptr, test_map::EmptyStyle));
  a.project->osm.reset(new osm_t());
  osm_t::ref o = a.project->osm;
  set_bounds(o);
  MainUiDummy * const ui = static_cast<MainUiDummy *>(a.uicontrol.get());

  base_attributes ba(123);
  ba.version = 1;
  way_t *w = new way_t(ba);
  o->insert(w);
  assert(!w->isDeleted());
  assert_cmpnum(w->flags, 0);

  ba.id = 4321;
  way_t *delW = new way_t(ba);
  o->insert(delW);
  assert(!delW->isDeleted());
  assert_cmpnum(delW->flags, 0);
  o->way_delete(delW, m.get());

  for (int i = 0; i < 4; i++) {
    lpos_t p(10, 10 + i);
    node_t *n = o->node_new(p);
    o->attach(n);
    assert(!n->isDeleted());
    assert_cmpnum(n->flags, OSM_FLAG_DIRTY);
    w->append_node(n);
    delW->append_node(n);
  }

  o->waySetHidden(w);
  assert(o->wayIsHidden(w));

  // hidden ways are not drawn
  m->draw(w);

  // trick the way to become unhidden bit still not drawn: also set deleted marker
  w->flags |= OSM_FLAG_DELETED;

  ui->m_actions.insert(std::make_pair(MainUi::MENU_ITEM_MAP_SHOW_ALL, false));
  m->show_all();

  assert_cmpnum(o->hiddenWays.size(), 0);
  w->flags = 0;

  // delete a node from a hidden way: this should trigger a redraw, but again it's not actually drawn
  o->waySetHidden(w);
  o->node_delete(w->node_chain.front(), m.get());
}

void test_way_add_cancel(const std::string &tmpdir)
{
  appdata_t a;
  std::unique_ptr<test_map> m(std::make_unique<test_map>(a));

  a.project.reset(new project_t("foo", tmpdir));
  a.project->osm.reset(new osm_t());
  set_bounds(a.project->osm);

  m->test_function();
}

void test_map_item_deleter(const std::string &tmpdir)
{
  appdata_t a;
  a.project.reset(new project_t("foo", tmpdir));
  std::unique_ptr<map_t> m(std::make_unique<test_map>(a, nullptr, test_map::InvalidStyle));
  a.project->osm.reset(new osm_t());
  osm_t::ref o = a.project->osm;
  set_bounds(o);

  way_t * const w = new way_t();
  o->attach(w);
  w->map_item = new map_item_t(object_t(w));

  map_item_destroyer mid(w->map_item);

  w->item_chain_destroy(m.get());

  assert_null(w->map_item);
  mid.run(nullptr);
}

void test_map_deselect(const std::string &tmpdir)
{
  appdata_t a;
  a.project.reset(new project_t("foo", tmpdir));
  std::unique_ptr<map_t> m(std::make_unique<test_map>(a, nullptr, test_map::InvalidStyle));
  a.project->osm.reset(new osm_t());
  osm_t::ref o = a.project->osm;
  set_bounds(o);
  iconbar_t::create(a);

  MainUiDummy * const ui = static_cast<MainUiDummy *>(a.uicontrol.get());
  expectMapItemDeselect(ui);

  m->item_deselect();
  assert_cmpnum(ui->m_actions.size(), 0);
  assert(!a.iconbar->isCancelEnabled());
  assert(!a.iconbar->isOkEnabled());
  assert(!a.iconbar->isInfoEnabled());
  assert(!a.iconbar->isTrashEnabled());
}

// start adding a way and cancel immediately through the action interface
void test_way_add_cancel_map(const std::string &tmpdir)
{
  appdata_t a;
  a.project.reset(new project_t("foo", tmpdir));
  std::unique_ptr<map_t> m(std::make_unique<test_map>(a, nullptr, test_map::InvalidStyle));
  a.project->osm.reset(new osm_t());
  osm_t::ref o = a.project->osm;
  set_bounds(o);
  iconbar_t::create(a);

  MainUiDummy * const ui = static_cast<MainUiDummy *>(a.uicontrol.get());
  expectMapItemDeselect(ui);
  ui->m_actions.insert(std::make_pair(MainUi::MENU_ITEM_WMS_ADJUST, false));
  ui->m_statusTexts.push_back(trstring("Place first node of new way"));

  m->set_action(MAP_ACTION_WAY_ADD);
  assert(a.iconbar->isCancelEnabled());
  assert(!a.iconbar->isOkEnabled());
  assert(!a.iconbar->isInfoEnabled());
  assert(!a.iconbar->isTrashEnabled());
  assert_cmpnum(ui->m_actions.size(), 0);
  assert_cmpnum(ui->m_statusTexts.size(), 0);

  // way add has started, prepare for cancel

  ui->clearFlags.push_back(MainUi::ClearNormal);
  ui->m_actions.insert(std::make_pair(MainUi::MENU_ITEM_WMS_ADJUST, true));
  map_t::map_action_cancel(m.get());
  assert(!a.iconbar->isCancelEnabled());
  assert(!a.iconbar->isOkEnabled());
  assert(!a.iconbar->isInfoEnabled());
  assert(!a.iconbar->isTrashEnabled());
}

void test_node_add_cancel_map(const std::string &tmpdir)
{
  appdata_t a;
  a.project.reset(new project_t("foo", tmpdir));
  std::unique_ptr<map_t> m(std::make_unique<test_map>(a, nullptr, test_map::InvalidStyle));
  a.project->osm.reset(new osm_t());
  osm_t::ref o = a.project->osm;
  set_bounds(o);
  iconbar_t::create(a);

  MainUiDummy * const ui = static_cast<MainUiDummy *>(a.uicontrol.get());
  expectMapItemDeselect(ui);
  ui->m_actions.insert(std::make_pair(MainUi::MENU_ITEM_WMS_ADJUST, false));
  ui->m_statusTexts.push_back(trstring("Place a node"));

  m->set_action(MAP_ACTION_NODE_ADD);
  assert(a.iconbar->isCancelEnabled());
  assert(a.iconbar->isOkEnabled());
  assert(!a.iconbar->isInfoEnabled());
  assert(!a.iconbar->isTrashEnabled());
  assert_cmpnum(ui->m_actions.size(), 0);
  assert_cmpnum(ui->m_statusTexts.size(), 0);

  // node add has started, prepare for cancel

  ui->clearFlags.push_back(MainUi::ClearNormal);
  ui->m_actions.insert(std::make_pair(MainUi::MENU_ITEM_WMS_ADJUST, true));
  map_t::map_action_cancel(m.get());
  assert(!a.iconbar->isCancelEnabled());
  assert(!a.iconbar->isOkEnabled());
  assert(!a.iconbar->isInfoEnabled());
  assert(!a.iconbar->isTrashEnabled());
}

void test_node_add_ok_map(const std::string &tmpdir)
{
  appdata_t a;
  a.project.reset(new project_t("foo", tmpdir));
  std::unique_ptr<map_t> m(std::make_unique<test_map>(a, nullptr, test_map::InvalidStyle));
  a.project->osm.reset(new osm_t());
  osm_t::ref o = a.project->osm;
  set_bounds(o);
  iconbar_t::create(a);

  MainUiDummy * const ui = static_cast<MainUiDummy *>(a.uicontrol.get());
  expectMapItemDeselect(ui);
  ui->m_actions.insert(std::make_pair(MainUi::MENU_ITEM_WMS_ADJUST, false));
  ui->m_statusTexts.push_back(trstring("Place a node"));

  m->set_action(MAP_ACTION_NODE_ADD);
  assert(a.iconbar->isCancelEnabled());
  assert(a.iconbar->isOkEnabled());
  assert(!a.iconbar->isInfoEnabled());
  assert(!a.iconbar->isTrashEnabled());
  assert_cmpnum(ui->m_actions.size(), 0);
  assert_cmpnum(ui->m_statusTexts.size(), 0);

  // node add has started, trigger "ok". This would add the node if there is a valid GPS position

  ui->clearFlags.push_back(MainUi::ClearNormal);
  ui->m_actions.insert(std::make_pair(MainUi::MENU_ITEM_WMS_ADJUST, true));
  m->action_ok();
  assert(!a.iconbar->isCancelEnabled());
  assert(!a.iconbar->isOkEnabled());
  assert(!a.iconbar->isInfoEnabled());
  assert(!a.iconbar->isTrashEnabled());
}

void test_map_detail(const std::string &tmpdir)
{
  appdata_t a;
  a.project.reset(new project_t("foo", tmpdir));
  canvas_holder canvas;
  std::unique_ptr<map_t> m(std::make_unique<test_map>(a, *canvas, test_map::InvalidStyle));
  a.project->osm.reset(new osm_t());
  osm_t::ref o = a.project->osm;
  set_bounds(o);
  iconbar_t::create(a);

  MainUiDummy * const ui = static_cast<MainUiDummy *>(a.uicontrol.get());
  expectMapItemDeselect(ui);
  expectMapItemDeselect(ui); // called twice from different places
  ui->clearFlags.push_back(MainUi::Busy);
  ui->m_statusTexts.push_back(trstring("Increasing detail level"));

  m->detail_increase();

  assert(!a.iconbar->isCancelEnabled());
  assert(!a.iconbar->isOkEnabled());
  assert(!a.iconbar->isInfoEnabled());
  assert(!a.iconbar->isTrashEnabled());
  assert_cmpnum(ui->m_actions.size(), 0);
  assert_cmpnum(ui->m_statusTexts.size(), 0);

  expectMapItemDeselect(ui);
  expectMapItemDeselect(ui); // called twice from different places
  ui->clearFlags.push_back(MainUi::Busy);
  ui->m_statusTexts.push_back(trstring("Decreasing detail level"));
  m->detail_decrease();

  assert(!a.iconbar->isCancelEnabled());
  assert(!a.iconbar->isOkEnabled());
  assert(!a.iconbar->isInfoEnabled());
  assert(!a.iconbar->isTrashEnabled());
}

void test_map_item_at_empty(const std::string &tmpdir)
{
  appdata_t a;
  a.project.reset(new project_t("foo", tmpdir));
  canvas_holder canvas;
  std::unique_ptr<test_map> m(std::make_unique<test_map>(a, *canvas, test_map::InvalidStyle));
  a.project->osm.reset(new osm_t());
  osm_t::ref o = a.project->osm;
  set_bounds(o);

  // there is nothing on the map
  assert_null(m->item_at(lpos_t(42, 42)));

  m->pen_down_item_public(nullptr);
}

// click while idle
void test_map_press_idle(const std::string &tmpdir)
{
  appdata_t a;
  a.project.reset(new project_t("foo", tmpdir));
  canvas_holder canvas;
  std::unique_ptr<test_map> m(std::make_unique<test_map>(a, *canvas, test_map::InvalidStyle));
  a.project->osm.reset(new osm_t());
  osm_t::ref o = a.project->osm;
  set_bounds(o);
  iconbar_t::create(a);

  MainUiDummy * const ui = static_cast<MainUiDummy *>(a.uicontrol.get());

  osm2go_platform::screenpos pos(1, 1);

  m->button_press_public(pos);
  assert_cmpnum(ui->m_actions.size(), 0);
  assert(!a.iconbar->isCancelEnabled());
  assert(!a.iconbar->isOkEnabled());
  assert(!a.iconbar->isInfoEnabled());
  assert(!a.iconbar->isTrashEnabled());

  expectMapItemDeselect(ui);
  m->button_release_public(pos);
}

// drag while idle
void test_map_drag_idle(const std::string &tmpdir)
{
  appdata_t a;
  a.project.reset(new project_t("foo", tmpdir));
  canvas_holder canvas;
  std::unique_ptr<test_map> m(std::make_unique<test_map>(a, *canvas, test_map::InvalidStyle));
  a.project->osm.reset(new osm_t());
  osm_t::ref o = a.project->osm;
  set_bounds(o);
  iconbar_t::create(a);

  MainUiDummy * const ui = static_cast<MainUiDummy *>(a.uicontrol.get());

  m->button_press_public(osm2go_platform::screenpos(1, 1));
  assert_cmpnum(ui->m_actions.size(), 0);
  assert(!a.iconbar->isCancelEnabled());
  assert(!a.iconbar->isOkEnabled());
  assert(!a.iconbar->isInfoEnabled());
  assert(!a.iconbar->isTrashEnabled());

  m->handle_motion_public(osm2go_platform::screenpos(2, 2));
  assert(!m->pen_down.drag);

  m->handle_motion_public(osm2go_platform::screenpos(2, 3));
  assert(!m->pen_down.drag);

  m->handle_motion_public(osm2go_platform::screenpos(4, 4));
  assert(m->pen_down.drag);

  m->button_release_public(osm2go_platform::screenpos(4, 4));
}

// like test_way_add_cancel_map, but add 2 nodes before cancel
void test_map_press_way_add_cancel(const std::string &tmpdir)
{
  appdata_t a;
  a.project.reset(new project_t("foo", tmpdir));
  canvas_holder canvas;
  std::unique_ptr<test_map> m(std::make_unique<test_map>(a, *canvas, test_map::EmptyStyle));
  a.project->osm.reset(new osm_t());
  osm_t::ref o = a.project->osm;
  set_bounds(o);
  iconbar_t::create(a);

  MainUiDummy * const ui = static_cast<MainUiDummy *>(a.uicontrol.get());
  expectMapItemDeselect(ui);
  ui->m_actions.insert(std::make_pair(MainUi::MENU_ITEM_WMS_ADJUST, false));
  ui->m_statusTexts.push_back(trstring("Place first node of new way"));

  assert_null(m->action_way());
  assert_null(m->action_way_extending());
  assert_null(m->action_way_ends_on());
  m->set_action(MAP_ACTION_WAY_ADD);
  assert(a.iconbar->isCancelEnabled());
  assert(!a.iconbar->isOkEnabled());
  assert(!a.iconbar->isInfoEnabled());
  assert(!a.iconbar->isTrashEnabled());
  assert_cmpnum(ui->m_actions.size(), 0);
  assert_cmpnum(ui->m_statusTexts.size(), 0);

  assert(m->action_way() != nullptr);
  assert_cmpnum(m->action_way()->node_chain.size(), 0);
  assert_null(m->action_way_extending());
  assert_null(m->action_way_ends_on());

  // click somewhere outside of the project, this must not add something to the temporary way
  osm2go_platform::screenpos posOutside(100, 1);
  m->button_press_public(posOutside);
  m->button_release_public(posOutside);
  assert_cmpnum(m->action_way()->node_chain.size(), 0);

  // "click" at a good position to add a node
  ui->m_statusTexts.push_back(trstring("Place next node of way"));
  osm2go_platform::screenpos posFirst(1, 1);
  m->button_press_public(posFirst);
  m->button_release_public(posFirst);
  assert_cmpnum(m->action_way()->node_chain.size(), 1);

  // with the given zoom this is "too close" so it should be ignored as double click
  assert_cmpnum(a.project->map_state.zoom, 0.25);
  osm2go_platform::screenpos posSecond(8, 8);
  m->button_press_public(posSecond);
  m->button_release_public(posSecond);
  assert_cmpnum(m->action_way()->node_chain.size(), 1);

  // now click another good position far enough away
  osm2go_platform::screenpos posThird(42, 27);
  m->button_press_public(posThird);
  m->button_release_public(posThird);
  assert_cmpnum(m->action_way()->node_chain.size(), 2);

  // way add has started, prepare for cancel

  ui->clearFlags.push_back(MainUi::ClearNormal);
  ui->m_actions.insert(std::make_pair(MainUi::MENU_ITEM_WMS_ADJUST, true));
  map_t::map_action_cancel(m.get());
  assert(!a.iconbar->isCancelEnabled());
  assert(!a.iconbar->isOkEnabled());
  assert(!a.iconbar->isInfoEnabled());
  assert(!a.iconbar->isTrashEnabled());
  assert_null(m->action_way());
  assert_null(m->action_way_extending());
  assert_null(m->action_way_ends_on());
}

// like test_map_press_way_add_cancel, but reuse an existing node
void test_map_press_way_reuse_add_cancel(const std::string &tmpdir)
{
  appdata_t a;
  a.project.reset(new project_t("foo", tmpdir));
  canvas_holder canvas;
  std::unique_ptr<test_map> m(std::make_unique<test_map>(a, *canvas, test_map::NodeStyle));
  a.project->osm.reset(new osm_t());
  osm_t::ref o = a.project->osm;
  set_bounds(o);
  iconbar_t::create(a);

  MainUiDummy * const ui = static_cast<MainUiDummy *>(a.uicontrol.get());
  expectMapItemDeselect(ui);
  ui->m_actions.insert(std::make_pair(MainUi::MENU_ITEM_WMS_ADJUST, false));
  ui->m_statusTexts.push_back(trstring("Place first node of new way"));

  // where the click will be
  osm2go_platform::screenpos clickpos(10, 10);
  lpos_t pos = canvas->window2world(clickpos);
  // a node to find later on
  node_t *node = o->node_new(pos);
  o->attach(node);

  assert_null(m->action_way());
  assert_null(m->action_way_extending());
  assert_null(m->action_way_ends_on());
  m->set_action(MAP_ACTION_WAY_ADD);
  assert(a.iconbar->isCancelEnabled());
  assert(!a.iconbar->isOkEnabled());
  assert(!a.iconbar->isInfoEnabled());
  assert(!a.iconbar->isTrashEnabled());
  assert_cmpnum(ui->m_actions.size(), 0);
  assert_cmpnum(ui->m_statusTexts.size(), 0);

  assert(m->action_way() != nullptr);
  assert_cmpnum(m->action_way()->node_chain.size(), 0);
  assert_null(m->action_way_extending());
  assert_null(m->action_way_ends_on());

  // "click" at a good position to add a node
  ui->m_statusTexts.push_back(trstring("Place next node of way"));
  assert(!m->touchnode);
  m->button_press_public(clickpos);
  assert(m->touchnode);
  assert(m->touchnode_node == node);
  m->button_release_public(clickpos);
  assert_cmpnum(m->action_way()->node_chain.size(), 1);
  assert(m->action_way()->node_chain.front() == node);

  // now click another good position far enough away
  osm2go_platform::screenpos posThird(42, 27);
  m->button_press_public(posThird);
  m->button_release_public(posThird);
  assert_cmpnum(m->action_way()->node_chain.size(), 2);
  assert(m->action_way()->node_chain.front() == node);
  assert(m->action_way()->node_chain.back() != node);

  // way add has started, prepare for cancel

  ui->clearFlags.push_back(MainUi::ClearNormal);
  ui->m_actions.insert(std::make_pair(MainUi::MENU_ITEM_WMS_ADJUST, true));
  map_t::map_action_cancel(m.get());
  assert(!a.iconbar->isCancelEnabled());
  assert(!a.iconbar->isOkEnabled());
  assert(!a.iconbar->isInfoEnabled());
  assert(!a.iconbar->isTrashEnabled());
  assert_null(m->action_way());
  assert_null(m->action_way_extending());
  assert_null(m->action_way_ends_on());

  // the node must not have been removed
  assert_cmpnum(o->nodes.size(), 1);
}

// like test_map_press_way_add_cancel, but reuse an existing node
// afterwards another node is added
void test_map_press_way_intermediate_reuse_add_ok(const std::string &tmpdir)
{
  appdata_t a;
  a.project.reset(new project_t("foo", tmpdir));
  canvas_holder canvas;
  std::unique_ptr<test_map> m(std::make_unique<test_map>(a, *canvas, test_map::NodeStyle));
  a.project->osm.reset(new osm_t());
  osm_t::ref o = a.project->osm;
  set_bounds(o);
  iconbar_t::create(a);

  MainUiDummy * const ui = static_cast<MainUiDummy *>(a.uicontrol.get());
  expectMapItemDeselect(ui);
  ui->m_actions.insert(std::make_pair(MainUi::MENU_ITEM_WMS_ADJUST, false));
  ui->m_statusTexts.push_back(trstring("Place first node of new way"));

  // where the click will be
  osm2go_platform::screenpos clickpos(20, 20);
  lpos_t pos = canvas->window2world(clickpos);
  // a node to find later on
  node_t *node = o->node_new(pos);
  o->attach(node);

  node_t *other = o->node_new(canvas->window2world(osm2go_platform::screenpos(240, 240)));
  o->attach(other);

  way_t *oldWay = new way_t();
  oldWay->append_node(node);
  oldWay->append_node(other);
  o->attach(oldWay);

  assert_null(m->action_way());
  assert_null(m->action_way_extending());
  assert_null(m->action_way_ends_on());
  m->set_action(MAP_ACTION_WAY_ADD);
  assert(a.iconbar->isCancelEnabled());
  assert(!a.iconbar->isOkEnabled());
  assert(!a.iconbar->isInfoEnabled());
  assert(!a.iconbar->isTrashEnabled());
  assert_cmpnum(ui->m_actions.size(), 0);
  assert_cmpnum(ui->m_statusTexts.size(), 0);

  assert(m->action_way() != nullptr);
  assert_cmpnum(m->action_way()->node_chain.size(), 0);
  assert_null(m->action_way_extending());
  assert_null(m->action_way_ends_on());

  // "click" at a good position to add a node
  ui->m_statusTexts.push_back(trstring("Place next node of way"));

  osm2go_platform::screenpos posFirst(1, 1);
  m->button_press_public(posFirst);
  m->button_release_public(posFirst);
  assert_cmpnum(m->action_way()->node_chain.size(), 1);

  assert(!m->touchnode);
  m->button_press_public(clickpos);
  assert(m->touchnode);
  assert(m->touchnode_node == node);
  m->button_release_public(clickpos);
  assert_cmpnum(m->action_way()->node_chain.size(), 2);
  assert(m->action_way()->node_chain.front() != node);
  assert(m->action_way()->node_chain.back() == node);
  assert(m->action_way_ends_on() == oldWay);

  // now click another good position far enough away
  osm2go_platform::screenpos posThird(42, 27);
  m->button_press_public(posThird);
  m->button_release_public(posThird);
  assert_cmpnum(m->action_way()->node_chain.size(), 3);
  assert(m->action_way()->node_chain.back() != node);
  assert_null(m->action_way_ends_on());

  // now accept everything
  assert(a.iconbar->isCancelEnabled());
  assert(a.iconbar->isOkEnabled());

  ui->clearFlags.push_back(MainUi::ClearNormal);
  ui->m_actions.insert(std::make_pair(MainUi::MENU_ITEM_WMS_ADJUST, true));

  ui->m_statusTexts.push_back(trstring("unspecified way"));
  ui->m_actions.insert(std::make_pair(MainUi::MENU_ITEM_MAP_HIDE_SEL, true));

  // add a tag here (which can't happen in reality when adding a way) to prevent showing the info dialog
  std::vector<tag_t> ntags(1, tag_t("source", "testcase"));
  m->action_way()->tags.replace(std::move(ntags));

  map_t::map_action_ok(m.get());
  assert(!a.iconbar->isCancelEnabled());
  assert(!a.iconbar->isOkEnabled());
  assert(a.iconbar->isInfoEnabled());
  assert(a.iconbar->isTrashEnabled());
  assert_null(m->action_way());
  assert_null(m->action_way_extending());
  assert_null(m->action_way_ends_on());

  // the node must not have been removed
  assert_cmpnum(o->nodes.size(), 4);
  assert_cmpnum(o->ways.size(), 2);
}

void test_map_node_create_outside(const std::string &tmpdir)
{
  appdata_t a;
  a.project.reset(new project_t("foo", tmpdir));
  canvas_holder canvas;
  std::unique_ptr<test_map> m(std::make_unique<test_map>(a, *canvas, test_map::InvalidStyle));
  a.project->osm.reset(new osm_t());
  osm_t::ref o = a.project->osm;
  set_bounds(o);
  iconbar_t::create(a);

  MainUiDummy * const ui = static_cast<MainUiDummy *>(a.uicontrol.get());
  expectMapItemDeselect(ui);
  ui->m_actions.insert(std::make_pair(MainUi::MENU_ITEM_WMS_ADJUST, false));
  ui->m_statusTexts.push_back(trstring("Place a node"));

  m->set_action(MAP_ACTION_NODE_ADD);
  assert(a.iconbar->isCancelEnabled());
  assert(a.iconbar->isOkEnabled());
  assert(!a.iconbar->isInfoEnabled());
  assert(!a.iconbar->isTrashEnabled());
  assert_cmpnum(ui->m_actions.size(), 0);
  assert_cmpnum(ui->m_statusTexts.size(), 0);

  // "click" at a good position to add a node
  m->button_press_public(osm2go_platform::screenpos(5, 5));

  // then drag it around
  m->handle_motion_public(osm2go_platform::screenpos(3, 3));
  assert(!m->pen_down.drag);

  m->handle_motion_public(osm2go_platform::screenpos(1, 1));
  // has reached the drag limit now
  assert(m->pen_down.drag);

  m->handle_motion_public(osm2go_platform::screenpos(-1, -1));
  // has reached the drag limit now
  assert(m->pen_down.drag);

  expectMapItemDeselect(ui);
  ui->m_actions.insert(std::make_pair(MainUi::MENU_ITEM_WMS_ADJUST, true));
  ui->clearFlags.push_back(MainUi::ClearNormal);
  m->button_release_public(osm2go_platform::screenpos(-1, -2));

  assert_null(m->action_way());
  assert_cmpnum(o->nodes.size(), 0);

  assert(!a.iconbar->isCancelEnabled());
  assert(!a.iconbar->isOkEnabled());
  assert(!a.iconbar->isInfoEnabled());
  assert(!a.iconbar->isTrashEnabled());
}

void test_map_reverse(const std::string &tmpdir)
{
  appdata_t a;
  a.project.reset(new project_t("foo", tmpdir));
  canvas_holder canvas;
  std::unique_ptr<test_map> m(std::make_unique<test_map>(a, *canvas, test_map::InvalidStyle));
  a.project->osm.reset(new osm_t());
  osm_t::ref o = a.project->osm;
  set_bounds(o);
  iconbar_t::create(a);

  MainUiDummy * const ui = static_cast<MainUiDummy *>(a.uicontrol.get());
  expectMapItemDeselect(ui);

  way_t *w = new way_t();
  lpos_t p(10, 10);
  node_t *n = o->node_new(p);
  o->attach(n);
  p = lpos_t(20, 20);
  w->append_node(n);
  n = o->node_new(p);
  o->attach(n);
  w->append_node(n);
  o->attach(w);

  m->selected = object_t(w);
  ui->m_statusTexts.push_back(trstring("unspecified way"));
  ui->m_actions.insert(std::make_pair(MainUi::MENU_ITEM_MAP_HIDE_SEL, true));

  m->way_reverse_public();

  assert(m->selected.object == object_t(w));
  ui->check();

  std::vector<tag_t> ntags(1, tag_t("oneway", "yes"));

  w->tags.replace(std::move(ntags));

  ui->m_statusTexts.push_back(trstring("oneway"));
  ui->m_statusTexts.push_back(trstring("1 tag updated"));
  expectMapItemDeselect(ui);
  ui->m_actions.insert(std::make_pair(MainUi::MENU_ITEM_MAP_HIDE_SEL, true));

  m->way_reverse_public();

  assert(m->selected.object == object_t(w));
  assert_cmpstr(w->tags.get_value("oneway"), "-1");
  ui->check();

  ntags.push_back(tag_t("type", "route"));
  relation_t *r = new relation_t();
  o->attach(r);
  r->members.push_back(member_t(object_t(w), "backward"));
  r->tags.replace(std::move(ntags));

  ui->m_statusTexts.push_back(trstring("oneway"));
  ui->m_statusTexts.push_back(trstring("1 tag & 1 relation updated"));
  expectMapItemDeselect(ui);
  ui->m_actions.insert(std::make_pair(MainUi::MENU_ITEM_MAP_HIDE_SEL, true));

  m->way_reverse_public();

  assert(m->selected.object == object_t(w));
  assert_cmpstr(w->tags.get_value("oneway"), "yes");
  assert_cmpstr(r->members.front().role, "forward");
  ui->check();

  w->tags.clear();

  ui->m_statusTexts.push_back(trstring("way: 'backward' in route <ID #-1>"));
  ui->m_statusTexts.push_back(trstring("1 relation updated"));
  expectMapItemDeselect(ui);
  ui->m_actions.insert(std::make_pair(MainUi::MENU_ITEM_MAP_HIDE_SEL, true));

  m->way_reverse_public();

  assert(m->selected.object == object_t(w));
  assert_cmpstr(r->members.front().role, "backward");
}

void test_select(const std::string &tmpdir)
{
  appdata_t a;
  a.project.reset(new project_t("foo", tmpdir));
  canvas_holder canvas;
  std::unique_ptr<test_map> m(std::make_unique<test_map>(a, *canvas, test_map::NodeStyle));
  a.project->osm.reset(new osm_t());
  osm_t::ref o = a.project->osm;
  set_bounds(o);
  iconbar_t::create(a);

  MainUiDummy * const ui = static_cast<MainUiDummy *>(a.uicontrol.get());

  way_t *w = new way_t();

  // where the click will be
  osm2go_platform::screenpos node1pos(100, 100);
  osm2go_platform::screenpos node2pos(200, 200);

  lpos_t pos = canvas->window2world(node1pos);
  node_t *n = o->node_new(pos);
  o->attach(n);
  w->append_node(n);

  pos = canvas->window2world(node2pos);
  n = o->node_new(pos);
  o->attach(n);
  w->append_node(n);
  o->attach(w);

  osm2go_platform::screenpos node3pos(node2pos.x(), node1pos.y());
  std::vector<tag_t> ntags(1, tag_t("emergency", "fire_hydrant"));

  pos = canvas->window2world(node3pos);
  n = o->node_new(pos);
  n->tags.replace(std::move(ntags));
  o->attach(n);

  m->paint();

  // FIXME: this should also work with the Gtk version, but it doesn't.
  // I suspect it's a problem with the test (widget) setup where something is missing.
#ifdef QT_VERSION
  // select the way
  osm2go_platform::screenpos clickpos((node1pos.x() + node2pos.x()) / 2, (node1pos.y() + node2pos.y()) / 2);

  m->button_press_public(clickpos);
  assert_cmpnum(m->highlight.items.size(), 0);

  expectMapItemDeselect(ui);
  ui->m_statusTexts.push_back(trstring("unspecified way"));
  ui->m_actions.insert(std::make_pair(MainUi::MENU_ITEM_MAP_HIDE_SEL, true));
  m->button_release_public(clickpos);

  assert(m->selected.object == w);
  // 1 for each node, 1 for the arrow, and 1 for the way itself
  assert_cmpnum(m->highlight.items.size(), 4);
#endif

  // deselect
  osm2go_platform::screenpos emptypos(node1pos.x(), node2pos.y());

  m->button_press_public(emptypos);
  expectMapItemDeselect(ui);
  m->button_release_public(emptypos);

  assert_cmpnum(m->selected.object.type, object_t::ILLEGAL);
  assert_cmpnum(m->highlight.items.size(), 0);

  relation_t *r = new relation_t();
  r->members.push_back(member_t(object_t(w), nullptr));
  r->members.push_back(member_t(object_t(n), nullptr));
  o->attach(r);

  ui->m_statusTexts.push_back(trstring("unspecified relation"));
  m->select_relation(r);
  assert_cmpnum(m->highlight.items.size(), 2);

  assert(m->selected.object == object_t(r));
}

} // namespace

int main(int argc, char **argv)
{
  char tmpdir[] = "/tmp/osm2go-project-XXXXXX";

  if(mkdtemp(tmpdir) == nullptr) {
    std::cerr << "cannot create temporary directory" << std::endl;
    return 1;
  }

  OSM2GO_TEST_INIT(argc, argv);

  std::string osm_path = tmpdir;
  osm_path += '/';

  test_map_delete();
  test_map_delete_items();
  test_draw_deleted(osm_path);
  test_draw_hidden(osm_path);
  test_way_add_cancel(osm_path);
  test_map_item_deleter(osm_path);
  test_map_deselect(osm_path);
  test_way_add_cancel_map(osm_path);
  test_node_add_cancel_map(osm_path);
  test_node_add_ok_map(osm_path);
  test_map_detail(osm_path);
  test_map_item_at_empty(osm_path);
  test_map_press_idle(osm_path);
  test_map_drag_idle(osm_path);
  test_map_press_way_add_cancel(osm_path);
  test_map_press_way_reuse_add_cancel(osm_path);
  test_map_press_way_intermediate_reuse_add_ok(osm_path);
  test_map_node_create_outside(osm_path);
  test_map_reverse(osm_path);
  test_select(osm_path);

  assert_cmpnum(rmdir(tmpdir), 0);

  return 0;
}

#include "dummy_appdata.h"
