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
  const way_t way = { .node_chain = chain };
  guint i;

  test_way(&way, n);
  g_assert(n > 0);

  for(i = 1; i < n; i++)
    g_assert(osm_way_min_length(&way, i) == TRUE);
}

int main(void)
{
  guint i;
  const way_t way0 = { 0 };
  node_t node = { 0 };
  node_chain_t chain[4];

  chain[0].next = NULL;
  for (i = 0; i < sizeof(chain) / sizeof(chain[0]); i++) {
    chain[i].node = &node;
    if(i > 0)
      chain[i].next = chain + i - 1;
  }

  test_way(&way0, 0);

  for (i = 0; i < sizeof(chain) / sizeof(chain[0]); i++) {
    test_chain(chain + i, i + 1);
  }

  return 0;
}
