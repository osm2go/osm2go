/*
 * Copyright (C) 2008 Till Harbaum <till@harbaum.org>.
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
 * along with OSM2Go.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MISC_H
#define MISC_H

#include <gtk/gtk.h>
#include <libxml/tree.h>

#define MISC_AGAIN_ID_DELETE           (1<<0)
#define MISC_AGAIN_ID_JOIN_NODES       (1<<1)
#define MISC_AGAIN_ID_JOIN_WAYS        (1<<2)
#define MISC_AGAIN_ID_OVERWRITE_TAGS   (1<<3)
#define MISC_AGAIN_ID_EXTEND_WAY       (1<<4)
#define MISC_AGAIN_ID_EXTEND_WAY_END   (1<<5)
#define MISC_AGAIN_ID_EXPORT_OVERWRITE (1<<6)
#define MISC_AGAIN_ID_AREA_TOO_BIG     (1<<7)

/* these flags prevent you from leaving the dialog with no (or yes) */
/* if the "dont show me this dialog again" checkbox is selected. This */
/* makes sure, that you can't permanently switch certain things in, but */
/* only on. e.g. it doesn't make sense to answer a "do you really want to */
/* delete this" dialog with "no and don't ask me again". You'd never be */
/* able to delete anything again */
#define MISC_AGAIN_FLAG_DONT_SAVE_NO  (1<<0)
#define MISC_AGAIN_FLAG_DONT_SAVE_YES (1<<1)

#include <osm2go_cpp.h>

#include <memory>
#include <string>
#include <vector>

extern const char *data_paths[];

std::string find_file(const std::string &n);

/* some compat code */

template<typename T> void shrink_to_fit(T &v) {
#if __cplusplus >= 201103L
  v.shrink_to_fit();
#else
  T tmp;
  tmp.resize(v.size());
  tmp = v;
  tmp.swap(v);
#endif
}

#if __cplusplus < 201402L
namespace std {
template<typename T> typename T::const_iterator cbegin(const T &c) {
#if __cplusplus >= 201103L
  return c.cbegin();
#else
  return typename T::const_iterator(c.begin());
#endif
}
}
#endif

// gcc did not set the C++ level before it was officially released
#if __cplusplus < 201103L && O2G_COMPILER_CXX_NULLPTR == 0
// taken from gcc 4.8.5, stripped down to what is actually needed
namespace std {
  template<typename T>
  struct default_delete {
    void operator()(T *p) const {
      delete p;
    }
  };

  template <typename _Tp, typename _Dp = default_delete<_Tp> >
    class unique_ptr {
    public:
      typedef _Tp                       element_type;
      typedef _Dp                       deleter_type;

      typedef std::pair<_Tp *, _Dp>  __tuple_type;
      __tuple_type                  _M_t;

      // Constructors.
      unique_ptr(_Tp *__p, deleter_type __d = deleter_type())
      : _M_t(__p, __d) { }

      // Destructor.
      ~unique_ptr() {
        _Tp *&__ptr = _M_t.first;
        if (__ptr != O2G_NULLPTR)
          get_deleter()(__ptr);
        __ptr = O2G_NULLPTR;
      }

      // Observers.
      element_type operator*() const {
        return *get();
      }

      _Tp *operator->() const {
        return get();
      }

      _Tp *get() const
      { return _M_t.first; }

      deleter_type& get_deleter()
      { return _M_t.second; }

      const deleter_type& get_deleter() const
      { return _M_t.first; }

      operator bool() const
      { return get() == O2G_NULLPTR ? false : true; }

      // Modifiers.
      _Tp *release() {
        _Tp *__p = get();
        _M_t.first = O2G_NULLPTR;
        return __p;
      }

      void reset(_Tp *__p = O2G_NULLPTR) {
        using std::swap;
        swap(_M_t.first, __p);
        if (__p != O2G_NULLPTR)
          get_deleter()(__p);
      }

    private:
      // Disable copy from lvalue.
      unique_ptr(const unique_ptr &);
      unique_ptr& operator=(const unique_ptr &);
  };

}
#endif

