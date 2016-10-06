#include <osm.h>

#include <gtk/gtk.h>
#include <stdlib.h>

static inline void
test_way(const way_t *way, const guint n)
{
  g_assert(osm_way_number_of_nodes(way) == n);
  g_assert(osm_way_min_length(way, n + 1) == FALSE);

  g_assert((osm_way_number_of_nodes(way) <= 2) ==
           !osm_way_min_length(way, 3));
  g_assert((osm_way_number_of_nodes(way) < 2) ==
           !osm_way_min_length(way, 2));
}

static inline void
test_chain(node_chain_t *chain, const guint n)
{
  way_t way = { 0 };
  guint i;

  way.node_chain = chain;
  test_way(&way, n);
  g_assert(n > 0);

  for(i = 1; i < n; i++)
    g_assert(osm_way_min_length(&way, i) == TRUE);
}

int main(void)
{
  const way_t way0 = { 0 };
  node_t node = { 0 };
  node_chain_t chain(4, &node);

  test_way(&way0, 0);

  while(!chain.empty()) {
    test_chain(&chain, chain.size());
    chain.erase(chain.begin());
  }

  return 0;
}
