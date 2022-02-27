/*
 * SPDX-FileCopyrightText: 2022 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "misc.h"

class api_limits {
public:
  enum ApiVersions {
    ApiVersion_0_6 = 6,
    ApiVersion_Unsupported = 9999
  };

private:
  api_limits();

  static api_limits &raw_instance();
  static const api_limits &instance();

  ApiVersions m_minApiVersion;
  float m_maxAreaSize;
  unsigned int m_nodesPerWay;
  unsigned int m_membersPerRelation;
  unsigned int m_elementsPerChangeset;
  unsigned int m_apiTimeout;
  bool m_initialized;
public:

  /**
   * @brief return the minimum supported API version
   *
   * API 0.6 -> 6
   */
  static ApiVersions minApiVersion()
  {
    return instance().m_minApiVersion;
  }

  /**
   * @brief return the maximum downloadable area in square degrees
   */
  static float maxAreaSize()
  {
    return instance().m_maxAreaSize;
  }

  static unsigned int nodesPerWay()
  {
    return instance().m_nodesPerWay;
  }

  static unsigned int membersPerRelation()
  {
    return instance().m_membersPerRelation;
  }

  static unsigned int elementsPerChangeset()
  {
    return instance().m_elementsPerChangeset;
  }

  static unsigned int apiTimeout()
  {
    return instance().m_apiTimeout;
  }

protected:
  /**
   * @brief parse the limits XML
   *
   * This is not private so the testcases can easily access it
   */
  static bool parseXml(const xmlDocGuard &xml);

  static bool queryXml(const char *url);
};
