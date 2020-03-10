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

void testSegment()
{
  std::vector<lpos_t> points;
  for (unsigned int i = 0; i < 8; i += 2)
    points.push_back(lpos_t(1 << i, 2 << i));

  canvas_holder canvas;

  canvas_item_t * const line = canvas->polyline_new(CANVAS_GROUP_WAYS, points, 1, 0);
  assert(line);

  int segnum = canvas->get_item_segment(line, lpos_t((4 + 16) / 2, (8 + 32) / 2));
  assert_cmpnum(segnum, 1);
}

void testInObject()
{
  canvas_holder canvas;

  // a circle that should have nothing to do with the initial search
  canvas_item_circle * const circle = canvas->circle_new(CANVAS_GROUP_WAYS, lpos_t(100, 20), 15,
                                                         0, color_t::transparent());
  assert(circle != nullptr);

  // a square, rotated by 45 degrees
  std::vector<lpos_t> points;
  points.push_back(lpos_t(0, 200));
  points.push_back(lpos_t(200, 400));
  points.push_back(lpos_t(400, 200));
  points.push_back(lpos_t(200, 0));
  points.push_back(points.front());

  canvas_item_t * const line = canvas->polygon_new(CANVAS_GROUP_WAYS, points, 1, 0, 0);
  assert(line != nullptr);

  canvas_item_t *search = canvas->get_item_at(lpos_t(200, 200));
  assert(line == search);

  search = canvas->get_item_at(lpos_t(40, 50));
  assert_null(search);

  // now try to find the circle
  // the given position is slightly outside the circle, but the fuzziness
  // should still catch it
  search = canvas->get_item_at(lpos_t(100, 38));
  assert(circle == search);
}

void testToBottom()
{
  std::vector<lpos_t> points;
  for (unsigned int i = 0; i < 3; i += 2)
    points.push_back(lpos_t(1 << i, 2 << i));

  canvas_holder canvas;

  // just to be sure that this does no harm
  assert_null(canvas->get_item_at(lpos_t(3, 6)));

  // 2 polygons that overlap
  canvas_item_t * const line = canvas->polyline_new(CANVAS_GROUP_WAYS, points, 1, 0);
  assert(line != nullptr);

  for (unsigned int i = 0; i < points.size(); i += 2) {
    lpos_t p = points[i];
    p.x *= 2;
    p.y *= 2;
    points[i] = p;
  }

  canvas_item_t *line2 = canvas->polyline_new(CANVAS_GROUP_WAYS, points, 1, 0);
  assert(line2 != nullptr);

  // an area polygon
  points.clear();
  points.push_back(lpos_t(2, 1));
  points.push_back(lpos_t(EXTRA_FUZZINESS_PIXEL * 3, 0));
  points.push_back(lpos_t(4, 7));
  points.push_back(lpos_t(1, 6));
  points.push_back(points.front());
  canvas_item_t *bgpoly = canvas->polygon_new(CANVAS_GROUP_POLYGONS, points, 1, color_t::black(), color_t::black());
  assert(bgpoly != nullptr);

  canvas_item_t *search1 = canvas->get_item_at(lpos_t(3, 6));
  // must be one of the items, it's exactly on them
  assert(search1 == line || search1 == line2);

  // now the other one must be on top
  canvas->item_to_bottom(search1);
  canvas_item_t *search2 = canvas->get_item_at(lpos_t(3, 6));
  assert(search2 == line || search2 == line2);
  assert(search1 != search2);

  // and back to the first
  canvas->item_to_bottom(search2);
  canvas_item_t *search3 = canvas->get_item_at(lpos_t(3, 6));
  assert(search1 == search3);

  canvas->item_to_bottom(search3);
  search3 = canvas->get_item_at(lpos_t(3, 6));
  assert(search2 == search3);
  canvas->item_to_bottom(search3);
  search3 = canvas->get_item_at(lpos_t(3, 6));
  assert(search1 == search3);

  // now the polygon should be the item
  search3 = canvas->get_item_at(lpos_t(EXTRA_FUZZINESS_PIXEL * 2, 1));
  assert(bgpoly == search3);

  // there is only one item at that position, so it should be returned again
  canvas->item_to_bottom(search3);
  search3 = canvas->get_item_at(lpos_t(15, 1));
  assert(bgpoly == search3);

  // outside of everything
  search3 = canvas->get_item_at(lpos_t(EXTRA_FUZZINESS_PIXEL * 4, 3));
  assert_null(search3);
}

