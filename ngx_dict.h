/*
 * ngx_dict.h
 * Copyright © 2007 Adrian Perez <adrianperez@udc.es>
 */

#ifndef __ngx_dict_h__
#define __ngx_dict_h__ 1

#include <ngx_config.h>
#include <ngx_core.h>

/*
 * Declarations for inline stuff (implemented below)
 */
static inline int
	ngx_dict_streq(const ngx_str_t const* a, const ngx_str_t const* b);
static inline int
	ngx_dict_strneq(const ngx_str_t const* a, const ngx_str_t const* b,
			register size_t len);
static inline unsigned
	ngx_dict_strhash(const ngx_str_t *s);
static inline unsigned
	ngx_dict_strnhash(const ngx_str_t *s, register size_t len);


typedef struct ngx_dict ngx_dict_t;
typedef void** ngx_dict_iterator_t;
typedef void (*ngx_dict_traverse_fun_t)(void *data, void *context);


int  ngx_dict_init(ngx_pool_t *pool, ngx_dict_t *d);
void ngx_dict_free(ngx_dict_t *d);
void ngx_dict_clear(ngx_dict_t *d);

size_t ngx_dict_count(const ngx_dict_t *d);

void* ngx_dict_getn(const ngx_dict_t *d, const ngx_str_t *key, size_t keylen);
void  ngx_dict_setn(ngx_dict_t *d, const ngx_str_t *key, size_t keylen, void *data);

#define ngx_dict_get(_d, _s) \
	ngx_dict_getn((_d), (_s), (_s)->len)

#define ngx_dict_set(_d, _s, _v) \
	ngx_dict_setn((_d), (_s), (_s)->len, (_v))

void  ngx_dict_del(ngx_dict_t *d, const ngx_str_t *key);
void  ngx_dict_update(ngx_dict_t *d, const ngx_dict_t *o);

void ngx_dict_traverse_nodes(const ngx_dict_t *d,
		ngx_dict_traverse_fun_t f, void *ctx);

void ngx_dict_traverse_keys(const ngx_dict_t *d,
		ngx_dict_traverse_fun_t f, void *ctx);

void ngx_dict_traverse_values(const ngx_dict_t *d,
		ngx_dict_traverse_fun_t f, void *ctx);


/*
 * This is the layout of a dictionary node. Do not move "val" out of the
 * first place, or the iterator trick won't work anymore.
 */
typedef struct ngx_dict_node ngx_dict_node_t;

struct ngx_dict_node
{
	void            *val;
	ngx_str_t        key;
	ngx_dict_node_t *next;
	ngx_dict_node_t *nextNode;
	ngx_dict_node_t *prevNode;
};


struct ngx_dict
{
	ngx_dict_node_t **nodes;
	ngx_dict_node_t *first;
	unsigned         count;
	unsigned         size;
};


#define ngx_dict_first_node(_d) \
	((_d)->first)

#define ngx_dict_next_node(_n) \
	((_n)->nextNode)

#define ngx_dict_count(_d) \
	((_d)->count)


/*
 * Inline functions
 */

static inline int
ngx_dict_streq(const ngx_str_t const* a, const ngx_str_t const* b)
{
	register unsigned len;
	u_char *sa, *sb;

	if (a->len != b->len) return 0;

	for (sa = a->data, sb = b->data, len = a->len; len--;)
		if (*sa++ != *sb++) return 0;

	return 1;
}


static inline int
ngx_dict_strneq(const ngx_str_t const* a, const ngx_str_t const* b, register size_t len)
{
	u_char *sa, *sb;

	if ((a->len != b->len) || (a->len < len)) return 0;

	for (sa = a->data, sb = b->data; len--;)
		if (*sa++ != *sb++) return 0;

	return 1;
}


static inline unsigned
ngx_dict_hashstr(const ngx_str_t *s)
{
	register unsigned ret = 0;
	register unsigned ctr = 0;
	register unsigned cnt = s->len;
	register u_char  *str = s->data;

	while (cnt--) {
		ret ^= *str++ << ctr;
		ctr  = (ctr + 1) % sizeof(void*);
	}
	return ret;
}


static inline unsigned
ngx_dict_hashnstr(const ngx_str_t *s, register unsigned len)
{
  register unsigned ret = 0;
  register unsigned ctr = 0;
  register u_char  *str = s->data;

  if (len > s->len)
    len = s->len;

  while (len--) {
    ret ^= *str++ << ctr;
    ctr  = (ctr + 1) % sizeof(void*);
  }
  return ret;
}

#endif /* !__ngx_dict_h__ */

