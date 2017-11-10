/*
 * Copyright (C) 2017 Rolf Eike Beer <eike@sf-mail.de>.
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

#include "osm2go_annotations.h"

#include "josm_presets_p.h"
#include "osm.h"
#include "pos.h"

#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <iomanip>
#include <iostream>

void
assert_msg_fmt(const char *file, const int line, const char *func, const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  fprintf(stderr, "%s:%i: %s: ", file, line, func);
  vfprintf(stderr, fmt, args);
  va_end(args);

  fprintf(stderr, "\n");
  abort();
}

static void
heremsg(const char *file, const int line, const char *func)
{
  std::cerr << "code at " << file << ":" << line << ":" << func << " ";
}

void
assert_msg_unreachable(const char *file, const int line, const char *func)
{
  heremsg(file, line, func);
  std::cerr << "should not be reachable" << std::endl;
  abort();
}

static void
nummsg(const char *amsg, const char *opmsg, const char *bmsg)
{
  std::cerr << amsg << " " << opmsg << " " << bmsg << " failed: " << amsg << ": ";
}

template<typename T>
void hexmsg(T a)
{
  std::cerr << std::dec << a << " (0x" << std::hex << a << ")";
}

template<typename T> assert_num_tpl<T>::assert_num_tpl(T a, const char *amsg, const char *opmsg,
                                                       const char *bmsg, const char *file,
                                                       const char *func, int line)
{
  heremsg(file, line, func);
  nummsg(amsg, opmsg, bmsg);
  hexmsg(a);
  std::cerr << std::endl;
  abort();
}

template<typename T> assert_num_tpl<T>::assert_num_tpl(T a, T b, const char *amsg,
                                                       const char *opmsg, const char *bmsg,
                                                       const char *file, const char *func, int line)
{
  heremsg(file, line, func);
  nummsg(amsg, opmsg, bmsg);
  hexmsg(a);
  std::cerr << " " << bmsg << ": ";
  hexmsg(b);
  std::cerr << std::endl;
  abort();
}

// the template as typeof(ca) intentionally so the type is always "const x" here,
// otherwise the instances would have to be duplicated for "x" and "const x"

template class assert_num_tpl<const unsigned long>;
template class assert_num_tpl<const unsigned int>;
template class assert_num_tpl<const int>;
template class assert_num_tpl<const type_t>;
template class assert_num_tpl<const item_id_t>;
template class assert_num_tpl<const osm_t::UploadPolicy>;
template class assert_num_tpl<const presets_widget_type_t>;
template class assert_num_tpl<const char>;

template<> assert_num_tpl<const pos_float_t>::assert_num_tpl(pos_float_t a, const char *amsg,
                                                             const char *opmsg, const char *bmsg,
                                                             const char *file, const char *func, int line)
{
  heremsg(file, line, func);
  nummsg(amsg, opmsg, bmsg);
  std::cerr << std::setprecision(9) << a << std::endl;
  abort();
}

template<> assert_num_tpl<const pos_float_t>::assert_num_tpl(pos_float_t a, pos_float_t b,
                                                             const char *amsg, const char *opmsg, const char *bmsg,
                                                             const char *file, const char *func, int line)
{
  heremsg(file, line, func);
  nummsg(amsg, opmsg, bmsg);
  std::cerr << std::setprecision(9) << a << ", "  << bmsg << ": " << b
            << std::endl;
  abort();
}

template<> assert_num_tpl<const float_t>::assert_num_tpl(float_t a, const char *amsg,
                                                         const char *opmsg, const char *bmsg,
                                                         const char *file, const char *func, int line)
{
  assert_num_tpl<pos_float_t>(a, amsg, opmsg, bmsg, file, func, line);
  abort();
}

template<> assert_num_tpl<const float_t>::assert_num_tpl(float_t a, float_t b,
                                                         const char *amsg, const char *opmsg, const char *bmsg,
                                                         const char *file, const char *func, int line)
{
  assert_num_tpl<pos_float_t>(a, b, amsg, opmsg, bmsg, file, func, line);
  abort();
}

void assert_cmpstr_struct::fail(const char *a, const char *astr, const char *b, const char *file, const char *func, int line) {
  assert_msg_fmt(file, line, func, "%s == \"%s\" failed: %s: '%s'", astr, b, astr, a);
}

void assert_cmpstr_struct::fail(const char *a, const char *astr, const char *b, const char *bstr, const char *file, const char *func, int line) {
  assert_msg_fmt(file, line, func, "%s == %s failed: %s: '%s', %s: '%s'", astr, bstr, astr, a, bstr, b);
}
