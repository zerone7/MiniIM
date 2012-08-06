#ifndef _CONN_ALLOCATOR_H_
#define _CONN_ALLOCATOR_H_

#include <stdlib.h>
#include <assert.h>
#include "conn_list.h"

/**
 * A simple allocator implementation
 */
struct allocator {
	struct list_head free_list;
	size_t element_size;
};

/**
 * memory item, this struct should not be used outside of this file
 */
struct __item {
	struct list_head list;
};

/**
 * initialize the allocator
 */
static inline allocator_init(struct allocator *a, size_t size)
{
	assert(a && size >= sizeof(struct __item));
	a->element_size = size;
}

/**
 * destroy the allocator, free the cached memory
 */
static inline void allocator_destroy(struct allocator *a)
{
	struct __item *ret;
	while (!list_empty(&a->free_list)) {
		ret = list_first_entry(&a->free_list, struct __item, list);
		list_del(&ret->list);
		free(ret);
	}
}

/**
 * allocate the memory
 */
static inline void* allocator_malloc(struct allocator *a)
{
	struct __item *ret;
	if (!list_empty(&a->free_list)) {
		ret = list_first_entry(&a->free_list, struct __item, list);
		list_del(&ret->list);
	} else {
		ret = malloc(a->size);
	}
	return ret;
}

/**
 * free the memory
 */
static inline void allocator_free(struct allocator *a, void *p)
{
	struct __item *item = (struct __item *)p;
	list_add(&a->free_list, &item->list);
}

#endif