void testTrackSegments()
{
  char tmpdir[] = "/tmp/osm2go-canvas-XXXXXX";

  if(mkdtemp(tmpdir) == nullptr) {
    std::cerr << "cannot create temporary directory" << std::endl;
    return;
  }

  std::string osm_path = tmpdir;
  osm_path += '/';

  appdata_t a;
  canvas_holder canvas;
  std::unique_ptr<map_t> m(std::make_unique<test_map>(a, *canvas));
  a.project.reset(new project_t("test_proj", tmpdir));
  a.project->osm.reset(new osm_t());
  set_bounds(a.project->osm);
  m->style.reset(new style_t());
  a.track.track.reset(new track_t());

  a.track.track->segments.push_back(track_seg_t());
  {
    track_seg_t &tseg1 = a.track.track->segments.back();

    // calling this on an empty segment should do nothing
    m->track_draw_seg(tseg1);
    assert(tseg1.item_chain.empty());

    // all items are outside the bounds
    for (int i = 0; i < 5; i++)
      tseg1.track_points.push_back(track_point_t(pos_t(i, i)));
    m->elements_drawn = 42;

    m->track_draw_seg(tseg1);
    assert(tseg1.item_chain.empty());
    assert_cmpnum(m->elements_drawn, 0);

    // and this one is still outside
    tseg1.track_points.push_back(track_point_t(pos_t(8, 8)));
    m->track_update_seg(tseg1);
    assert(tseg1.item_chain.empty());
    assert_cmpnum(m->elements_drawn, 0);

    tseg1.track_points.clear();

    // draw one element in the middle of the bounds
    tseg1.track_points.push_back(track_point_t(a.project->osm->bounds.ll.center()));
    m->track_draw_seg(tseg1);
    assert(!tseg1.item_chain.empty());
    assert_cmpnum(m->elements_drawn, 1);
  }

  a.track.track->clear_current();

  pos_t uncenter = a.project->osm->bounds.ll.center();
  // a track segment entering the bounds
  a.track.track->segments.push_back(track_seg_t());
  {
    track_seg_t &tseg2 = a.track.track->segments.back();

    tseg2.track_points.push_back(track_point_t(pos_t(0, 0)));
    tseg2.track_points.push_back(track_point_t(a.project->osm->bounds.ll.center()));
    m->track_draw_seg(tseg2);
    assert(!tseg2.item_chain.empty());
    assert_cmpnum(m->elements_drawn, 2);

    // add another point that is within bounds
    uncenter.lat = (uncenter.lat + a.project->osm->bounds.ll.max.lat) / 2;
    tseg2.track_points.push_back(track_point_t(uncenter));
    m->track_update_seg(tseg2);
    assert(!tseg2.item_chain.empty());
    assert_cmpnum(m->elements_drawn, 3);
  }

  a.track.track->clear();

  // a track segment going in and out
  a.track.track->segments.push_back(track_seg_t());
  {
    track_seg_t &tseg3 = a.track.track->segments.back();

    tseg3.track_points.push_back(track_point_t(pos_t(0, 0)));
    tseg3.track_points.push_back(track_point_t(a.project->osm->bounds.ll.center()));
    tseg3.track_points.push_back(track_point_t(pos_t(2, 2)));
    m->track_draw_seg(tseg3);
    assert(!tseg3.item_chain.empty());
    assert_cmpnum(m->elements_drawn, 3);

    // add another one that now is onscreen again
    tseg3.track_points.push_back(track_point_t(uncenter));
    m->track_update_seg(tseg3);
    assert_cmpnum(tseg3.item_chain.size(), 1);
    assert_cmpnum(m->elements_drawn, 4);
  }
}

} // namespace

int main()
{
  testSegment();
  testInObject();
  testToBottom();
  testTrackSegments();

  return 0;
}

#include "dummy_appdata.h"
