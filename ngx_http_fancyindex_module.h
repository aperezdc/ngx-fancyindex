/*
 * ngx_http_fancyindex_module.h
 * Copyright © 2007 Adrian Perez <adrianperez@udc.es>
 *
 * Distributed under terms of the BSD license.
 */

#ifndef __ngx_http_fancyindex_module_h__
#define __ngx_http_fancyindex_module_h__

#define nfi_sizeof_ssz(_s)  (sizeof(_s) - 1)


#include "template.h"


/**
 * Copy a static zero-terminated string. Useful to output template
 * string pieces into a temporary buffer.
 */
#define nfi_cpymem_ssz(_p, _t) \
	(ngx_cpymem((_p), (_t), sizeof(_t) - 1))

/**
 * Copy a ngx_str_t.
 */
#define nfi_cpymem_str(_p, _s) \
	(ngx_cpymem((_p), (_s).data, (_s).len))

/**
 * Copy a static zero-terminated string, but only if the string is
 * non-empty. Using this may avoid jumping into ngx_cpymem().
 */
#define nfi_maybe_cpymem_ssz(__p, __t) \
	if ((__t)[0] != '\0') nfi_cpymem_ssz((__p), (__t))

#define nfi_has_flag(_where, _what) \
	(((_where) & (_what)) == (_what))

#endif /* !__ngx_http_fancyindex_module_h__ */
/* vim:ft=c
 */
