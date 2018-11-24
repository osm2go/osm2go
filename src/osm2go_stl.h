/*
 * Copyright (C) 2017-2018 Rolf Eike Beer <eike@sf-mail.de>.
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

#include <string>

#include <osm2go_cpp.h>

// some compat code

#if __cplusplus < 201402L
namespace std {
template<typename T> inline typename T::const_iterator cbegin(const T &c) {
#if __cplusplus >= 201103L
  return c.cbegin();
#else
  return typename T::const_iterator(c.begin());
#endif
}
}
#endif

// gcc did not set the C++ level before it was officially released
#if __cplusplus < 201103L && !defined(_UNIQUE_PTR_H)
// taken from gcc 4.8.5, stripped down to what is actually needed
namespace std {
  template<typename T>
  struct default_delete {
    void operator()(T *p) const {
      delete p;
    }
  };

  template <typename _Tp, typename _Dp = default_delete<_Tp> >
    class unique_ptr {
    public:
      typedef _Tp                       element_type;
      typedef _Dp                       deleter_type;

      _Tp *_M_t;

      // Constructors.
      explicit unique_ptr(_Tp *__p = nullptr)
      : _M_t(__p) { }

      // Destructor.
      ~unique_ptr() {
        _Tp *&__ptr = _M_t;
        if (__ptr != nullptr)
          _Dp()(__ptr);
        __ptr = nullptr;
      }

      // Observers.
      // be careful when creating instances where _TP is void, this breaks
      // on gcc 4.2 (i.e. N900)
      _Tp &operator*() const {
        return *get();
      }

      _Tp *operator->() const {
        return get();
      }

      _Tp *get() const
      { return _M_t; }

      operator bool() const
      { return get() == nullptr ? false : true; }

      // Modifiers.
      _Tp *release() {
        _Tp *__p = get();
        _M_t = nullptr;
        return __p;
      }

      void reset(_Tp *__p = nullptr) {
        using std::swap;
        swap(_M_t, __p);
        if (__p != nullptr)
          _Dp()(__p);
      }

      /// Exchange the pointer and deleter with another object.
      void
      swap(unique_ptr& __u)
      {
        using std::swap;
        swap(_M_t, __u._M_t);
      }

    private:
      // Disable copy from lvalue.
      unique_ptr(const unique_ptr &);
      unique_ptr& operator=(const unique_ptr &);
  };

  template<typename _Tp>
    class unique_ptr<_Tp[]>
    {
    public:
      typedef _Tp                       element_type;

      _Tp *                  _M_t;


      // Constructors.

      /** Takes ownership of a pointer.
       *
       * @param __p  A pointer to an array of a type safely convertible
       * to an array of @c element_type
       *
       * The deleter will be value-initialized.
       */
      explicit
      unique_ptr(_Tp *__p)
      : _M_t(__p)
        { }

      /// Destructor, invokes the deleter if the stored pointer is not null.
      ~unique_ptr()
      {
        _Tp *&__ptr = _M_t;
        if (__ptr != nullptr)
          delete[] (__ptr);
        __ptr = nullptr;
      }

      // Observers.

      /// Access an element of owned array.
      const element_type &
      operator[](size_t __i) const
      {
        return get()[__i];
      }

      element_type &
      operator[](size_t __i)
      {
        return get()[__i];
      }

      /// Return the stored pointer.
      element_type *
      get()
      { return _M_t; }

      const element_type *
      get() const
      { return _M_t; }

      /// Return @c true if the stored pointer is not null.
      operator bool() const
      { return get() == nullptr ? false : true; }

      // Modifiers.

      /// Release ownership of any stored pointer.
      element_type *
      release()
      {
        element_type * __p = get();
        _M_t._M_ptr() = nullptr;
        return __p;
      }

      /** @brief Replace the stored pointer.
       *
       * @param __p  The new pointer to store.
       *
       * The deleter will be invoked if a pointer is already owned.
       */
      void
      reset(_Tp *__p = nullptr)
      {
        element_type *__ptr = __p;
        using std::swap;
        swap(_M_t._M_ptr(), __ptr);
        if (__ptr != nullptr)
          delete[] (__ptr);
      }

      /// Exchange the pointer and deleter with another object.
      void
      swap(unique_ptr& __u)
      {
        using std::swap;
        swap(_M_t, __u._M_t);
      }

    private:
      // Disable copy from lvalue.
      unique_ptr(const unique_ptr&) O2G_DELETED_FUNCTION;
      unique_ptr& operator=(const unique_ptr&) O2G_DELETED_FUNCTION;
    };

  template<typename _Tp, typename _Dp>
    inline void
    swap(unique_ptr<_Tp, _Dp>& __x,
         unique_ptr<_Tp, _Dp>& __y) noexcept
    { __x.swap(__y); }

}
#endif

// gcc did not set the C++ level before it was officially released
#if __cplusplus < 201103L && !defined(_SHARED_PTR_H)
#include <tr1/memory>
namespace std {
  using tr1::shared_ptr;
  using tr1::weak_ptr;
}
#endif

template<typename T> void shrink_to_fit(T &v) {
#if __cplusplus >= 201103L
  v.shrink_to_fit();
#else
  // resize(size()) would only shrink to the next power of 2
  size_t sz = v.size();
  if(v.capacity() == sz)
    return;
  T tmp(sz);
  tmp = v;
  tmp.swap(v);
#endif
}

#if __cplusplus < 201103L
#include <type_traits>
namespace std {
  template<typename T, bool = std::is_integral<T>::value>
  struct to_string_tpl;

  template<typename T>
  struct to_string_tpl<T, true> {
    explicit inline to_string_tpl() {}
    std::string operator()(T n, const char *fmt)
    {
      char buf[sizeof(n) * 4];
      snprintf(buf, sizeof(buf), fmt, n);
      return buf;
    }
  };

  template<typename T>
  std::string to_string(T n)
  {
    return to_string_tpl<T>()(n, "");
  }

  template<> inline std::string to_string(int n)
  {
    return to_string_tpl<int>()(n, "%i");
  }

  template<> inline std::string to_string(long n)
  {
    return to_string_tpl<long>()(n, "%li");
  }

  template<> inline std::string to_string(long long n)
  {
    return to_string_tpl<long long>()(n, "%lli");
  }

  template<> inline std::string to_string(unsigned int n)
  {
    return to_string_tpl<unsigned int>()(n, "%u");
  }

  template<> inline std::string to_string(unsigned long n)
  {
    return to_string_tpl<unsigned long>()(n, "%lu");
  }

  template<> inline std::string to_string(unsigned long long n)
  {
    return to_string_tpl<unsigned long long>()(n, "%llu");
  }

  template<typename T>
  inline T &move(T &a) { return a; }
}
#endif

#if __cplusplus < 201703L
namespace std {
  template<typename T> class optional {
    const bool match;
    const T result;

  public:
    explicit inline optional() noexcept : match(false), result() {}
    inline optional(T r) : match(true), result(r) {}
    inline optional(const optional<T> &o) : match(o.match), result(o.result) {}

    inline operator bool() const noexcept { return match; }
    inline T operator *() const { return result; }
  };
}
#endif
