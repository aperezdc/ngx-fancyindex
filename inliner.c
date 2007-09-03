/*
 * include+cache.c
 * Copyright © 2007 Adrian Perez <adrianperez@udc.es>
 *
 * Distributed under terms of the MIT license.
 */

#include "ngx_http_fancyindex_module.h"
#include <ngx_rbtree.h>


static int s_inliner_init_done = 0;


int
nfi_inliner_init(void)
{
	if (s_inliner_init_done) return 1;
}

