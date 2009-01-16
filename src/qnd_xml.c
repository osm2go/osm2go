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

/*
 * qnd_xml - quick'n dirty xml is a very small and very fast implementation
 *           of a xml parser. The idea is to replace the usage of libxml2
 *           by this whenever performance is an issue. This is the case
 *           with reading the *.osm files on mobile devices. A powerful
 *           desktop will likely still use the libxml as it's just "better"
 */

#include "appdata.h"

#include <ctype.h>
int isblank(int c);

#define QND_XML_BUFFER_SIZE 1024
typedef struct {
  gpointer userdata;

  FILE *file;
  int total, bytes_read;

  char buffer[QND_XML_BUFFER_SIZE], *cur;
  int fill;

  qnd_xml_stack_t *stack, *sp;
  int mod;   // modifier (?, !, /) in element
  gboolean done;

  qnd_xml_attribute_t *attributes;

} qnd_xml_context_t;


void stack_dump(qnd_xml_context_t *context) {
  qnd_xml_stack_t *stack = context->stack;

  printf("Stack:\n");
  while(stack) {
    if(stack == context->sp) printf(" *");
    else                     printf("  ");

    printf("%s\n", stack->entry->name);
    stack = stack->next;
  }
}

void stack_push(qnd_xml_context_t *context, qnd_xml_entry_t *entry) {
  //  printf("push %s\n", entry->name);

  context->sp->next = g_new0(qnd_xml_stack_t, 1);
  context->sp->next->prev = context->sp;
  context->sp = context->sp->next;
  context->sp->entry = entry;

  //  stack_dump(context);
}

qnd_xml_entry_t *stack_pop(qnd_xml_context_t *context) {
  qnd_xml_entry_t *cur = context->sp->entry;

  context->sp = context->sp->prev;
  g_free(context->sp->next);
  context->sp->next = NULL;

  /* did we just empty the stack? if yes, we're done parsing */
  if(context->sp == context->stack) {
    printf("done parsing\n");
    context->done = TRUE;
  }

  //  printf("popped %s\n", cur->name);
  //  stack_dump(context);
  return cur;
}

gboolean update_buffer(qnd_xml_context_t *context) {

  /* if buffer is empty just fill it */
  if(!context->fill) {
    context->cur = context->buffer;
    context->fill = fread(context->buffer, 1l, 
	  QND_XML_BUFFER_SIZE, context->file);

    if(context->fill < 0) {
      printf("read error\n");
      context->fill = 0;
      return FALSE;
    }    
    context->bytes_read += context->fill;
    return TRUE;
  }

  /* shift remaining data down */
  int offset = context->cur - context->buffer;
  g_memmove(context->buffer, context->cur, QND_XML_BUFFER_SIZE - offset);
  context->fill -= offset;
  int bytes_read = fread(context->buffer + QND_XML_BUFFER_SIZE - 
			 offset, 1l, offset, context->file);

  context->cur = context->buffer;
  if(bytes_read < 0) {
    printf("read error\n");
    return FALSE;
  }

  context->bytes_read += bytes_read;
  context->fill += bytes_read;
  return TRUE;
}

/* 
   utf8:
   0xxxxxxx
   110xxxxx 10xxxxxx
   1110xxxx 10xxxxxx 10xxxxxx
   11110xxx 10xxxxxx 10xxxxxx 10xxxxxx

   Do we really need to handle this? Internally we are only 
   handling ascii characters (e.g. '<', '>', '/', '?' etc.)
   thus it's only important to be able to skip utf8 characters
   correctly. Since a subbyte of utf8 never equals a ascii character
   it should be possible to parse the file correctly when ignoring utf8
*/

/* TODO: this needs to be updated to cope with utf8 */
inline char current_char(qnd_xml_context_t *context) {
  return *context->cur;
}

/* TODO: this needs to be updated to cope with utf8 */
inline gboolean skip_char(qnd_xml_context_t *context) {
  context->cur++;
  /* TODO: check buffer range */
  return TRUE;
}

gboolean skip_to_char(qnd_xml_context_t *context, char *chrs) {
  do {
    while(context->cur < context->buffer + context->fill) {
      if(strchr(chrs, current_char(context))) {
	return skip_char(context);
      }
      if(!skip_char(context)) return FALSE;
    }

    /* try to get more data */
    if(!update_buffer(context))
      return FALSE;

  } while(context->fill);

  /* if we get here the system was unable to fill the buffer */
  return FALSE;
}

