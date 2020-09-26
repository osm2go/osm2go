/*
 * SPDX-FileCopyrightText: 2017 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <osm2go_cpp.h>
#include <osm2go_i18n.h>
#include <osm2go_stl.h>

#include <cassert>
#include <cstring>
#include <string>
#include <type_traits>

// this omits magic to support every thinkable compiler until anyone really needs them

#define likely(x)       __builtin_expect(static_cast<bool>(x), 1)
#define unlikely(x)     __builtin_expect(static_cast<bool>(x), 0)

#ifdef __has_attribute
#if __has_attribute(cold)
#define ATTRIBUTE_COLD __attribute__((cold))
#endif
#endif

#ifndef ATTRIBUTE_COLD
#ifdef __GNUC_PREREQ
#if __GNUC_PREREQ(4, 3)
#define ATTRIBUTE_COLD __attribute__((cold))
#else
#define ATTRIBUTE_COLD
#endif
#else
#define ATTRIBUTE_COLD
#endif
#endif

void __attribute__((format (printf, 4, 5))) __attribute__((noreturn)) ATTRIBUTE_COLD
assert_msg_fmt(const char *file, const int line, const char *func, const char *fmt, ...);

void __attribute__((noreturn)) ATTRIBUTE_COLD
assert_msg_unreachable(const char *file, const int line, const char *func);

#define ASSERT_MSG_FMT(fmt, a, b) assert_msg_fmt(__FILE__, __LINE__, __PRETTY_FUNCTION__, fmt, a, b)

#define assert_null(x) \
       do { \
         const void *p = x; \
         if (unlikely(p != nullptr)) \
           ASSERT_MSG_FMT("'%s' should be nullptr, but is %p", #x, p); \
       } while(0)

template<typename T> class assert_num_tpl {
public:
  // version for dynamic a and b
  __attribute__((noreturn)) ATTRIBUTE_COLD
  assert_num_tpl(T a, T b, const char *amsg, const char *opmsg, const char *bmsg, const char *file, const char *func, int line);
  // version for constant b
  __attribute__((noreturn)) ATTRIBUTE_COLD
  assert_num_tpl(T a, const char *amsg, const char *opmsg, const char *bmsg, const char *file, const char *func, int line);
};

#define assert_cmpnum(a, b) assert_cmpnum_op(a, ==, b)

#define assert_cmpnum_op(a, op, b) \
       do { \
         const typeof(a) &ca = a; \
         const typeof(b) &cb = b; \
         if (unlikely(!(ca op static_cast<std::remove_const<typeof(a)>::type>(cb)))) { \
           __builtin_constant_p(b) ? \
             assert_num_tpl<typeof(ca)>(ca,     #a, #op, #b, __FILE__, __PRETTY_FUNCTION__, __LINE__) : \
             assert_num_tpl<typeof(ca)>(ca, cb, #a, #op, #b, __FILE__, __PRETTY_FUNCTION__, __LINE__); \
         } \
       } while (0)

class assert_cmpstr_struct {
public:
  inline assert_cmpstr_struct(const std::string &a, const char *astr, const char *b, const char *file, const char *func, int line) {
    if(unlikely(a != b))
      fail(a.c_str(), astr, b, file, func, line);
  }
  inline assert_cmpstr_struct(const char *a, const char *astr, const char *b, const char *file, const char *func, int line) {
    assert(a != nullptr);
    assert(b != nullptr);
    if(unlikely(strcmp(a, b) != 0))
      fail(a, astr, b, file, func, line);
  }
  assert_cmpstr_struct(const std::string &a, const char *astr, const char *b, const char *bstr, const char *file, const char *func, int line) {
    if(unlikely(a != b))
      fail(a.c_str(), astr, b, bstr, file, func, line);
  }
  assert_cmpstr_struct(const char *a, const char *astr, const char *b, const char *bstr, const char *file, const char *func, int line) {
    assert(a != nullptr);
    assert(b != nullptr);
    if(unlikely(strcmp(a, b) != 0))
      fail(a, astr, b, bstr, file, func, line);
  }
  // not inline, should happen only in test code
  assert_cmpstr_struct(trstring::arg_type a, const char *astr, trstring::arg_type b, const char *file, const char *func, int line);
  assert_cmpstr_struct(trstring::arg_type a, const char *astr, trstring::arg_type b, const char *bstr, const char *file, const char *func, int line);

  assert_cmpstr_struct(trstring::arg_type a, const char *astr, const char *b, const char *file, const char *func, int line);
  assert_cmpstr_struct(trstring::arg_type a, const char *astr, const char *b, const char *bstr, const char *file, const char *func, int line);

  assert_cmpstr_struct(trstring::arg_type a, const char *astr, const std::string &b, const char *file, const char *func, int line);
  assert_cmpstr_struct(trstring::arg_type a, const char *astr, const std::string &b, const char *bstr, const char *file, const char *func, int line);

#ifndef TRSTRING_NATIVE_TYPE_IS_TRSTRING
  // assist in overload resolution
  inline assert_cmpstr_struct(const trstring &a, const char *astr, trstring::native_type_arg b, const char *file, const char *func, int line)
  {
    assert_cmpstr_struct relay(trstring::arg_type(a), astr, trstring::arg_type(b), file, func, line);
  }
  inline assert_cmpstr_struct(const trstring &a, const char *astr, trstring::native_type_arg b, const char *bstr, const char *file, const char *func, int line)
  {
    assert_cmpstr_struct relay(trstring::arg_type(a), astr, trstring::arg_type(b), bstr, file, func, line);
  }
  inline assert_cmpstr_struct(trstring::native_type_arg a, const char *astr, trstring::native_type_arg b, const char *file, const char *func, int line)
  {
    assert_cmpstr_struct relay(trstring::arg_type(a), astr, trstring::arg_type(b), file, func, line);
  }
  inline assert_cmpstr_struct(trstring::native_type_arg a, const char *astr, trstring::native_type_arg b, const char *bstr, const char *file, const char *func, int line)
  {
    assert_cmpstr_struct relay(trstring::arg_type(a), astr, trstring::arg_type(b), bstr, file, func, line);
  }
  inline assert_cmpstr_struct(const trstring &a, const char *astr, const char *b, const char *file, const char *func, int line)
  {
    assert_cmpstr_struct relay(trstring::arg_type(a), astr, b, file, func, line);
  }
  inline assert_cmpstr_struct(const trstring &a, const char *astr, const char *b, const char *bstr, const char *file, const char *func, int line)
  {
    assert_cmpstr_struct relay(trstring::arg_type(a), astr, b, bstr, file, func, line);
  }
  inline assert_cmpstr_struct(const trstring &a, const char *astr, const std::string &b, const char *file, const char *func, int line)
  {
    assert_cmpstr_struct relay(trstring::arg_type(a), astr, b.c_str(), file, func, line);
  }
  inline assert_cmpstr_struct(const trstring &a, const char *astr, const std::string &b, const char *bstr, const char *file, const char *func, int line)
  {
    assert_cmpstr_struct relay(trstring::arg_type(a), astr, b.c_str(), bstr, file, func, line);
  }
  inline assert_cmpstr_struct(const trstring &a, const char *astr, const trstring &b, const char *file, const char *func, int line)
  {
    assert_cmpstr_struct relay(trstring::arg_type(a), astr, trstring::arg_type(b), file, func, line);
  }
  inline assert_cmpstr_struct(const trstring &a, const char *astr, const trstring &b, const char *bstr, const char *file, const char *func, int line)
  {
    assert_cmpstr_struct relay(trstring::arg_type(a), astr, trstring::arg_type(b), bstr, file, func, line);
  }
#endif
  assert_cmpstr_struct(const std::string &a, const char *astr, const std::string &b, const char *file, const char *func, int line);
  assert_cmpstr_struct(const std::string &a, const char *astr, const std::string &b, const char *bstr, const char *file, const char *func, int line);
  assert_cmpstr_struct(const std::string &a, const char *astr, const trstring &b, const char *file, const char *func, int line);
  assert_cmpstr_struct(const std::string &a, const char *astr, const trstring &b, const char *bstr, const char *file, const char *func, int line);

#if __cplusplus >= 201103L
  // catch if one passes a constant nullptr as second argument
  assert_cmpstr_struct(const std::string &a, const char *astr, std::nullptr_t n, const char *file, const char *func, int line) = delete;
  assert_cmpstr_struct(const char *a, const char *astr, std::nullptr_t n, const char *bstr, const char *file, const char *func, int line) = delete;
#endif

private:
  __attribute__((noreturn)) ATTRIBUTE_COLD void fail(const char *a, const char *astr, const char *b, const char *file, const char *func, int line);
  __attribute__((noreturn)) ATTRIBUTE_COLD void fail(const char *a, const char *astr, const char *b, const char *bstr, const char *file, const char *func, int line);
};

#define assert_cmpstr(a, b) \
         do { \
           __builtin_constant_p(b) ? \
             assert_cmpstr_struct(a, #a, b, __FILE__, __PRETTY_FUNCTION__, __LINE__) : \
             assert_cmpstr_struct(a, #a, b, #b, __FILE__, __PRETTY_FUNCTION__, __LINE__); \
         } while (0)

#define assert_cmpmem(p1, l1, p2, l2) \
       do { \
         const void *q1 = p1, *q2 = p2; \
         const size_t __l1 = l1, __l2 = l2; \
         if (__l1 != __l2) \
           ASSERT_MSG_FMT(#l1 " (len(" #p1 ")) == " #l2 " (len(" #p2 ")) %zu == %zu", __l1, __l2); \
         else if (__l1 != 0 && memcmp (q1, q2, __l1) != 0) \
           assert(p1 == p2); \
       } while (0)

#define assert_unreachable() \
       assert_msg_unreachable(__FILE__, __LINE__, __PRETTY_FUNCTION__);