struct pos_t;

void remove_trailing_zeroes(char *str);

double xml_get_prop_float(xmlNode *node, const char *prop);
bool xml_get_prop_is(xmlNode *node, const char *prop, const char *str);
bool xml_get_prop_pos(xmlNode *node, struct pos_t *pos);
void xml_set_prop_pos(xmlNode *node, const struct pos_t *pos);

#ifndef g_assert_true
#define g_assert_true(expr)             G_STMT_START { \
                                             if G_LIKELY (expr) ; else \
                                               g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
                                                                    "'" #expr "' should be TRUE"); \
                                        } G_STMT_END
#endif
#ifndef g_assert_false
#define g_assert_false(expr)            G_STMT_START { \
                                             if G_LIKELY (!(expr)) ; else \
                                               g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
                                                                    "'" #expr "' should be FALSE"); \
                                        } G_STMT_END
#endif
#ifndef g_assert_null
#define g_assert_null(expr)             G_STMT_START { if G_LIKELY ((expr) == O2G_NULLPTR) ; else \
                                               g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
                                                                    "'" #expr "' should be NULL"); \
                                        } G_STMT_END
#endif
#ifndef g_assert_nonnull
#define g_assert_nonnull(expr)          G_STMT_START { \
                                             if G_LIKELY ((expr) != O2G_NULLPTR) ; else \
                                               g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
                                                                    "'" #expr "' should not be NULL"); \
                                        } G_STMT_END
#endif

GtkWidget *button_new_with_label(const gchar *label);

struct appdata_t;

void errorf(GtkWidget *parent, const char *fmt, ...) G_GNUC_PRINTF(2, 3);
void warningf(GtkWidget *parent, const char *fmt, ...) G_GNUC_PRINTF(2, 3);
void messagef(GtkWidget *parent, const char *title, const char *fmt, ...) G_GNUC_PRINTF(3, 4);

/* dialog size are specified rather fuzzy */
enum DialogSizeHing {
  MISC_DIALOG_NOSIZE = -1,
  MISC_DIALOG_SMALL  =  0,
  MISC_DIALOG_MEDIUM =  1,
  MISC_DIALOG_LARGE  =  2,
  MISC_DIALOG_WIDE   =  3,
  MISC_DIALOG_HIGH   =  4
};

GtkWidget *misc_dialog_new(DialogSizeHing hint, const gchar *title, GtkWindow *parent, ...);
GtkWidget *misc_scrolled_window_new(gboolean etched_in);
void misc_table_attach(GtkWidget *table, GtkWidget *widget, int x, int y);

/* unified widgets */
GtkWidget *entry_new(void);
GType entry_type(void);

bool yes_no_f(GtkWidget *parent,
              appdata_t &appdata, guint again_bit, gint flags,
              const char *title, const char *fmt, ...) G_GNUC_PRINTF(6, 7);
GtkWidget *check_button_new_with_label(const gchar *label);
void check_button_set_active(GtkWidget *button, gboolean active);
gboolean check_button_get_active(GtkWidget *button);
GType check_button_type(void);

GtkWidget *notebook_new(void);
void notebook_append_page(GtkWidget *notebook,
			  GtkWidget *page, const gchar *label);
GtkNotebook *notebook_get_gtk_notebook(GtkWidget *notebook);

GtkWidget *combo_box_new(const gchar *title);
GtkWidget *combo_box_entry_new(const gchar *title);
void combo_box_append_text(GtkWidget *cbox, const gchar *text);
void combo_box_set_active(GtkWidget *cbox, int index);
int combo_box_get_active(GtkWidget *cbox);
std::string combo_box_get_active_text(GtkWidget *cbox);
GType combo_box_type(void);
GType combo_box_entry_type(void);

void open_url(appdata_t &appdata, const char *url);

void misc_init(void);

GtkWidget *string_select_widget(const char *title, const std::vector<std::string> &entries, int match);

#endif // MISC_H
