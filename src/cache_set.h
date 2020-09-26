/*
 * SPDX-FileCopyrightText: 2018 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <cstring>
#include <unordered_set>

#include <osm2go_annotations.h>
#include <osm2go_cpp.h>

/**
 * @brief a cache wrapping an unordered_set for caching strings
 *
 * The main purpose is to avoid the allocation of a temporary std::string
 * instance for lookups, otherwise std::unordered_set<std::string> could be
 * used.
 */
class cache_set {
  struct values_equal {
    bool operator()(const char *a, const char *b) const noexcept
    {
      return a == b || strcmp(a, b) == 0;
    }
  };

  /**
   * @brief build a hash about a character sequence
   *
   * One can't simply use std::hash, as that hashes the pointer, not the
   * contents behind it. All implementations have an internal helper that takes
   * a buffer and a length (used e.g. for std::hash<std::string>), but the name
   * of this differs between implementations.
   */
  struct str_hash {
    size_t operator()(const char *s) const noexcept
    {
      const size_t l = strlen(s);
      size_t ret = 0;
      if (l <= sizeof(size_t)) {
        memcpy(&ret, s, l);
      } else {
        // use both start and end of the string in the hash to avoid too many hash
        // collisions when values have a common prefix
        memcpy(&ret, s, sizeof(size_t) / 2);
        memcpy(reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(&ret) + 2),
               s + l - sizeof(size_t) / 2, sizeof(size_t) / 2);
        ret ^= l;
      }
      return ret;
    }
  };

  typedef std::unordered_set<const char *, str_hash, values_equal> content_type;
  content_type contents;

  static inline void value_free(const char *m)
  {
    free(const_cast<char *>(m));
  }

  /**
   * @brief do the actual insertion
   * @param value a valid element (i.e. not empty)
   */
  const char *inner_insert(const char *value)
  {
    content_type::const_iterator it = contents.find(value);
    if (unlikely(it == contents.end()))
      it = contents.insert(strdup(value)).first;

    return *it;
  }

public:
  ~cache_set()
  {
    // free the values behind the keys
    std::for_each(contents.begin(), contents.end(), value_free);
  }

  const char *insert(const char *value)
  {
    if (unlikely(value == nullptr))
      return nullptr;

    return inner_insert(value);
  }

  inline const char *insert(const std::string &value)
  {
    if (unlikely(value.empty()))
      return nullptr;

    return inner_insert(value.c_str());
  }

  const char *getValue(const char *value) const
  {
    const content_type::const_iterator it = contents.find(value);
    if (unlikely(it == contents.end()))
      return nullptr;

    return *it;
  }
};
