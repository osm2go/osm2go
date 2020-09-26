/*
 * SPDX-FileCopyrightText: 2008 Till Harbaum <till@harbaum.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <string>

#include <osm2go_platform.h>

class icon_item {
protected:
  inline icon_item() {}
public:
  int maxDimension() const;
};

class icon_t {
protected:
  inline icon_t() {}
public:
  static icon_t &instance();
  ~icon_t();

  /**
   * @brief load an icon from disk, limited to given dimensions
   * @param sname the name of the icon, must not be empty
   * @param limit the maximum dimensions of the image
   * @return the scaled pixbuf
   *
   * The image is only scaled down to the given dimensions, not enlarged.
   * The limit is only applied if the icon is not already cached.
   */
  icon_item *load(const std::string &sname, int limit = -1);

  osm2go_platform::Widget *widget_load(const std::string &name, int limit = -1);

  void icon_free(icon_item *buf);
};
