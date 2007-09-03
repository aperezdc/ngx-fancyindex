/*
 * ngx_dict.c
 * Copyright © 2007 Adrian Perez <adrianperez@udc.es>
 */

#ifndef NGX_DICT_DEFAULT_SIZE
#define NGX_DICT_DEFAULT_SIZE 128
#endif /* !NGX_DICT_DEFAULT_SIZE */

#ifndef NGX_DICT_RESIZE_FACTOR
#define NGX_DICT_RESIZE_FACTOR 10
#endif /* !NGX_DICT_RESIZE_FACTOR */

#ifndef NGX_DICT_COUNT_TO_SIZE_RATIO
#define NGX_DICT_COUNT_TO_SIZE_RATIO 1.2
#endif /* !NGX_DICT_COUNT_TO_SIZE_RATIO */

#ifndef NGX_DICT_HASH
#define NGX_DICT_HASH(_k, _s) (ngx_dict_hashstr(_k) % ((_s) - 1))
#endif /* !NGX_DICT_HASH */

#ifndef NGX_DICT_HASHN
#define NGX_DICT_HASHN(_k, _s, _n) (ngx_dict_hashnstr(_k, _n) % ((_s) - 1))
#endif /* !NGX_DICT_HASHN */

#ifndef NFI_DICT_KEY_EQ
#define NFI_DICT_KEY_EQ(_a, _b) (ngx_dict_streq((_a), (_b)))
#endif /* !NGX_DICT_KEY_EQ */

#ifndef NGX_DICT_KEY_EQN
#define NGX_DICT_KEY_EQN(_a, _b, _blen) (ngx_dict_strneq((_a), (_b), (_blen)))
#endif /* !NGX_DICT_KEY_EQN */

#include "ngx_dict.h"


typedef struct ngx_dict_node ngx_dict_node_t;


/*
 * XXX  Never, NEVER, change the layout of this struct. If  XXX
 * XXX  you want to add new fields, ADD FIELDS AT THE END.  XXX
 */

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


static ngx_slab_pool_t s_node_slab;
static ngx_slab_pool_t s_strs_slab;


static inline ngx_dict_node_t*
_ngx_dict_node_new(const ngx_str_t *key, void *val)
{
	ngx_dict_node_t *node;

	node = ngx_slab_alloc(&s_node_slab, sizeof(ngx_dict_node_t));
	node->val = val;
	node->key.len = key->len;
	/* FIXME: Allocate from proper place! */
	node->key.data = ngx_slab_alloc(&s_strs_slab, key->len);
	/* TODO: Copy string data */
	return node;
}


static inline ngx_dict_node_t*
_ngx_dict_node_newn(const ngx_str_t *key, size_t len, void *val)
{
	ngx_dict_node_t *node;

	node = ngx_slab_alloc(&s_node_slab, sizeof(ngx_dict_node_t));
	node->val = val;
	node->key.len = len;
	/* FIXME: Allocate from proper place! */
	node->key.data = ngx_slab_alloc(&s_strs_slab, len);
	/* TODO: Copy string data */
	return node;
}


static inline void
_ngx_dict_node_free(ngx_dict_node_t *node)
{
	/* TODO: What to free and how?
	w_free(node->key);
	w_free(node);
	*/
}


static inline void
_ngx_dict_free_nodes(ngx_dict_t *d)
{
	ngx_dict_node_t *node = d->first;
	ngx_dict_node_t *next;

	while (node) {
		next = node->nextNode;
		_ngx_dict_node_free(node);
		node = next;
	}
}


int
ngx_dict_init(ngx_pool_t *pool, ngx_dict_t *d)
{
	d->size  = NGX_DICT_DEFAULT_SIZE;
	/* TODO: Allocate this one... from where? */
	d->nodes = w_alloc(w_dict_node_t*, d->size);
	return 1;
}


void
ngx_dict_free(ngx_dict_t *d)
{
	_ngx_dict_free_nodes(d);
	/* FIXME: How to deallocate those?
	w_free(d->nodes);
	w_free(d);
	*/
}


unsigned
ngx_dict_count(const ngx_dict_t *d)
{
	return d->count;
}


void
ngx_dict_clear(ngx_dict_t *d)
{
	_ngx_dict_free_nodes(d);
	memset(d->nodes, 0x00, d->size * sizeof(ngx_dict_node_t*));
}


void*
ngx_dict_get(const ngx_dict_t *d, const ngx_str_t *key)
{
	return ngx_dict_getn(d, key, key->len);
}


