/*
 * include/linux/pagevec.h
 *
 * In many places it is efficient to batch an operation up against multiple
 * pages.  A pagevec is a multipage container which is used for that.
 */

#ifndef _LINUX_PAGEVEC_H
#define _LINUX_PAGEVEC_H

/* 14 pointers + two long's align the pagevec structure to a power of two */
#define PAGEVEC_SIZE	14

#include <linux/page-isolation.h>
struct page;
struct address_space;

struct pagevec {
	unsigned long nr;
	unsigned long cold;
	struct page *pages[PAGEVEC_SIZE];
};

void __pagevec_release(struct pagevec *pvec);
void __pagevec_lru_add(struct pagevec *pvec);
unsigned pagevec_lookup_entries(struct pagevec *pvec,
				struct address_space *mapping,
				pgoff_t start, unsigned nr_entries,
				pgoff_t *indices);
void pagevec_remove_exceptionals(struct pagevec *pvec);
unsigned pagevec_lookup(struct pagevec *pvec, struct address_space *mapping,
		pgoff_t start, unsigned nr_pages);
unsigned pagevec_lookup_tag(struct pagevec *pvec,
		struct address_space *mapping, pgoff_t *index, int tag,
		unsigned nr_pages);

static inline void pagevec_init(struct pagevec *pvec, int cold)
{
	pvec->nr = 0;
	pvec->cold = cold;
}

static inline void pagevec_reinit(struct pagevec *pvec)
{
	pvec->nr = 0;
}

static inline unsigned pagevec_count(struct pagevec *pvec)
{
	return pvec->nr;
}

static inline unsigned pagevec_space(struct pagevec *pvec)
{
	return PAGEVEC_SIZE - pvec->nr;
}

/*
 * Add a page to a pagevec.  Returns the number of slots still available.
 */
static inline unsigned pagevec_add(struct pagevec *pvec, struct page *page)
{
	unsigned ret = 0;
#ifdef CONFIG_CMA
	int migrate_type = 0;
#endif

	pvec->pages[pvec->nr++] = page;
	ret = pagevec_space(pvec);

#ifdef CONFIG_CMA
	migrate_type = get_pageblock_migratetype(page);
	if (is_migrate_cma(migrate_type) ||
	   is_migrate_isolate(migrate_type)) {
		ret = 0;
	}
#endif

	return ret;
}

static inline void pagevec_release(struct pagevec *pvec)
{
	if (pagevec_count(pvec))
		__pagevec_release(pvec);
}

#endif /* _LINUX_PAGEVEC_H */
