/*
 * SPDX-FileCopyrightText: 2008 Till Harbaum <till@harbaum.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "map_gtk.h"

#include <appdata.h>
#include <canvas.h>
#include "canvas_goocanvas.h"
#include <diff.h>
#include <iconbar.h>
#include <info.h>
#include "osm2go_platform.h"
#include <project.h>
#include <style.h>
#include <track.h>
#include <uicontrol.h>

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

static gboolean map_destroy_event(map_gtk *map)
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
    double zoom = map->appdata.project->map_state.zoom;
    if(event->direction == GDK_SCROLL_DOWN)
      zoom /= ZOOM_FACTOR_WHEEL;
    else
      zoom *= ZOOM_FACTOR_WHEEL;
    map->set_zoom(zoom, true);
  }

  return TRUE;
}

gboolean map_gtk::map_button_event(map_gtk *map, GdkEventButton *event) {
  if(unlikely(!map->appdata.project->osm))
    return FALSE;

  if(event->button == 1) {
    osm2go_platform::screenpos p(event->x, event->y);

    if(event->type == GDK_BUTTON_PRESS)
      map->button_press(p);

    else if(event->type == GDK_BUTTON_RELEASE)
      map->button_release(p);
  }

  return FALSE;  /* forward to further processing */
}

gboolean map_gtk::map_motion_notify_event(GtkWidget *, GdkEventMotion *event, map_gtk *map) {
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

  osm2go_platform::screenpos p(0, 0);
  /* handle hints */
  if(event->is_hint) {
    gint x, y;
    gdk_window_get_pointer(event->window, &x, &y, nullptr);
    p = osm2go_platform::screenpos(x, y);
  } else {
    p = osm2go_platform::screenpos(event->x, event->y);
  }

  map->handle_motion(p);

  return FALSE;  /* forward to further processing */
}

gboolean map_gtk::key_press_event(unsigned int keyval)
{
  switch(keyval) {
  case GDK_Left:
    scroll_step(osm2go_platform::screenpos(-50, 0));
    break;

  case GDK_Right:
    scroll_step(osm2go_platform::screenpos(+50, 0));
    break;

  case GDK_Up:
    scroll_step(osm2go_platform::screenpos(0, -50));
    break;

  case GDK_Down:
    scroll_step(osm2go_platform::screenpos(0, +50));
    break;

  case GDK_KP_Enter:
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
    set_zoom(appdata.project->map_state.zoom * ZOOM_FACTOR_BUTTON, true);
    return TRUE;

#ifdef FREMANTLE
  case HILDON_HARDKEY_DECREASE:
#else
  case '-':
  case GDK_KP_Subtract:
#endif
    set_zoom(appdata.project->map_state.zoom / ZOOM_FACTOR_BUTTON, true);
    return TRUE;

  default:
    g_debug("key event %d", keyval);
    break;
  }

  return FALSE;
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

map_gtk::map_gtk(appdata_t &a)
  : map_t(a, new canvas_goocanvas())
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

void map_gtk::set_autosave(bool enable)
{
  if(enable)
    autosave.restart(120, map_autosave, this);
  else
    autosave.stop();
}
