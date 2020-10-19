#pragma once

#include <osm.h>

#include <osm2go_stl.h>

class verify_osm_db {
  verify_osm_db() O2G_DELETED_FUNCTION;
  ~verify_osm_db() O2G_DELETED_FUNCTION;

  template<typename T> static void
  verify_osm_map(osm_t::ref osm)
  {
    const std::unordered_map<item_id_t, const T *> &orig = osm->originalObjects<T>();
    const std::map<item_id_t, T *> &objects = osm->objects<T>();

    unsigned int o_modified = 0;
    unsigned int o_deleted = 0;
    unsigned int modified = 0;
    unsigned int deleted = 0;

    const typename std::map<item_id_t, T *>::const_iterator itEnd = objects.end();
    const typename std::unordered_map<item_id_t, const T *>::const_iterator oitEnd = orig.end();

    for (typename std::unordered_map<item_id_t, const T *>::const_iterator oit = orig.begin(); oit != oitEnd; oit++) {
      const typename std::map<item_id_t, T *>::const_iterator it = objects.find(oit->first);
      assert(it != itEnd);
      if (it->second->flags & OSM_FLAG_DELETED)
        o_deleted++;
      else if (it->second->flags & OSM_FLAG_DIRTY)
        o_modified++;
      else
        assert_unreachable();
    }

    for (typename std::map<item_id_t, T *>::const_iterator it = objects.begin(); it != itEnd; it++) {
      unsigned int flags = it->second->flags;
      assert(it->second->id != ID_ILLEGAL);
      // ignore new entries: they are not accounted for in the original map
      if (it->second->id < ID_ILLEGAL)
        continue;
      if (flags & OSM_FLAG_DELETED)
        deleted++;
      else if (flags & OSM_FLAG_DIRTY)
        modified++;
      else
        assert_cmpnum(flags, 0);
    }

    assert_cmpnum(o_modified, modified);
    assert_cmpnum(o_deleted, deleted);
  }

public:
  static void
  run(osm_t::ref osm)
  {
    verify_osm_map<node_t>(osm);
    verify_osm_map<way_t>(osm);
    verify_osm_map<relation_t>(osm);
  }
};
