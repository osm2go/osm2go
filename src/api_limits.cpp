/*
 * SPDX-FileCopyrightText: 2022 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "api_limits.h"

#include "misc.h"
#include "net_io.h"
#include "notifications.h"
#include "osm2go_annotations.h"
#include "settings.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <unordered_map>
#include <utility>

namespace {

unsigned int
xml_get_prop_uint(xmlNode *node, const char *prop)
{
  xmlString str(xmlGetProp(node, BAD_CAST prop));

  if(str)
    return strtoul(str, nullptr, 10);
  else
    return std::numeric_limits<unsigned int>::max();
}

class match_sv {
  nonstd::string_view sv;
public:
  inline match_sv(nonstd::string_view v) : sv(v) {}
  bool operator()(const std::pair<std::string, api_limits> &other) const
  {
    return other.first == sv;
  }
};

} // namespace

api_limits::api_limits()
  : m_minApiVersion(ApiVersion_0_6)
  , m_maxAreaSize(360 * 180)
  , m_nodesPerWay(std::numeric_limits<unsigned int>::max())
  , m_membersPerRelation(std::numeric_limits<unsigned int>::max())
  , m_elementsPerChangeset(std::numeric_limits<unsigned int>::max())
  , m_apiTimeout(std::numeric_limits<unsigned int>::max())
  , m_initialized(false)
{
}

static std::unordered_map<std::string, api_limits> instances;

const api_limits &api_limits::instance(nonstd::string_view server)
{
  const std::unordered_map<std::string, api_limits>::iterator itEnd = instances.end();
  std::unordered_map<std::string, api_limits>::iterator it = std::find_if(instances.begin(), itEnd, match_sv(server));

  if (it != itEnd)
    return it->second;

  api_limits ret;

  std::string url;
  nonstd::string_view cap_url = "/api/capabilities";
  url.resize(server.size() + cap_url.size());
  // no assignment operator if string_view is no native type, and data() returns non-const in new C++ versions anyway
  server.copy(const_cast<char *>(url.data()), server.size());
  cap_url.copy(const_cast<char *>(url.data()) + server.size(), cap_url.size());

  if (!ret.queryXml(url.c_str())) {
    static const api_limits empty;
    return empty;
  }

  return instances.insert(std::make_pair(nonstd::to_string(server), ret)).first->second;
}

const api_limits &api_limits::offlineInstance(const std::string &server)
{
  std::unordered_map<std::string, api_limits>::iterator it;

  if (server.empty())
    it = instances.find(settings_t::instance()->server);
  else
    it = instances.find(server);

  if (it != instances.end())
    return it->second;

  static const api_limits empty;
  return empty;
}

bool api_limits::parseXml(const xmlDocGuard &xml)
{
  for (xmlNodePtr cur_node = xmlDocGetRootElement(xml.get()); cur_node != nullptr; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if(likely(strcasecmp(reinterpret_cast<const char *>(cur_node->name), "osm") == 0)) {
        for (xmlNode *sub_node = cur_node->children; sub_node != nullptr; sub_node = sub_node->next) {
          if (sub_node->type == XML_ELEMENT_NODE) {
            if(strcasecmp(reinterpret_cast<const char *>(sub_node->name), "api") == 0) {
              m_initialized = true;

              for (xmlNode *policy_node = sub_node->children; policy_node != nullptr; policy_node = policy_node->next) {
                if (policy_node->type == XML_ELEMENT_NODE) {
                  if(strcasecmp(reinterpret_cast<const char *>(policy_node->name), "version") == 0) {
                    xmlString str(xmlGetProp(policy_node, BAD_CAST "minimum"));

                    if (!str.empty() && strcmp(static_cast<const char *>(str), "0.6") == 0)
                      m_minApiVersion = ApiVersion_0_6;
                    else
                      m_minApiVersion = ApiVersion_Unsupported;
                  } else if(strcasecmp(reinterpret_cast<const char *>(policy_node->name), "area") == 0) {
                    m_maxAreaSize = xml_get_prop_float(policy_node, "maximum");
                  } else if(strcasecmp(reinterpret_cast<const char *>(policy_node->name), "waynodes") == 0) {
                    m_nodesPerWay = xml_get_prop_uint(policy_node, "maximum");
                  } else if(strcasecmp(reinterpret_cast<const char *>(policy_node->name), "relationmembers") == 0) {
                    m_membersPerRelation = xml_get_prop_uint(policy_node, "maximum");
                  } else if(strcasecmp(reinterpret_cast<const char *>(policy_node->name), "changesets") == 0) {
                    m_elementsPerChangeset = xml_get_prop_uint(policy_node, "maximum_elements");
                  } else if(strcasecmp(reinterpret_cast<const char *>(policy_node->name), "timeout") == 0) {
                    m_apiTimeout = xml_get_prop_uint(policy_node, "seconds");
                  } else
                    printf("DEBUG: found unhandled osm/api/%s\n", policy_node->name);
                }
              }
            } else
              printf("DEBUG: found unhandled osm/%s\n", sub_node->name);
          }
        }
      } else
        printf("DEBUG: found unhandled %s\n", cur_node->name);
    }
  }

  return m_initialized;
}

bool api_limits::queryXml(const char *url)
{
  std::string capmem;

  if(unlikely(!net_io_download_mem(nullptr, url, capmem, _("API limits")))) {
    error_dlg(_("API limits download failed"));
    return false;
  }

  xmlDocGuard doc(xmlReadMemory(capmem.c_str(), capmem.size(),
                                                          nullptr, nullptr, XML_PARSE_NONET));

  /* parse the file and get the DOM */
  if(unlikely(!doc)) {
    xmlErrorPtr errP = xmlGetLastError();
    error_dlg(trstring("API limits download failed:\n\n"
              "XML error while parsing limits:\n%1").arg(errP->message));

    return false;
  } else {
    printf("ok, parse doc tree\n");

    return api_limits::parseXml(doc);
  }
}
