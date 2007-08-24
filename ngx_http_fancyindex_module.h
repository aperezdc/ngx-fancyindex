/*
 * ngx_http_fancyindex_module.h
 * Copyright © 2007 Adrian Perez <adrianperez@udc.es>
 *
 * Distributed under terms of the BSD license.
 */

#ifndef __ngx_http_fancyindex_module_h__
#define __ngx_http_fancyindex_module_h__

#include "template.h"

#define NFI_TEMPLATE_SIZE \
	( (sizeof(t01_head1) - 1) \
	+ (sizeof(t02_head2) - 1) \
	+ (sizeof(t03_body1) - 1) \
	+ (sizeof(t04_body2) - 1) \
	+ (sizeof(t05_list1) - 1) \
	+ (sizeof(t06_list2) - 1) \
	+ (sizeof(t07_body3) - 1) \
	+ (sizeof(t08_body4) - 1) \
	+ (sizeof(t09_foot1) - 1) )

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

#endif /* !__ngx_http_fancyindex_module_h__ */
/* vim:ft=c
 */
