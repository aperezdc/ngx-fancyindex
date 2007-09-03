/*
 * ngx_dict.h
 * Copyright © 2007 Adrian Perez <adrianperez@udc.es>
 */

#ifndef __ngx_dict_h__
#define __ngx_dict_h__ 1

#include <ngx_core.h>

/*
 * Declarations for inline stuff (implemented below)
 */
static inline int
	ngx_dict_streq(const ngx_str_t *a, const ngx_str_t *b);
static inline int
	ngx_dict_strneq(const ngx_str_t *a, const ngx_str_t, register size_t len);
static inline unsigned
	ngx_dict__strhash(const ngx_str_t *s);
static inline unsigned
	ngx_dict_strnhash(const ngx_str_t *s, register size_t len);




typedef struct ngx_dict ngx_dict_t;


int  ngx_dict_init(ngx_pool_t *pool, ngx_dict_t *d);
void ngx_dict_free(ngx_dict_t *d);
void ngx_dict_clear(ngx_dict_t *d);

size_t ngx_dict_count(const ngx_dict_t *d);

void* ngx_dict_getn(const ngx_dict_t *d, const ngx_str_t *key, size_t keylen);
void  ngx_dict_setn(ngx_dict_t *d, const ngx_str_t *key, size_t keylen, void *data);

void  ngx_dict_del(ngx_dict_t *d, const ngx_str_t *key);
void  ngx_dict_set(ngx_dict_t *d, const ngx_str_t *key, void *data);
void* ngx_dict_get(const ngx_dict_t *d, const ngx_str_t *key);

void  ngx_dict_update(ngx_dict_t *d, const ngx_dict_t *o);

void ngx_dict_traverse(const ngx_dict_t *d,
		ngx_dict_traverse_fun_t f, void *ctx);

void ngx_dict_traverse_keys(const ngx_dict_t *d,
		ngx_dict_traverse_fun_t f, void *ctx);

void ngx_dict_traverse_values(const ngx_dict_t *d,
		ngx_dict_traverse_fun_t f, void *ctx);

ngx_dict_iterator_t ngx_dict_first(const ngx_dict_t *d);
ngx_dict_iterator_t ngx_dict_next(const ngx_dict_t *d, ngx_dict_iterator_t i);
const char ngx_str_t* ngx_dict_iterator_get_key(ngx_iterator_t i);


/*
 * XXX: Never, NEVER change the layout of this structure. This **MUST**
 *      follow w_dict_node_t defined in wopt.c.
 * XXX: They key is totally unmodifiable, as it is insane to change it while
 *      traversing the dictionary.
 */
struct ngx_dict_item
{
	void *val;
	const ngx_str_t const* key;
};
typedef struct ngx_dict_item ngx_dict_item_t;


#define ngx_dict_item_first(_d) \
	((ngx_dict_item_t*) ngx_dict_first(_d))

#define ngx_dict_item_next(_d, _i) \
	((ngx_dict_item_t*) ngx_dict_next((_d), (ngx_iterator_t)(_i)))


/*
 * Inline functions
 */

static inline int
ngx_dict_streq(const ngx_str_t *a, const ngx_str_t *b)
{
	register unsigned len;
	u_char *sa, *sb;

	if (a->len != b->len) return 0;

	for (sa = a->data, sb = b->data, len = a->len; len--;)
		if (*sa++ != *sb++) return 0;

	return 1;
}


static inline int
ngx_dict_strneq(const ngx_str_t *a, const ngx_str_t, register unsigned len)
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
nfi_dict_hashnstr(const ngx_str_t *s, register unsigned len)
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

