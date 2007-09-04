/*
 * inliner.c
 * Copyright © 2007 Adrian Perez <adrianperez@udc.es>
 *
 * Distributed under terms of the MIT license.
 */

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


ngx_buf_t* nfi_inline_getbuf(ngx_http_request_t *r,
		const ngx_str_t const * path, ngx_int_t mode)
{
	ngx_buf_t *b;

	if ((b = ngx_pcalloc(r->pool, sizeof(ngx_buf_t))) == NULL)
		return NULL;

	return b;
}

