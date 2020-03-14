#include "dummy_map.h"

#include <canvas.h>
#include <canvas_p.h>
#include <style.h>

#include <cassert>
#include <iostream>
#include <memory>

#include <osm2go_annotations.h>
#include <osm2go_stl.h>
#include <osm2go_test.h>

namespace {

void set_bounds(osm_t::ref o)
{
  bool b = o->bounds.init(pos_area(pos_t(52.2692786, 9.5750497), pos_t(52.2695463, 9.5755)));
  assert(b);
}

void testBackground()
{
  char tmpdir[] = "/tmp/osm2go-canvas-XXXXXX";

  if(mkdtemp(tmpdir) == nullptr) {
    std::cerr << "cannot create temporary directory" << std::endl;
    return;
  }

  std::string osm_path = tmpdir;
  osm_path += '/';

  std::string nonfile = osm_path + "not_here.jpg";

  appdata_t a;
  canvas_holder canvas;
  std::unique_ptr<map_t> m(std::make_unique<test_map>(a, *canvas));
  a.project.reset(new project_t("test_proj", tmpdir));
  a.project->osm.reset(new osm_t());
  set_bounds(a.project->osm);
  m->style.reset(new style_t());
  a.track.track.reset(new track_t());
  MainUiDummy * const ui = static_cast<MainUiDummy *>(a.uicontrol.get());

  ui->m_actions[MainUi::MENU_ITEM_WMS_CLEAR] = false;
  ui->m_actions[MainUi::MENU_ITEM_WMS_ADJUST] = false;
  assert(!m->set_bg_image(std::string(), osm2go_platform::screenpos(0, 0)));
  assert(ui->m_actions.empty());

  ui->m_actions[MainUi::MENU_ITEM_WMS_CLEAR] = false;
  ui->m_actions[MainUi::MENU_ITEM_WMS_ADJUST] = false;
  assert(!m->set_bg_image(nonfile, osm2go_platform::screenpos(0, 0)));
  assert(ui->m_actions.empty());

  ui->m_actions[MainUi::MENU_ITEM_WMS_CLEAR] = false;
  ui->m_actions[MainUi::MENU_ITEM_WMS_ADJUST] = false;
  m->remove_bg_image();
  assert(ui->m_actions.empty());
}

} // namespace

int main()
{
  testBackground();

  return 0;
}

#include "dummy_appdata.h"
