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
 * along with OSM2Go.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <cassert>
#include <locale.h>
#include <libintl.h>
#include <string>

#include <osm2go_stl.h>

typedef char gchar;

#define _(String) trstring::tr(String)

extern const char *bad_tr_call; // never defined, so bad calls to tr_noop() can be detected
// #define _(String) trstring::tr(String)
#define tr_noop(String) __builtin_constant_p(String) ? \
                  (String) : bad_tr_call

class trstring : private std::string {
#if __cplusplus >= 201103L
  explicit inline trstring(std::string &&s) : std::string(std::move(s)) {}
#else
  explicit inline trstring(const std::string &s) : std::string(s) {}
#endif
  trstring argFloatHelper(double a) const;
public:
  // use this type directly only when declaring variables, not arguments
  class native_type {
    const char *value;
  public:
#if __cplusplus >= 201103L
    // catch if one passes a constant nullptr as argument
    native_type(std::nullptr_t) = delete;
    inline native_type(native_type &&) = default;
    inline native_type &operator=(native_type &&) = default;
#endif
    inline native_type(const native_type &other) : value(other.value) {}
    inline native_type &operator=(const native_type &other) { value = other.value; return *this; }
    inline native_type(const char *v = nullptr) : value(v) {}
    inline bool isEmpty() const { return value == nullptr; }
    inline void clear() { value = nullptr; }
    inline operator const char *() const { return value; }
    inline std::string toStdString() const { return isEmpty() ? std::string() : value; }
  };
  // exclusively use this type in function interfaces
  // think of it being "const native_type &", it is not because just copying one pointer is cheaper
  typedef native_type native_type_arg;
#undef TRSTRING_NATIVE_TYPE_IS_TRSTRING

  class any_type {
    friend class trstring;

    const trstring *m_t;
    native_type m_n;
  public:
    inline any_type() : m_t(nullptr) {}
    inline any_type(native_type a) : m_t(nullptr), m_n(a) {}
    inline any_type(const trstring &a) : m_t(&a) {}
    inline any_type(const any_type &other) : m_t(other.m_t), m_n(other.m_n) {}
    inline any_type &operator=(const any_type &other)
    { m_t = other.m_t; m_n = other.m_n; return *this; }

#if __cplusplus >= 201103L
    explicit
#endif
    inline operator native_type() const
    {
      if (m_t != nullptr)
        return m_t->toStdString().c_str();
      else
        return m_n;
    }

    inline bool isEmpty() const
    {
      return m_t != nullptr ? m_t->isEmpty() : m_n.isEmpty();
    }
  };

  typedef const any_type &arg_type;

  explicit inline trstring() : std::string() {}
  explicit inline trstring(const char *s) __attribute__((nonnull(2))) : std::string(gettext(s)) {}
  explicit inline trstring(native_type s) : std::string(static_cast<const gchar *>(s)) {}
#if __cplusplus >= 201103L
  // catch if one passes a constant nullptr as argument
  trstring(std::nullptr_t) = delete;
  trstring(std::nullptr_t, const char *, int) = delete;
  trstring arg(std::nullptr_t) = delete;
#endif
  trstring(const char *msg, const char *, int n) __attribute__((nonnull(2)));

  trstring arg(const std::string &a) const;
  trstring arg(const char *a) const __attribute__((nonnull(2)));
  inline trstring arg(char *a) const __attribute__((nonnull(2)))
  { return arg(static_cast<const char *>(a)); }
  inline trstring arg(const trstring &a) const
  { return arg(static_cast<std::string>(a)); }
  inline trstring arg(native_type a) const
  {
    assert(!a.isEmpty());
    return arg(static_cast<const char *>(a));
  }
  template<typename T> inline trstring arg(T l) const
  { return arg(std::to_string(l)); }

  inline trstring arg(double a, int fieldWidth = 0, char format = 'g', int precision = -1) const
  {
    // I only need this at a single place, so simplify the implementation...
    assert(fieldWidth == 0);
    assert(format == 'f');
    assert(precision == 2);
    return argFloatHelper(a);
  }

  const std::string &toStdString() const { return *this; }

  inline trstring arg(any_type a) const
  {
    if (a.m_t)
      return arg(*a.m_t);
    else
      return arg(static_cast<const gchar *>(a.m_n));
  }

  inline void assign(std::string other)
  {
    std::string::swap(other);
  }

  inline void swap(trstring &other)
  {
    std::string::swap(other);
  }

  inline bool isEmpty() const
  { return empty(); }

  // this is a helper method to implement _(), do not call it directly
  static inline native_type tr(const char *s) __attribute__((nonnull(1)))
  {
    return native_type(gettext(s));
  }

  // There is intentionally no c_str() here as it would too easily be used in generic code,
  // instead there is a cast to a type that tells everyone "hey, this is glib specific".
#if __cplusplus >= 201103L
  explicit
#endif
  operator const gchar *() const { return c_str(); }
};

static_assert(sizeof(trstring::native_type) <= sizeof(char*), "trstring::native_type is too big");
