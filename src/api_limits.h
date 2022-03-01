/*
 * SPDX-FileCopyrightText: 2022 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "misc.h"

#include <osm2go_cpp.h>

class api_limits {
public:
  enum ApiVersions {
    ApiVersion_0_6 = 6,
    ApiVersion_Unsupported = 9999
  };

protected:
  api_limits();

private:
  ApiVersions m_minApiVersion;
  float m_maxAreaSize;
  unsigned int m_nodesPerWay;
  unsigned int m_membersPerRelation;
  unsigned int m_elementsPerChangeset;
  unsigned int m_apiTimeout;
  bool m_initialized;

public:
  /**
   * @brief get the API limits of the given server
   * @param server the base URL of the API server
   *
   * If the server can not be reached an instance with default
   * values will be returned.
   */
  static const api_limits &instance(const std::string &server);

  /**
   * @brief get the API limits of the given server if already available
   * @param server the base URL of the API server
   *
   * If the server has already been contacted return the instance with
   * the real values, otherwise one with default values.
   *
   * In case server is empty the default server will be used.
   */
  static const api_limits &offlineInstance(const std::string &server);

  bool initialized() const
  {
    return m_initialized;
  }

  /**
   * @brief return the minimum supported API version
   *
   * API 0.6 -> 6
   */
  ApiVersions minApiVersion() const
  {
    return m_minApiVersion;
  }

  /**
   * @brief return the maximum downloadable area in square degrees
   */
  float maxAreaSize() const
  {
    return m_maxAreaSize;
  }

  unsigned int nodesPerWay() const
  {
    return m_nodesPerWay;
  }

  unsigned int membersPerRelation() const
  {
    return m_membersPerRelation;
  }

  unsigned int elementsPerChangeset() const
  {
    return m_elementsPerChangeset;
  }

  unsigned int apiTimeout() const
  {
    return m_apiTimeout;
  }

protected:
  /**
   * @brief parse the limits XML
   *
   * This is not private so the testcases can easily access it
   */
  bool parseXml(const xmlDocGuard &xml);

  bool queryXml(const char *url);
};
