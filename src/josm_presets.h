/*
 * SPDX-FileCopyrightText: 2008 Till Harbaum <till@harbaum.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <set>
#include <string>

class object_t;
class relation_t;

class presets_items {
protected:
  presets_items() {}
public:
  virtual ~presets_items() {}

  static presets_items *load(void);

  /**
   * @brief collect the roles suggested by presets for the given object in the given relation
   */
  virtual std::set<std::string> roles(const relation_t *relation, const object_t &obj) const = 0;
};

std::string josm_icon_name_adjust(const char *name) __attribute__((nonnull(1)));
