#include <osm2go_i18n.h>

#include <osm2go_annotations.h>
#include <osm2go_cpp.h>

#include <iostream>

int main()
{
  std::string foo = trstring("abc %1 def %2 ghi").arg("nkw").arg(1);
  assert_cmpstr(foo, "abc nkw def 1 ghi");

  foo = trstring("abc %1 def %1 ghi").arg("nkw").arg(1);
  assert_cmpstr(foo, "abc nkw def nkw ghi");

  foo = trstring("abc %1 def %2 ghi %3").arg("3.14").arg("nkw");
  assert_cmpstr(foo, "abc 3.14 def nkw ghi %3");

  foo = trstring("abc %1 def %2 ghi %3").arg(3).arg("nkw");
  assert_cmpstr(foo, "abc 3 def nkw ghi %3");

  foo = trstring("%1%n%2", nullptr, 2).arg("a").arg("b");
  assert_cmpstr(foo, "a2b");

  return 0;
}
