/*
 * ngx_http_fancyindex_module.h
 * Copyright © 2007 Adrian Perez <adrianperez@udc.es>
 *
 * Distributed under terms of the BSD license.
 */

#ifndef __ngx_http_fancyindex_module_h__
#define __ngx_http_fancyindex_module_h__

#include <ngx_config.h>
#include <ngx_core.h>


#define NGX_HTTP_FANCYINDEX_README_ASIS   0x01
#define NGX_HTTP_FANCYINDEX_README_TOP    0x02
#define NGX_HTTP_FANCYINDEX_README_BOTTOM 0x04
#define NGX_HTTP_FANCYINDEX_README_DIV    0x08
#define NGX_HTTP_FANCYINDEX_README_IFRAME 0x10
#define NGX_HTTP_FANCYINDEX_README_PRE    0x20


/*
 * NGX_HTTP_FANCYINDEX_INCLUDE_STATIC
 *    Cache file contents on first request
 * NGX_HTTP_FANCYINDEX_INCLUDE_CACHED
 *    Cache file contents on first request,
 *    and re-read if needed afterwards
 */
#define NGX_HTTP_FANCYINDEX_INCLUDE_STATIC  0
#define NGX_HTTP_FANCYINDEX_INCLUDE_CACHED  1



/**
 * Configuration structure for the fancyindex module. The configuration
 * commands defined in the module do fill in the members of this structure.
 */
typedef struct {
    ngx_flag_t enable;       /**< Module is enabled. */
    ngx_flag_t localtime;    /**< File mtime dates are sent in local time. */
    ngx_flag_t exact_size;   /**< Sizes are sent always in bytes. */

    ngx_str_t  header;       /**< File name for header, or empty if none. */
    ngx_str_t  footer;       /**< File name for footer, or empty if none. */
    ngx_str_t  readme;       /**< File name for readme, or empty if none. */

    ngx_uint_t readme_flags; /**< Options for readme file inclusion. */
    ngx_uint_t include_mode; /**< Controls how to include footer/header. */
} ngx_http_fancyindex_loc_conf_t;




#define nfi_sizeof_ssz(_s)  (sizeof(_s) - 1)


#define NFI_BUILTIN_HEAD_SIZE ( \
		nfi_sizeof_ssz(t01_head1) + \
		nfi_sizeof_ssz(t02_head2) + \
		nfi_sizeof_ssz(t03_body1) )

#define NFI_BUILTIN_FOOT_SIZE ( \
		nfi_sizeof_ssz(t08_body4) + \
		nfi_sizeof_ssz(t09_foot1) )


#define NGX_HTTP_FANCYINDEX_PREALLOCATE  50
#define NGX_HTTP_FANCYINDEX_NAME_LEN     50


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


#include <ngx_http.h>

/**
 * Get a buffer out of a file name. The buffer will be ready to add to
 * a processing chain (an ngx_chain_t). The "mode" determines how the file
 * will be searched for, it ca be either NGX_HTTP_FANCYINDEX_INCLUDE_STATIC
 * or NGX_HTTP_FANCYINDEX_INCLUDE_CACHED.
 */
ngx_buf_t* nfi_inline_getbuf(ngx_http_request_t *req,
		const ngx_str_t const * path, ngx_int_t mode);


#endif /* !__ngx_http_fancyindex_module_h__ */
/* vim:ft=c
 */
