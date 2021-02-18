#pragma once

#include <appdata.h>
#include <gps_state.h>
#include <icon.h>
#include <josm_elemstyles.h>
#include <map.h>
#include <style.h>
#include <uicontrol.h>

#include <cstdlib>
#include <map>

#include <osm2go_annotations.h>

class MainUiDummy : public MainUi {
public:
  std::multimap<menu_items, bool> m_actions;

  MainUiDummy() : MainUi(), msg(nullptr) {}

  void check()
  {
    assert_cmpnum(m_actions.size(), 0);
    assert_cmpnum(clearFlags.size(), 0);
    assert_cmpnum(m_statusTexts.size(), 0);
  }

  ~MainUiDummy() override
  {
    check();
  }

  void setActionEnable(menu_items item, bool en) override
  {
    std::map<menu_items, bool>::iterator it = m_actions.find(item);
    if (it != m_actions.end() && it->second == en)
      m_actions.erase(it);
    else
      abort();
  }
  void showNotification(trstring::arg_type text, unsigned int) override
  {
    assert_cmpnum_op(m_statusTexts.size(), >, 0);
    assert_cmpstr(m_statusTexts.front(), static_cast<trstring::native_type>(text));
    m_statusTexts.erase(m_statusTexts.begin());
  }
  const char *msg;

  void clearNotification(NotificationFlags flags) override;

  std::vector<NotificationFlags> clearFlags;
  std::vector<trstring> m_statusTexts;
};

void MainUiDummy::clearNotification(NotificationFlags flags)
{
  assert_cmpnum_op(clearFlags.size(), >, 0);
  assert_cmpnum(static_cast<int>(flags), static_cast<int>(clearFlags.front()));
  clearFlags.erase(clearFlags.begin());
}

class gps_state_dummy : public gps_state_t {
public:
  gps_state_dummy() : gps_state_t(nullptr, nullptr) {}

  pos_t get_pos(float *) override
  { return pos_t(NAN, NAN); }

  void setEnable(bool) override
  { abort(); }
};

class invalid_style : public style_t {
public:
    invalid_style() {}
    ~invalid_style() override {}

  void colorize(node_t *) const override
  {
    abort();
  }
  void colorize(way_t *) const override
  {
    abort();
  }
};

appdata_t::appdata_t()
  : uicontrol(new MainUiDummy())
  , map(nullptr)
  , icons(icon_t::instance())
  , gps_state(new gps_state_dummy())
{
}

class test_map: public map_t {
public:
  enum Flags {
    MapDefaults = 0,
    InvalidStyle = 0x1,   ///< the style is empty and must not be used for colorization
    EmptyStyle = 0x2      ///< the style is empty and will do nothing
  };

  explicit test_map(appdata_t &a, canvas_t *cv = nullptr, unsigned int flags = MapDefaults)
    : map_t(a, cv)
  {
    assert((flags & (InvalidStyle | EmptyStyle)) != (InvalidStyle | EmptyStyle));
    if (flags & InvalidStyle)
      style.reset(new invalid_style());
    else if (flags & EmptyStyle)
      style.reset(new josm_elemstyle());
  }

  void set_autosave(bool) override { abort(); }

  /**
   * @brief a function the testcase may implement to read protected members
   */
  void test_function();

  void pen_down_item_public(canvas_item_t *item)
  {
    pen_down_item(item);
  }

  void button_press_public(const osm2go_platform::screenpos &p)
  {
    button_press(p);
  }

  void button_release_public(const osm2go_platform::screenpos &p)
  {
    button_release(p);
  }

  void handle_motion_public(const osm2go_platform::screenpos &p)
  {
    handle_motion(p);
  }

  map_action_t action_type() const
  {
    return action.type;
  }

  const way_t * action_way() const
  {
    return action.way.get();
  }

  const way_t * action_way_extending() const
  {
    return action.extending;
  }

  const way_t * action_way_ends_on() const
  {
    return action.ends_on;
  }

  void way_reverse_public()
  {
    way_reverse();
  }
};

#if __cplusplus < 201402L
namespace std {
  template<typename _Tp>
  inline _Tp *make_unique(appdata_t &a, canvas_t *v, test_map::Flags w) { return new _Tp(a, v, w); }
}
#endif
