
#ifndef __templates_inc__
#define __templates_inc__

static const u_char t01_head1[] =
#include "01-head1.inc"
;
static const u_char t02_head2[] =
#include "02-head2.inc"
;
static const u_char t03_body1[] =
#include "03-body1.inc"
;
static const u_char t04_body2[] =
#include "04-body2.inc"
;
static const u_char t05_list1[] =
#include "05-list1.inc"
;
static const u_char t06_list2[] =
#include "06-list2.inc"
;
static const u_char t07_body3[] =
#include "07-body3.inc"
;
static const u_char t08_body4[] =
#include "08-body4.inc"
;
static const u_char t09_foot1[] =
#include "09-foot1.inc"
;

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

#endif /* !__templates_inc__ */
/* vim:ft=c
 */
