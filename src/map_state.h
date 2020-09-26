/*
 * SPDX-FileCopyrightText: 2008 Till Harbaum <till@harbaum.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <osm2go_platform.h>

struct map_state_t {
  map_state_t() noexcept;

  void reset() noexcept;

  float zoom;                          // zoom level (1.0 = 1m/pixel
  float detail;                        // detail level (1.0 = normal)
  osm2go_platform::screenpos scroll_offset; // initial scroll offset
};
