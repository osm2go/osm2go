/*
 * SPDX-FileCopyrightText: 2017 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <osm2go_cpp.h>

class color_t {
  uint32_t value;
public:
  inline color_t(uint8_t cr, uint8_t cg, uint8_t cb, uint8_t ca = 0xff) noexcept
  { value = ((static_cast<uint32_t>(cr) << 24) |
            (static_cast<uint32_t>(cg) << 16) |
            (static_cast<uint32_t>(cb) <<  8) | ca); }
  inline color_t(uint16_t cr, uint16_t cg, uint16_t cb, uint8_t ca = 0xff) noexcept
  { value = ((static_cast<uint32_t>(cr & 0xff00) << 16) |
            (static_cast<uint32_t>(cg & 0xff00) << 8)  |
            (static_cast<uint32_t>(cb & 0xff00)        | ca)); }
  inline color_t(unsigned int c) noexcept : value(c) {}
  inline color_t &operator=(unsigned int c) noexcept { value = c; return *this; }
#if __cplusplus >= 201103L
  inline color_t() = default;
  inline color_t(const color_t &other) = default;
  inline color_t &operator=(const color_t &other) = default;

  // this usually just means "is_transparent()"
  template<typename T> bool operator&(T) = delete;
#else
  inline color_t() {}
  inline color_t(const color_t &other) : value(other.value) {}
  inline color_t &operator=(const color_t other) { value = other.value; return *this; }
#endif
  inline operator unsigned int() const noexcept { return value; }

  inline uint32_t rgba() const noexcept { return value; }
  inline unsigned int rgb() const noexcept { return value >> 8; }

  inline bool is_transparent() const noexcept { return (value & 0xff) == 0; }

  static inline color_t transparent() noexcept
  {
    return color_t(0);
  }

  static inline color_t black() noexcept
  {
    return color_t(0x000000ff);
  }

  inline color_t combine_alpha(color_t other) const noexcept
  {
    return color_t((value & ~0xff) | (other.value & 0xff));
  }
} __attribute__ ((packed));

static_assert(sizeof(color_t) == sizeof(unsigned int), "wrong size for color_t");