gboolean buffer_overflow(qnd_xml_context_t *context) {
  return(!(context->cur < context->buffer + context->fill));
}

gboolean get_element_name(qnd_xml_context_t *context) {

  /* drop everything before element from buffer */
  if(!update_buffer(context)) return FALSE;

  char *start = context->cur;

  if(buffer_overflow(context) || !isalpha(current_char(context))) {
    printf("invalid element name #1 (%c)\n", current_char(context));
    return FALSE;
  }

  while(!buffer_overflow(context) && !isblank(current_char(context)) &&
	(current_char(context) != '>')) {
    if(!isalnum(current_char(context))) {
      printf("invalid element name #2 (%c)\n", current_char(context));
      return FALSE;
    }
    if(!skip_char(context)) return FALSE;
  }

#if 0
  char *format = g_strdup_printf("Element name = %%.%ds\n", 
				 context->cur-start);
  printf(format, start);
  g_free(format);
#endif  

  /* handle special elements locally */
  if(context->mod) {

  } else {
    qnd_xml_entry_t *entry = context->sp->entry, *hit = NULL;

    int i=0;
    for(i=0;!hit && i<entry->num_children;i++) 
      if(strncmp(entry->children[i]->name, start, 
		 strlen(entry->children[i]->name)) == 0) 
	hit = entry->children[i];

    if(hit) 
      stack_push(context, hit);
    else {
      printf("element search failed\n");
      return FALSE;
    }
  }

  return TRUE;
}

gboolean get_attribute_name(qnd_xml_context_t *context) {

  char *start = context->cur;

  if(buffer_overflow(context) || !isalpha(current_char(context))) {
    printf("invalid attribute name\n");
    return FALSE;
  }

  while(!buffer_overflow(context) && !isblank(current_char(context)) && 
	!(current_char(context) == '=')) {
    if(!isalnum(current_char(context))) {
      printf("invalid attribute name\n");
      return FALSE;
    }
    if(!skip_char(context)) return FALSE;
  }

  /* attach a new attribute to chain */
  qnd_xml_attribute_t **attr = &context->attributes;
  while(*attr) attr = &(*attr)->next;
  
  /* terminate name at closing '=' */
  *context->cur = '\0';

  *attr = g_new0(qnd_xml_attribute_t, 1);
  (*attr)->name = start;

  return TRUE;
}

gboolean get_attribute_value(qnd_xml_context_t *context) {

  char *start = context->cur;

  while(!buffer_overflow(context) && !(current_char(context) == '\"')) 
    if(!skip_char(context)) return FALSE;

  /* attach a new attribute to chain */
  qnd_xml_attribute_t **attr = &context->attributes;
  while((*attr) && (*attr)->next) attr = &(*attr)->next;
  
  if(!(*attr) || (*attr)->value) {
    printf("error storing attribute value\n");
    return FALSE;
  }

  /* terminate value at closing '\"' */
  *context->cur = '\0';
  (*attr)->value = start;

  return TRUE;
}

gboolean skip_white(qnd_xml_context_t *context) {
  /* skip all white space */
  while(!buffer_overflow(context) && isblank(current_char(context))) 
    if(!skip_char(context)) return FALSE;

  if(isblank(current_char(context))) {
    printf("error skipping white space\n");
    return FALSE;
  }

  return TRUE;
}

gboolean get_attributes(qnd_xml_context_t *context) {
  /* drop everything before element from buffer */

  if(!update_buffer(context)) return FALSE;
  if(!skip_white(context)) return FALSE;

  while(isalpha(current_char(context))) {

    /* get attribute name */
    if(!get_attribute_name(context)) return FALSE;

    if(!skip_to_char(context, "=")) return FALSE;
    if(!skip_to_char(context, "\"")) return FALSE;

    if(!get_attribute_value(context)) return FALSE;
    if(!skip_to_char(context, "\"")) return FALSE;

    if(!skip_white(context)) return FALSE;
  }
  return TRUE;
}

void attributes_free(qnd_xml_context_t *context) {
  qnd_xml_attribute_t *attr = context->attributes;

  while(attr) {
    qnd_xml_attribute_t *next = attr->next;
    g_free(attr);
    attr = next;
  }

  context->attributes = NULL;
}

void qnd_xml_cleanup(qnd_xml_context_t *context) {
  /* todo: clean stack */

  if(context->file) fclose(context->file);
  g_free(context);
}

