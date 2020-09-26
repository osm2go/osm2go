/*
 * SPDX-FileCopyrightText: 2019 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "cache_set.h"

struct relation_object_replacer {
  const object_t &old;
  const object_t &replace;
  relation_object_replacer(const object_t &t, const object_t &n) : old(t), replace(n) {}
  inline void operator()(std::pair<item_id_t, relation_t *> pair)
  { operator()(pair.second); }
  void operator()(relation_t *r);
};

struct find_member_object_functor {
  const object_t &object;
  explicit inline find_member_object_functor(const object_t &o) : object(o) {}
  bool operator()(const member_t &member) {
    return member.object == object;
  }
};

extern cache_set value_cache; ///< the cache for key, value, and role strings
