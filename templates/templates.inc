
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


static const u_char t09_foot1[] =
#include "09-foot1.inc"
;

#define NGX_HTTP_FANCYINDEX_TEMPLATE_SIZE \
        ( (sizeof(t01_head1) - 1) \
        + (sizeof(t02_head2) - 1) \
        + (sizeof(t03_body1) - 1) \
        + (sizeof(t04_body2) - 1) \
        \
        + (sizeof(t09_foot1) - 1) )

#endif /* !__templates_inc__ */
/* vim:ft=c
 */
