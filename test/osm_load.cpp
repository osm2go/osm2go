#include <icon.h>
#include <osm.h>

#include <osm2go_cpp.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <iostream>

template<typename T>
struct tag_counter {
  unsigned int &tags;
  unsigned int &tag_objs;
  tag_counter(unsigned int &t, unsigned int &o) : tags(t), tag_objs(o) {}
  void operator()(const tag_t &) {
    tags++;
  }
  void operator()(const std::pair<item_id_t, T *> &pair) {
    const base_object_t * const obj = pair.second;
    if(obj->tags.empty())
      return;
    tag_objs++;
    obj->tags.for_each(*this);
  }
};

int main(int argc, char **argv)
{
  if(argc != 2)
    return EINVAL;

  xmlInitParser();

  icon_t &icons = icon_t::instance();
  osm_t *osm = osm_t::parse(std::string(), argv[1], icons);
  if(!osm) {
    std::cerr << "cannot open " << argv[1] << ": " << strerror(errno) << std::endl;
    return 1;
  }

  unsigned int t[] = { 0, 0, 0 };
  unsigned int to[] = { 0, 0, 0 };

  std::for_each(osm->nodes.begin(), osm->nodes.end(), tag_counter<node_t>(t[0], to[0]));
  std::for_each(osm->ways.begin(), osm->ways.end(), tag_counter<way_t>(t[1], to[1]));
  std::for_each(osm->relations.begin(), osm->relations.end(), tag_counter<relation_t>(t[2], to[2]));

  std::cout
    << "Nodes: " << osm->nodes.size()     << ", " << to[0] << " with " << t[0] << " tags" << std::endl
    << "Ways: " << osm->ways.size()      << ", " << to[1] << " with " << t[1] << " tags" << std::endl
    << "Relations: " << osm->relations.size() << ", " << to[2] << " with " << t[2] << " tags" << std::endl;

  delete osm;

  xmlCleanupParser();

  return 0;
}
