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

#ifndef COLOR_H
#define COLOR_H

#include <cstdint>

class color_t {
  uint32_t value;
public:
  inline color_t() {}
  inline color_t(uint8_t cr, uint8_t cg, uint8_t cb, uint8_t ca = 0xff)
  { value = ((static_cast<uint32_t>(cr) << 24) |
            (static_cast<uint32_t>(cg) << 16) |
            (static_cast<uint32_t>(cb) <<  8) | ca); }
  inline color_t(uint16_t cr, uint16_t cg, uint16_t cb, uint8_t ca = 0xff)
  { value = ((static_cast<uint32_t>(cr & 0xff00) << 16) |
            (static_cast<uint32_t>(cg & 0xff00) << 8)  |
            (static_cast<uint32_t>(cb & 0xff00)        | ca)); }
  inline color_t(unsigned int c) : value(c) {}
  inline color_t &operator=(unsigned int c) { value = c; return *this; }
#if __cplusplus >= 201103L
  inline color_t(const color_t &other) = default;
  inline color_t &operator=(const color_t &other) = default;
#else
  inline color_t(const color_t &other) : value(other.value) {}
  inline color_t &operator=(const color_t other) { value = other.value; return *this; }
#endif
  inline operator unsigned int() const { return value; }

  inline uint32_t rgba() const { return value; }
  inline unsigned int rgb() const { return value >> 8; }

  static inline color_t transparent()
  {
    color_t r;
    r.value = 0;
    return r;
  }
} __attribute__ ((packed));

#endif /* COLOR_H */
