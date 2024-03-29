/*
 * SPDX-FileCopyrightText: 2017 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "osm2go_annotations.h"

#include "color.h"
#include "josm_presets_p.h"
#include "osm.h"
#include "pos.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <iostream>

#include <osm2go_i18n.h>

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

namespace {

void
heremsg(const char *file, const int line, const char *func)
{
  std::cerr << "code at: " << file << ":" << line << ": " << func << ": ";
}

void
nummsg(const char *amsg, const char *opmsg, const char *bmsg)
{
  std::cerr << "Assertion " << amsg << " " << opmsg << " " << bmsg << " failed: " << amsg << ": ";
}

template<typename T>
void hexmsg(T a)
{
  std::cerr << std::dec << a << " (0x" << std::hex << a << ")";
}

} // namespace

void
assert_msg_unreachable(const char *file, const int line, const char *func)
{
  heremsg(file, line, func);
  std::cerr << "should not be reachable" << std::endl;
  abort();
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

// the template has typeof(ca) intentionally so the type is always "const x" here,
// otherwise the instances would have to be duplicated for "x" and "const x"

template class assert_num_tpl<const unsigned long>;
template class assert_num_tpl<const unsigned int>;
template class assert_num_tpl<const color_t>;
template class assert_num_tpl<const int>;
template class assert_num_tpl<const object_t::type_t>;
template class assert_num_tpl<const item_id_t>;
template class assert_num_tpl<const osm_t::UploadPolicy>;
template class assert_num_tpl<const presets_element_type_t>;
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

assert_cmpstr_struct::assert_cmpstr_struct(const std::string &a, const char *astr, const std::string &b, const char *bstr, const char *file, const char *func, int line)
{
  if(unlikely(a != b))
    fail(a.c_str(), astr, b.c_str(), bstr, file, func, line);
}

void assert_cmpstr_struct::fail(const char *a, const char *astr, const char *b, const char *file, const char *func, int line) {
  assert_msg_fmt(file, line, func, "%s == \"%s\" failed: %s: '%s'", astr, b, astr, a);
}

void assert_cmpstr_struct::fail(const char *a, const char *astr, const char *b, const char *bstr, const char *file, const char *func, int line) {
  assert_msg_fmt(file, line, func, "%s == %s failed: %s: '%s', %s: '%s'", astr, bstr, astr, a, bstr, b);
}