gboolean get_element(qnd_xml_context_t *context) {

  /* skip all text */
  if(!skip_to_char(context, "<")) return FALSE;

  /* handle optional modifier */
  if(current_char(context) == '?' || current_char(context) == '!') {
    context->mod = current_char(context);
    if(!skip_char(context)) return FALSE;
  } else
    context->mod = 0;

  /* check for closing element */
  if(current_char(context) == '/') {
    context->mod = '/';
    if(!skip_char(context)) return FALSE;
  }

  if(!get_element_name(context)) return FALSE;
  if(!get_attributes(context)) return FALSE;

  if(context->mod && context->mod != '/') {
    if(current_char(context) != context->mod) {
      printf("modifier mismatch\n");
      return FALSE;
    }
    
    /* skip the modifier */
    if(!skip_char(context)) return FALSE;  
  } 

  if(!skip_white(context)) return FALSE;

  /* call callback now since the entry may be taken from stack */
  if(!context->mod && context->sp->entry->cb) 
    if(!context->sp->entry->cb(context->sp, 
			       context->attributes, context->userdata))
      return FALSE;

  if(context->mod == '/') 
    stack_pop(context);
  else {
    /* if this element closes here it's cleaned up immediately */
    if(current_char(context) == '/') {
      if(!skip_char(context)) return FALSE;
      stack_pop(context);
    }
  }

  if(current_char(context) != '>') {
    printf("element closing error\n");
    return FALSE;
  }

  if(!skip_char(context)) return FALSE;

  attributes_free(context);

  return TRUE;
}

gpointer qnd_xml_parse(char *name, qnd_xml_entry_t *root, gpointer userdata) {
  qnd_xml_context_t *context = g_new0(qnd_xml_context_t, 1);
  context->cur = context->buffer;
  context->userdata = userdata;

  /* init stack by adding root entry */
  context->sp = context->stack = g_new0(qnd_xml_stack_t, 1);
  context->sp->entry = root;

  /* check if file exists and is a regular file */
  if(!g_file_test(name, G_FILE_TEST_IS_REGULAR)) {
    printf("file doesn't exist or is not a regular file\n");
    qnd_xml_cleanup(context);
    return FALSE;
  }

  /* open file */
  context->file = g_fopen(name, "r");
  if(!context->file) { 
    printf("unable to open file\n");
    qnd_xml_cleanup(context);
    return FALSE;
  }

  printf("file is open\n");

  /* get file length */
  fseek(context->file, 0l, SEEK_END);
  context->total = ftell(context->file);
  fseek(context->file, 0l, SEEK_SET);
  
  printf("file length is %d bytes\n", context->total);

  gboolean error = FALSE;
  do
    error = !get_element(context);
  while(!error && !context->done);

  if(error) printf("parser ended with error\n");
  else      printf("parser ended successfully\n");

  printf("current bytes read: %d of %d\n", 
	 context->bytes_read, context->total);
  printf("current buffer offset: %d\n", context->cur - context->buffer);

  /* user pointer[0] of root element is retval */
  gpointer retval = error?NULL:context->stack->userdata[0];

  /* close file and cleanup */
  qnd_xml_cleanup(context);

  return retval;
}

char *qnd_xml_get_prop(qnd_xml_attribute_t *attr, char *name) {
  while(attr) {
    if(strcasecmp(name, attr->name) == 0) 
      return attr->value;
    
    attr = attr->next;
  }
  return NULL;
}

char *qnd_xml_get_prop_str(qnd_xml_attribute_t *attr, char *name) {
  char *value = qnd_xml_get_prop(attr, name);
  if(value) return g_strdup(value);
  return NULL;
}

gboolean qnd_xml_get_prop_double(qnd_xml_attribute_t *attr, char *name,
				 double *dest) {
  char *value = qnd_xml_get_prop(attr, name);
  if(!value) return FALSE;

  *dest = g_ascii_strtod(value, NULL);
  return TRUE;
}

gboolean qnd_xml_get_prop_gulong(qnd_xml_attribute_t *attr, char *name,
			      gulong *dest) {
  char *value = qnd_xml_get_prop(attr, name);
  if(!value) return FALSE;

  *dest = strtoul(value, NULL, 10);
  return TRUE;
}

gboolean qnd_xml_get_prop_is(qnd_xml_attribute_t *attr, char *name,
			     char *ref) {
  char *value = qnd_xml_get_prop(attr, name);
  if(!value) return FALSE;

  return g_strcasecmp(ref, value);
}
