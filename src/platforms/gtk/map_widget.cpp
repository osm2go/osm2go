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
 * along with OSM2Go.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "map.h"

#include "appdata.h"
#include "canvas.h"
#include "diff.h"
#include "gps.h"
#include "iconbar.h"
#include "info.h"
#include "map_hl.h"
#include "misc.h"
#include "osm2go_platform.h"
#include "project.h"
#include "style.h"
#include "track.h"
#include "uicontrol.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <gdk/gdkkeysyms.h>
#ifdef FREMANTLE
#include <hildon/hildon-defines.h>
#endif
#include <memory>
#include <vector>

#include "osm2go_annotations.h"
#include <osm2go_cpp.h>
#include <osm2go_i18n.h>
#include <osm2go_stl.h>

class map_internal : public map_t {
  map_highlight_t m_hl;
public:
  map_internal(appdata_t &a);

  osm2go_platform::Timer autosave;

  struct {
    std::unique_ptr<GdkPixbuf, g_object_deleter> pix;
    canvas_item_pixmap *item;
  } background;

  static gboolean map_motion_notify_event(GtkWidget *, GdkEventMotion *event, map_internal *map);
  static gboolean map_button_event(map_internal *map, GdkEventButton *event);
};

static gboolean map_destroy_event(map_t *map) {
  g_debug("destroying entire map");

  map->appdata.map = nullptr;
  delete map;

  return FALSE;
}

static gboolean map_scroll_event(GtkWidget *, GdkEventScroll *event, map_t *map) {
  if(unlikely(!map->appdata.project->osm))
    return FALSE;

  if(event->type == GDK_SCROLL && map) {
    double zoom = map->state.zoom;
    if(event->direction)
      zoom /= ZOOM_FACTOR_WHEEL;
    else
      zoom *= ZOOM_FACTOR_WHEEL;
    map->set_zoom(zoom, true);
  }

  return TRUE;
}

/* move the background image (wms data) during wms adjustment */
void map_t::bg_adjust(int x, int y) {
  osm_t::ref osm = appdata.project->osm;
  assert(osm);

  x += osm->bounds.min.x + bg.offset.x - pen_down.at.x;
  y += osm->bounds.min.y + bg.offset.y - pen_down.at.y;

  static_cast<map_internal *>(this)->background.item->image_move(x, y, bg.scale.x, bg.scale.y);
}

gboolean map_internal::map_button_event(map_internal *map, GdkEventButton *event) {
  if(unlikely(!map->appdata.project->osm))
    return FALSE;

  if(event->button == 1) {
    float x = event->x, y = event->y;

    if(event->type == GDK_BUTTON_PRESS)
      map->button_press(x, y);

    else if(event->type == GDK_BUTTON_RELEASE)
      map->button_release(x, y);
  }

  return FALSE;  /* forward to further processing */
}

gboolean map_internal::map_motion_notify_event(GtkWidget *, GdkEventMotion *event, map_internal *map) {
  gint x, y;
  GdkModifierType state;

  if(unlikely(!map->appdata.project || !map->appdata.project->osm))
    return FALSE;

#if 0 // def FREMANTLE
  /* reduce update frequency on hildon to keep screen update fluid */
  static guint32 last_time = 0;

  if(event->time - last_time < 250) return FALSE;
  last_time = event->time;
#endif

  if(gtk_events_pending())
    return FALSE;

  if(!map->pen_down.is)
    return FALSE;

  /* handle hints */
  if(event->is_hint)
    gdk_window_get_pointer(event->window, &x, &y, &state);
  else {
    x = event->x;
    y = event->y;
    state = static_cast<GdkModifierType>(event->state);
  }

  map->handle_motion(x, y);

  return FALSE;  /* forward to further processing */
}

bool map_t::key_press_event(unsigned int keyval) {
  switch(keyval) {
  case GDK_Left:
    scroll_step(-50, 0);
    break;

  case GDK_Right:
    scroll_step(+50, 0);
    break;

  case GDK_Up:
    scroll_step(0, -50);
    break;

  case GDK_Down:
    scroll_step(0, +50);
    break;

  case GDK_Return:   // same as HILDON_HARDKEY_SELECT
    /* if the ok button is enabled, call its function */
    if(appdata.iconbar->isOkEnabled())
      action_ok();
    /* otherwise if info is enabled call that */
    else if(appdata.iconbar->isInfoEnabled())
      info_selected();
    break;

  case GDK_Escape:   // same as HILDON_HARDKEY_ESC
    /* if the cancel button is enabled, call its function */
    if(appdata.iconbar->isCancelEnabled())
      appdata.map->action_cancel();
    break;

  case GDK_Delete:
    /* if the delete button is enabled, call its function */
    if(appdata.iconbar->isTrashEnabled())
      delete_selected();
    break;

#ifdef FREMANTLE
  case HILDON_HARDKEY_INCREASE:
#else
  case '+':
  case GDK_KP_Add:
#endif
    set_zoom(state.zoom * ZOOM_FACTOR_BUTTON, true);
    return true;

#ifdef FREMANTLE
  case HILDON_HARDKEY_DECREASE:
#else
  case '-':
  case GDK_KP_Subtract:
#endif
    set_zoom(state.zoom / ZOOM_FACTOR_BUTTON, true);
    return true;

  default:
    g_debug("key event %d", keyval);
    break;
  }

  return false;
}

