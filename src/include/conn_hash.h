#ifndef _CONN_HASH_H_
#define _CONN_HASH_H_

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define DEFAULT_CONTAINER_CAPACITY	32
#define CONTAINER_COPY(dest, src, c)	((c)->copy) ?	\
	(c)->copy(dest, src) : memcpy(dest, src, (c)->elem_size)

#define MAX_BUCKET_CAPACITY	11
#define EQUALS(e1, e2, h)	(h->compare) ?	\
	(!h->compare(e1, e2)) : (!memcmp(e1, e2, h->key_size))
#define HSET_INIT(h, elem_size)	hset_init((h), (elem_size), NULL, NULL, NULL)

#define rot(x,k) (((x)<<(k)) | ((x)>>(32-(k))))
#define mix(a,b,c) \
do { \
	a -= c;  a ^= rot(c, 4);  c += b; \
	b -= a;  b ^= rot(a, 6);  a += c; \
	c -= b;  c ^= rot(b, 8);  b += a; \
	a -= c;  a ^= rot(c,16);  c += b; \
	b -= a;  b ^= rot(a,19);  a += c; \
	c -= b;  c ^= rot(b, 4);  b += a; \
} while (0)

#define final(a,b,c) \
do { \
	c ^= b; c -= rot(b,14); \
	a ^= c; a -= rot(c,11); \
	b ^= a; b -= rot(a,25); \
	c ^= b; c -= rot(b,16); \
	a ^= c; a -= rot(c,4);  \
	b ^= a; b -= rot(a,14); \
	c ^= b; c -= rot(b,24); \
} while (0)

#define UTIL_FOREACH(it, c)				\
	if ((c))					\
		for ((c)->iter_head(&(it), (c));	\
				(it).ptr;		\
				(c)->iter_next(&(it), (c)))

#define UTIL_FOREACH_REVERSE(it, c)			\
	if ((c))					\
		for ((c)->iter_tail(&(it), (c));	\
				(it).ptr;		\
				(c)->iter_prev(&(it), (c)))

#define util_foreach		UTIL_FOREACH
#define util_foreach_reverse	UTIL_FOREACH_REVERSE

typedef struct {
	void *ptr;
	void *data;
	void *key;
	size_t bkt_index;
	size_t i;
	size_t size;
} iterator_t;

/* hash function for little endian 32bit machine */
static inline uint32_t hashlittle32(const void *key, size_t length, uint32_t initval)
{
	uint32_t a,b,c;                                          /* internal state */

	/* Set up the internal state */
	a = b = c = 0xdeadbeef + ((uint32_t)length) + initval;

	const uint32_t *k = (const uint32_t *)key;         /* read 32-bit chunks */
	const uint8_t  *k8;

	/*------ all but last block: aligned reads and affect 32 bits of (a,b,c) */
	while (length > 12)
	{
		a += k[0];
		b += k[1];
		c += k[2];
		mix(a,b,c);
		length -= 12;
		k += 3;
	}

	/*----------------------------- handle the last (probably partial) block */
	/**
	 * "k[2]&0xffffff" actually reads beyond the end of the string, but
	 * then masks off the part it's not allowed to read.  Because the
	 * string is aligned, the masked-off tail is in the same word as the
	 * rest of the string.  Every machine with memory protection I've seen
	 * does it on word boundaries, so is OK with this.  But VALGRIND will
	 * still catch it and complain.  The masking trick does make the hash
	 * noticably faster for short strings (like English words).
	 */
	k8 = (const uint8_t *)k;
	switch(length)
	{
		case 12: c+=k[2]; b+=k[1]; a+=k[0]; break;
		case 11: c+=((uint32_t)k8[10])<<16;  /* fall through */
		case 10: c+=((uint32_t)k8[9])<<8;    /* fall through */
		case 9 : c+=k8[8];                   /* fall through */
		case 8 : b+=k[1]; a+=k[0]; break;
		case 7 : b+=((uint32_t)k8[6])<<16;   /* fall through */
		case 6 : b+=((uint32_t)k8[5])<<8;    /* fall through */
		case 5 : b+=k8[4];                   /* fall through */
		case 4 : a+=k[0]; break;
		case 3 : a+=((uint32_t)k8[2])<<16;   /* fall through */
		case 2 : a+=((uint32_t)k8[1])<<8;    /* fall through */
		case 1 : a+=k8[0]; break;
		case 0 : return c;
	}

	final(a,b,c);
	return c;
}

static inline uint32_t hash(const void *key, size_t length)
{
	return hashlittle32(key, length, 0);
}

typedef struct hash_set hash_set_t;

struct chain_node {
	struct chain_node *next;
	char data[0];
};

struct bucket {
	struct chain_node *first;
	size_t size;
};

struct hash_set {
	struct bucket *buckets;
	size_t elem_size;
	size_t key_size;
	size_t size;
	size_t bucket_size;
	size_t max_bucket_capacity;
	int has_long_chain;
	size_t long_chain_index;
	void (*copy)(void *dest, void *src);
	void (*free)(void *element);
	int (*compare)(void *e1, void *e2);
	void (*iter_head)(iterator_t *it, hash_set_t *h);
	void (*iter_next)(iterator_t *it, hash_set_t *h);
};

/* function prototype */
void hset_init(hash_set_t *h, size_t elem_size,
		void (*copy_func)(void *, void *),
		void (*free_func)(void *),
		int (*cmp_func)(void *, void *));
void hset_clear(hash_set_t *h);
void hset_insert(hash_set_t *h, void *key);
void hset_erase(hash_set_t *h, void *key);
void hset_find(hash_set_t *h, void *key, iterator_t *it);

/* destroy the hash set */
static inline void hset_destroy(hash_set_t *h)
{
	assert(h);
	hset_clear(h);
	free(h->buckets);
	h->buckets = NULL;
}

/* checks whether the hash set is empty */
static inline int hset_empty(hash_set_t *h)
{
	assert(h);
	return !h->size;
}

/* return the number of elements */
static inline size_t hset_size(hash_set_t *h)
{
	assert(h);
	return h->size;
}

static inline void __set_key_size(hash_set_t *h, size_t key_size)
{
	assert(h && key_size > 0 && key_size <= h->elem_size);
	h->key_size = key_size;
}

static inline void __free_chain_node(hash_set_t *h, struct chain_node *n)
{
	assert(h && n);
	if (h->free != NULL) {
		h->free(n->data);
	}

	free(n);
}

#endif
