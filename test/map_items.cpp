#include <map.h>

#include <appdata.h>
#include <gps_state.h>
#include <iconbar.h>
#include <josm_presets.h>
#include <osm.h>
#include <osm2go_annotations.h>
#include <style.h>
#include <uicontrol.h>

#include <iostream>
#include <memory>
#include <unistd.h>

namespace {

class MainUiDummy : public MainUi {
public:
  MainUiDummy() : MainUi(), msg(nullptr) {}
  void setActionEnable(menu_items, bool) override
  { abort(); }
  void showNotification(const char *, unsigned int) override
  { abort(); }
  const char *msg;
};

} // namespace

appdata_t::appdata_t()
  : uicontrol(new MainUiDummy())
  , map(nullptr)
  , icons(icon_t::instance())
{
}

appdata_t::~appdata_t()
{
}

void appdata_t::track_clear()
{
  abort();
}

class test_map: public map_t {
public:
  explicit test_map(appdata_t &a) : map_t(a, nullptr) {}

  void set_autosave(bool) override { abort(); }

  void test_way_add_cancel();
};

static void set_bounds(osm_t::ref o) {
  bool b = o->bounds.init(pos_area(pos_t(52.2692786, 9.5750497), pos_t(52.2695463, 9.5755)));
  assert(b);
}

static void test_map_delete()
{
  appdata_t a;
  std::unique_ptr<map_t> m(new test_map(a));
}

static void test_map_delete_items()
{
  appdata_t a;
  std::unique_ptr<map_t> m(new test_map(a));
  std::unique_ptr<osm_t> o(new osm_t());
  set_bounds(o);

  way_t *w = o->way_attach(new way_t(1));

  // keep it here, it ill only be reset, but not freed as that is done through the map
  std::unique_ptr<map_item_t> mi(new map_item_t(object_t(w), nullptr));
  w->map_item = mi.get();

  o->way_delete(w, m.get());

  lpos_t p(10, 10);
  node_t *n = o->node_new(p);
  o->node_attach(n);
  n->map_item = mi.get();

  o->node_delete(n);
}

static void test_draw_deleted(const std::string &tmpdir)
{
  appdata_t a;
  a.project.reset(new project_t("foo", tmpdir));
  std::unique_ptr<map_t> m(new test_map(a));
  m->style.reset(new style_t());
  std::unique_ptr<osm_t> o(new osm_t());
  set_bounds(o);

  lpos_t p(10, 10);
  node_t *n = o->node_new(p);
  o->node_attach(n);
  n->flags = OSM_FLAG_DELETED;
  assert(n->isDeleted());

  m->draw(n);

  way_t *w = new way_t(1);
  o->way_attach(w);
  w->flags = OSM_FLAG_DELETED;
  assert(w->isDeleted());

  m->draw(w);
}

void test_map::test_way_add_cancel()
{
  way_add_begin();
  way_add_cancel();
}

static void test_way_add_cancel(const std::string &tmpdir)
{
  appdata_t a;
  std::unique_ptr<test_map> m(new test_map(a));

  a.project.reset(new project_t("foo", tmpdir));
  a.project->osm.reset(new osm_t());
  set_bounds(a.project->osm);

  m->test_way_add_cancel();
}

int main()
{
  char tmpdir[] = "/tmp/osm2go-project-XXXXXX";

  if(mkdtemp(tmpdir) == nullptr) {
    std::cerr << "cannot create temporary directory" << std::endl;
    return 1;
  }

  std::string osm_path = tmpdir;
  osm_path += '/';

  test_map_delete();
  test_map_delete_items();
  test_draw_deleted(osm_path);
  test_way_add_cancel(osm_path);

  assert_cmpnum(rmdir(tmpdir), 0);

  return 0;
}