static gboolean map_autosave(gpointer data) {
  map_t *map = static_cast<map_t *>(data);

  /* only do this if root window has focus as otherwise */
  /* a dialog may be open and modifying the basic structures */
  if(gtk_window_is_active(GTK_WINDOW(appdata_t::window))) {
    g_debug("autosave ...");

    if(likely(map->appdata.project)) {
      track_save(map->appdata.project.get(), map->appdata.track.track.get());
      map->appdata.project->diff_save();
    }
  } else
    g_debug("autosave suppressed");

  return TRUE;
}

map_internal::map_internal(appdata_t &a)
  : map_t(a, m_hl)
{
  background.item = nullptr;

  g_signal_connect_swapped(canvas->widget, "button_press_event",
                           G_CALLBACK(map_button_event), this);
  g_signal_connect_swapped(canvas->widget, "button_release_event",
                           G_CALLBACK(map_button_event), this);
  g_signal_connect(canvas->widget, "motion_notify_event",
                   G_CALLBACK(map_motion_notify_event), this);
  g_signal_connect(canvas->widget, "scroll_event",
                   G_CALLBACK(map_scroll_event), this);

  g_signal_connect_swapped(canvas->widget, "destroy",
                           G_CALLBACK(map_destroy_event), this);
}

map_t *map_t::create(appdata_t &a)
{
  return new map_internal(a);
}

void map_t::action_cancel() {
  switch(action.type) {
  case MAP_ACTION_WAY_ADD:
    way_add_cancel();
    break;

  case MAP_ACTION_BG_ADJUST: {
    /* undo all changes to bg_offset */
    bg.offset.x = appdata.project->wms_offset.x;
    bg.offset.y = appdata.project->wms_offset.y;

    const bounds_t &bounds = appdata.project->osm->bounds;
    static_cast<map_internal *>(this)->background.item->image_move(bounds.min.x + bg.offset.x,
                                                                   bounds.min.y + bg.offset.y,
                                                                   bg.scale.x, bg.scale.y);
    break;
  }

  default:
    break;
  }

  set_action(MAP_ACTION_IDLE);
}

/* ------------------- map background ------------------ */

void map_t::remove_bg_image() {
  map_internal *m = static_cast<map_internal *>(this);
  if(m->background.item) {
    delete m->background.item;
    m->background.item = nullptr;
  }
}

static void map_bg_item_destroy_event(gpointer data) {
  map_internal *map = static_cast<map_internal *>(data);

  /* destroying background item */

  map->background.item = nullptr;
  if(map->background.pix) {
    g_debug("destroying background item");
    map->background.pix.reset();
  }
}

bool map_t::set_bg_image(const std::string &filename) {
  const bounds_t &bounds = appdata.project->osm->bounds;

  remove_bg_image();

  map_internal *m = static_cast<map_internal *>(this);

  m->background.pix.reset(gdk_pixbuf_new_from_file(filename.c_str(), nullptr));
  if(!m->background.pix)
    return false;

  /* calculate required scale factor */
  bg.scale.x = static_cast<float>(bounds.max.x - bounds.min.x) /
                    gdk_pixbuf_get_width(m->background.pix.get());
  bg.scale.y = static_cast<float>(bounds.max.y - bounds.min.y) /
                    gdk_pixbuf_get_height(m->background.pix.get());

  m->background.item = canvas->image_new(CANVAS_GROUP_BG, m->background.pix.get(),
                              bounds.min.x, bounds.min.y, bg.scale.x, bg.scale.y);

  m->background.item->destroy_connect(map_bg_item_destroy_event, this);

  int x = bounds.min.x + bg.offset.x;
  int y = bounds.min.y + bg.offset.y;
  m->background.item->image_move(x, y, bg.scale.x, bg.scale.y);

  return true;
}

/* -------- hide and show objects (for performance reasons) ------- */

void map_t::set_autosave(bool enable) {
  map_internal *m = static_cast<map_internal *>(this);
  if(enable)
    m->autosave.restart(120, map_autosave, this);
  else
    m->autosave.stop();
}

// vim:et:ts=8:sw=2:sts=2:ai
