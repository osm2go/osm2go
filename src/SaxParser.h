/*
 * SPDX-FileCopyrightText: 2023 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <libxml/parser.h>
#include <libxml/tree.h>

#ifndef LIBXML_SAX1_ENABLED
#error SAX1 parser needs to be enabled in libxml2
#endif

/**
 * @brief base class for SAX parsers
 *
 * This just puts some boilerplate code into a common place.
 */
class SaxParser {
public:
  SaxParser()
  {
    memset(&handler, 0, sizeof(handler));
    handler.characters = cb_characters;
    handler.startElement = cb_startElement;
    handler.endElement = cb_endElement;
  }
  virtual ~SaxParser() {}

  bool parseFile(const char *filename)
  {
    return xmlSAXUserParseFile(&handler, this, filename) == 0;
  }
  inline bool parseFile(const std::string &filename)
  {
    return parseFile(filename.c_str());
  }

protected:
  virtual void characters(const char *ch, int len) = 0;
  virtual void startElement(const char *name, const char **attrs) = 0;
  virtual void endElement(const char *name) = 0;

private:
  static void cb_characters(void *ts, const xmlChar *ch, int len) {
    static_cast<SaxParser *>(ts)->characters(reinterpret_cast<const char *>(ch), len);
  }
  static void cb_startElement(void *ts, const xmlChar *name, const xmlChar **attrs) {
    static_cast<SaxParser *>(ts)->startElement(reinterpret_cast<const char *>(name),
                                               reinterpret_cast<const char **>(attrs));
  }
  static void cb_endElement(void *ts, const xmlChar *name) {
    static_cast<SaxParser *>(ts)->endElement(reinterpret_cast<const char *>(name));
  }

  xmlSAXHandler handler;
};
