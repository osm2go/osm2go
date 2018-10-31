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
#include "iconbar.h"
#include "info.h"
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
#include "osm2go_platform_gtk.h"
#include <osm2go_stl.h>

class map_internal : public map_t {
public:
  explicit map_internal(appdata_t &a);

  osm2go_platform::Timer autosave;

  static gboolean map_motion_notify_event(GtkWidget *, GdkEventMotion *event, map_internal *map);
  static gboolean map_button_event(map_internal *map, GdkEventButton *event);
};

static gboolean map_destroy_event(map_internal *map)
{
  g_debug("destroying entire map");

  map->appdata.map = nullptr;
  delete map;

  return FALSE;
}

static gboolean map_scroll_event(GtkWidget *, GdkEventScroll *event, map_t *map) {
  if(unlikely(!map->appdata.project->osm))
    return FALSE;

  if(event->type == GDK_SCROLL) {
    double zoom = map->state.zoom;
    if(event->direction)
      zoom /= ZOOM_FACTOR_WHEEL;
    else
      zoom *= ZOOM_FACTOR_WHEEL;
    map->set_zoom(zoom, true);
  }

  return TRUE;
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

  if(gtk_events_pending() == TRUE)
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
  if(gtk_window_is_active(GTK_WINDOW(appdata_t::window)) == TRUE) {
    g_debug("autosave ...");

    if(likely(map->appdata.project)) {
      track_save(map->appdata.project, map->appdata.track.track.get());
      map->appdata.project->diff_save();
    }
  } else
    g_debug("autosave suppressed");

  return TRUE;
}

map_internal::map_internal(appdata_t &a)
  : map_t(a, canvas_t::create())
{
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
    canvas->move_background(bounds.min.x + bg.offset.x, bounds.min.y + bg.offset.y);
    break;
  }

  default:
    break;
  }

  set_action(MAP_ACTION_IDLE);
}

/* -------- hide and show objects (for performance reasons) ------- */

void map_t::set_autosave(bool enable) {
  map_internal *m = static_cast<map_internal *>(this);
  if(enable)
    m->autosave.restart(120, map_autosave, this);
  else
    m->autosave.stop();
}
