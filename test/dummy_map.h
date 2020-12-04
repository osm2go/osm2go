#pragma once

#include <appdata.h>
#include <gps_state.h>
#include <icon.h>
#include <map.h>
#include <uicontrol.h>

#include <cstdlib>
#include <map>

#include <osm2go_annotations.h>

class MainUiDummy : public MainUi {
public:
  std::multimap<menu_items, bool> m_actions;

  MainUiDummy() : MainUi(), msg(nullptr) {}
  ~MainUiDummy() override
  {
    assert_cmpnum(m_actions.size(), 0);
    assert_cmpnum(clearFlags.size(), 0);
    assert_cmpstr(m_statusText, trstring());
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
    assert_cmpstr(m_statusText, static_cast<trstring::native_type>(text));
    m_statusText = trstring();
  }
  const char *msg;

  void clearNotification(NotificationFlags flags) override;

  std::vector<NotificationFlags> clearFlags;
  trstring m_statusText;
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

appdata_t::appdata_t()
  : uicontrol(new MainUiDummy())
  , map(nullptr)
  , icons(icon_t::instance())
  , gps_state(new gps_state_dummy())
{
}

class test_map: public map_t {
public:
  explicit test_map(appdata_t &a, canvas_t *cv = nullptr) : map_t(a, cv) {}

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

  map_action_t action_type() const
  {
    return action.type;
  }

  const way_t * action_way() const
  {
    return action.way;
  }

  const way_t * action_way_extending() const
  {
    return action.extending;
  }

  const way_t * action_way_ends_on() const
  {
    return action.ends_on;
  }
};
