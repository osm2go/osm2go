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

#ifndef QND_XML_H
#define QND_XML_H

typedef struct qnd_xml_attribute_s {
  char *name, *value;
  struct qnd_xml_attribute_s *next;
} qnd_xml_attribute_t;

typedef struct qnd_xml_stack_s {
  struct qnd_xml_entry_s *entry;
  gpointer userdata[4];
  
  struct qnd_xml_stack_s *prev;
  struct qnd_xml_stack_s *next;
} qnd_xml_stack_t;

typedef gboolean(*qnd_xml_callback_t)(qnd_xml_stack_t *, 
				      qnd_xml_attribute_t *, gpointer);

typedef struct qnd_xml_entry_s {
  char *name;

  qnd_xml_callback_t cb;

  int num_children;
  struct qnd_xml_entry_s **children;
} qnd_xml_entry_t;

#define QND_XML_CHILDREN(a) (sizeof(a)/sizeof(qnd_xml_entry_t *)), a
#define QND_XML_LEAF  0, NULL

gpointer qnd_xml_parse(char *name, qnd_xml_entry_t *root, gpointer userdata);
char *qnd_xml_get_prop(qnd_xml_attribute_t *attr, char *name);
gboolean qnd_xml_get_prop_double(qnd_xml_attribute_t *, char *, double *);
gboolean qnd_xml_get_prop_gulong(qnd_xml_attribute_t *, char *, gulong *);
gboolean qnd_xml_get_prop_is(qnd_xml_attribute_t *, char *, char *);
char *qnd_xml_get_prop_str(qnd_xml_attribute_t *attr, char *name);

#endif // QND_XML_H