void*
ngx_dict_getn(const ngx_dict_t *d, const ngx_str_t *key, size_t len)
{
	ngx_dict_node_t *node;
	unsigned hval;

	hval = NGX_DICT_HASHN(key, d->size, len);
	node = d->nodes[hval];

	if (node) {
		if (NGX_DICT_KEY_EQN(node->key, key, len)) {
			return node->val;
		}
		else {
			ngx_dict_node_t *lastNode = node;
			node = node->next;
			while (node) {
				if (NGX_DICT_KEY_EQN(node->key, key, len)) {
					lastNode->next = node->next;
					node->next = d->nodes[hval];
					d->nodes[hval] = node;
					return node->val;
				}
				lastNode = node;
				node = node->next;
			}
		}
	}
	return NULL;
}


static inline void
_ngx_dict_rehash(w_dict_t *d)
{
	ngx_dict_node_t *node = d->first;

	while (node) {
		node->next = NULL;
		node = node->nextNode;
	}

	d->size *= NGX_DICT_RESIZE_FACTOR;
	/* TODO FIXME XXX: How the hell do we resize memory with nginx's model?
	d->nodes = w_resize(d->nodes, w_dict_node_t*, ++d->size);
	*/
	memset(d->nodes, 0x00, d->size * sizeof(ngx_dict_node_t*));

	for (node = d->first; node; node = node->nextNode) {
		unsigned hval = NGX_DICT_HASH(node->key, d->size);
		ngx_dict_node_t *n = d->nodes[hval];
		if (!n) d->nodes[hval] = node;
		else {
			for (;; n = n->next) {
				if (!n->next) {
					n->next = node;
					break;
				}
			}
		}
	}
}


void
ngx_dict_set(ngx_dict_t *d, const char *key, void *val)
{
	ngx_dict_setn(d, key, key->len, val);
}


void
ngx_dict_setn(ngx_dict_t *d, const ngx_str_t *key, size_t len, void *val)
{
	unsigned hval;
	ngx_dict_node_t *node;

	hval = NGX_DICT_HASHN(key, d->size, len);
	node = d->nodes[hval];

	while (node) {
		if (NGX_DICT_KEY_EQN(node->key, key, len)) {
			node->val = val;
			return;
		}
		node = node->next;
	}

	node = _ngx_dict_node_newn(key, len, val);

	if (d->nodes[hval]) node->next = d->nodes[hval];
	d->nodes[hval] = node;

	node->nextNode = d->first;
	if (d->first) d->first->prevNode = node;
	d->first = node;

	d->count++;
	if (d->count > (d->size * NGX_DICT_COUNT_TO_SIZE_RATIO))
		_ngx_dict_rehash(d);
}


void
ngx_dict_del(ngx_dict_t *d, const ngx_str_t *key)
{
	unsigned hval;
	ngx_dict_node_t *node;
	ngx_dict_node_t *lastNode = NULL;

	hval = NGX_DICT_HASH(key, d->size);

	for (node = d->nodes[hval]; node; node = node->next) {
		if (NGX_DICT_KEY_EQ(node->key, key)) {
			ngx_dict_node_t *prevNode = node->prevNode;
			ngx_dict_node_t *nextNode = node->nextNode;

			if (prevNode) prevNode->nextNode = nextNode;
			else d->first = nextNode;
			if (nextNode) nextNode->prevNode = prevNode;

			if (lastNode) lastNode->next = node->next;
			else d->nodes[hval] = node->next;

			_w_dict_node_free(node);
			d->count--;
			return;
		}
	}
}


ngx_dict_iterator_t
ngx_dict_first(const ngx_dict_t *d)
{
	return (ngx_dict_iterator_t) d->first;
}


ngx_dict_iterator_t
ngx_dict_next(const ngx_dict_t *d, ngx_dict_iterator_t i)
{
	ngx_dict_node_t *node = (ngx_dict_node_t*) i;

	(void) d; /* Avoid compiler warnings. */

	return (ngx_dict_iterator_t) node->nextNode;
}


const ngx_str_t const*
ngx_dict_iterator_get_key(ngx_dict_iterator_t i)
{
	ngx_dict_node_t *node = (ngx_dict_node_t*) i;
	return node->key;
}


void
ngx_dict_traverse(ngx_dict_t *d, ngx_dict_traverse_fun_t f, void *ctx)
{
	ngx_dict_iterator_t i;

	for (i = ngx_dict_first(d); i != NULL; i = ngx_dict_next(d, i))
		*i = (*f)(*i, ctx);
}


void
ngx_dict_traverse_keys(ngx_dict_t *d, ngx_dict_traverse_fun_t f, void *ctx)
{
	ngx_dict_item_t *i;

	for (i = ngx_dict_item_first(d); i != NULL; i = ngx_dict_item_next(d, i))
		(*f)((void*) i->key, ctx);
}


void
ngx_dict_traverse_values(ngx_dict_t *d, ngx_dict_traverse_fun_t f, void *ctx)
{
	ngx_dict_item_t *i;

	for (i = ngx_dict_item_first(d); i != NULL; i = ngx_dict_item_next(d, i))
		i->val = (*f)(i->val, ctx);
}

