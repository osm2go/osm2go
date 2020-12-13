/*
 * SPDX-FileCopyrightText: 2020 Rolf Eike Beer <eike@sf-mail.de>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <pos.h>

#include <libintl.h> // for ngettext() declaration
#include <QCoreApplication>
#include <QString>
#include <string>

#define _(String) trstring(String)
extern const char *bad_tr_call; // never defined, so bad calls to tr_noop() can be detected
// #define _(String) trstring::tr(String)
#define tr_noop(String) __builtin_constant_p(String) ? \
                  (String) : bad_tr_call

extern "C" {
inline char *ngettext(const char *__msgid1, const char *, unsigned long int)
{
  return const_cast<char *>(__msgid1);
}
}

class trstring : public QString {
  explicit inline trstring(const QString &s) : QString(s) {}
  std::string argn(const std::string &pattern, const std::string &a) const;
public:
  typedef trstring native_type;
  typedef const trstring &native_type_arg;
#define TRSTRING_NATIVE_TYPE_IS_TRSTRING
  typedef trstring any_type;
  typedef const trstring &arg_type;

  explicit inline trstring() = default;
  explicit inline trstring(const char *s) : QString(QCoreApplication::translate("OSM2go", s)) {}
  trstring(const char *msg, const char *disambiguation, int n) __attribute__((nonnull(2)))
    : QString(QCoreApplication::translate("OSM2go", msg, disambiguation, n)) { }
  // catch if one passes a constant nullptr as argument
  trstring(std::nullptr_t) = delete;
  trstring(std::nullptr_t msg, const char *, int) = delete;

  trstring arg(const std::string &a) const
  { return trstring(QString::arg(QString::fromStdString(a))); }
  inline trstring arg(const char *a) const
  { return trstring(QString::arg(QString::fromUtf8(a))); }
  inline trstring arg(const QString &a) const
  { return trstring(QString::arg(a)); }
  inline trstring arg(pos_float_t a) const
  { return trstring(QString::arg(a, '0', 'f')); }
  template<typename T> inline trstring arg(T a) const
  { return trstring(QString::arg(a)); }
  inline trstring arg(double a, int fieldWidth = 0, char fmt = 'g', int prec = -1) const
  { return trstring(QString::arg(a, fieldWidth, fmt, prec)); }

  inline void assign(std::string other)
  {
    QString::operator=(QString::fromStdString(other));
  }
};
