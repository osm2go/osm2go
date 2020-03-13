#pragma once

#include <appdata.h>
#include <icon.h>
#include <map.h>
#include <uicontrol.h>

#include <cstdlib>

class MainUiDummy : public MainUi {
public:
  std::map<menu_items, bool> m_actions;

  MainUiDummy() : MainUi(), msg(nullptr) {}
  ~MainUiDummy() override
  {
    assert(m_actions.empty());
  }

  void setActionEnable(menu_items item, bool en) override
  {
    std::map<menu_items, bool>::iterator it = m_actions.find(item);
    if (it != m_actions.end() && it->second == en)
      m_actions.erase(it);
    else
      abort();
  }
  void showNotification(const char *, unsigned int) override
  { abort(); }
  const char *msg;
};

appdata_t::appdata_t()
  : uicontrol(new MainUiDummy())
  , map(nullptr)
  , icons(icon_t::instance())
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
};
