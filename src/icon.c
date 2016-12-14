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
 * along with OSM2Go.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "icon.h"

#include "appdata.h"
#include "misc.h"

#include <sys/stat.h>

struct icon_t {
  GdkPixbuf *buf;
  int use;
  struct icon_t *next;
  gchar name[];
};

static gchar*
icon_file_exists(const gchar *file) {
  const char *icon_exts[] = { ".gif", ".png", ".jpg", NULL };
  gint idx;

  for (idx = 0; icon_exts[idx]; idx++) {
    gchar *fullname = find_file("icons/", file, icon_exts[idx]);

    if(fullname)
      return fullname;
  }
  return NULL;
}

GdkPixbuf *icon_load(icon_t **icon, const char *name) {
  if(!name) return NULL;

  /* check if icon list already contains an icon of that name */
  while(*icon) {
    if(strcmp(name, (*icon)->name) == 0) {
      //      printf("reuse existing icon\n");
      (*icon)->use++;
      return (*icon)->buf;
    }

    icon = &((*icon)->next);
  }

  gchar *fullname = icon_file_exists(name);
  if(fullname) {
    GdkPixbuf *pix = gdk_pixbuf_new_from_file(fullname, NULL);
    g_free(fullname);

    //    printf("Successfully loaded icon %s to %p\n", name, pix);
      size_t nlen = strlen(name) + 1;
      *icon = g_malloc(sizeof(**icon) + nlen);
      memcpy((*icon)->name, name, nlen);
      (*icon)->buf = pix;
      (*icon)->use = 1;
      (*icon)->next = NULL;

      return pix;
  }

  printf("Icon %s not found\n", name);
  return NULL;
}

GtkWidget *icon_widget_load(icon_t **icon, const char *name) {
  GdkPixbuf *pix = icon_load(icon, name);
  if(!pix) return NULL;

  return gtk_image_new_from_pixbuf(pix);
}

static icon_t *icon_destroy(icon_t *icon) {
  icon_t *next = icon->next;

  if(icon->buf)
    g_object_unref(icon->buf);
  g_free(icon);

  return next;
}

void icon_free(icon_t **icon, GdkPixbuf *buf) {
  //  printf("request to free icon %p\n", buf);

  while(*icon) {
    //    printf("   -> %s %p\n", (*icon)->name, (*icon)->buf);

    if(buf == (*icon)->buf) {
      (*icon)->use--;
      if(!(*icon)->use) {
	//	printf("freeing unused icon %s\n", (*icon)->name);

	*icon = icon_destroy(*icon);

      } else {
	//	printf("keeping icon %s still in use by %d\n",
	//	       (*icon)->name, (*icon)->use);
      }

      return;
    }
    icon = &((*icon)->next);
  }

  printf("ERROR: icon to be freed not found\n");
}

void icon_free_all(icon_t **icons) {
  int cnt = 0;

  icon_t *icon = *icons;
  while(icon) {
    cnt++;
    icon = icon_destroy(icon);
  }

  *icons = NULL;

  printf("freed %d icons\n", cnt);
}
