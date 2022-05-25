/*
 * ngx_http_fancyindex_module.c
 * Copyright © 2007-2016 Adrian Perez <aperez@igalia.com>
 *
 * Module used for fancy indexing of directories. Features and differences
 * with the stock nginx autoindex module:
 *
 *  - Output is a table instead of a <pre> element with embedded <a> links.
 *  - Header and footer may be added to every generated directory listing.
 *  - Default header and/or footer are generated if custom ones are not
 *    configured. Files used for header and footer can only be local path
 *    names (i.e. you cannot insert the result of a subrequest.)
 *  - Proper HTML is generated: it should validate both as XHTML 1.0 Strict
 *    and HTML 4.01.
 *
 * Base functionality heavy based upon the stock nginx autoindex module,
 * which in turn was made by Igor Sysoev, like the majority of nginx.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <ngx_log.h>

#include "template.h"

#if defined(__GNUC__) && (__GNUC__ >= 3)
# define ngx_force_inline __attribute__((__always_inline__))
#else /* !__GNUC__ */
# define ngx_force_inline
#endif /* __GNUC__ */


static const char *short_weekday[] = {
    "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun",
};
static const char *long_weekday[] = {
    "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Sunday",
};
static const char *short_month[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
};
static const char *long_month[] = {
    "January", "February", "March", "April", "May", "June", "July",
    "August", "September", "October", "November", "December",
};


#define DATETIME_FORMATS(F_, t) \
    F_ ('a',  3, "%3s",  short_weekday[((t)->ngx_tm_wday + 6) % 7]) \
    F_ ('A',  9, "%s",   long_weekday [((t)->ngx_tm_wday + 6) % 7]) \
    F_ ('b',  3, "%3s",  short_month[(t)->ngx_tm_mon - 1]         ) \
    F_ ('B',  9, "%s",   long_month [(t)->ngx_tm_mon - 1]         ) \
    F_ ('d',  2, "%02d", (t)->ngx_tm_mday                         ) \
    F_ ('e',  2, "%2d",  (t)->ngx_tm_mday                         ) \
    F_ ('F', 10, "%d-%02d-%02d",                                    \
                  (t)->ngx_tm_year,                                 \
                  (t)->ngx_tm_mon,                                  \
                  (t)->ngx_tm_mday                                ) \
    F_ ('H',  2, "%02d", (t)->ngx_tm_hour                         ) \
    F_ ('I',  2, "%02d", ((t)->ngx_tm_hour % 12) + 1              ) \
    F_ ('k',  2, "%2d",  (t)->ngx_tm_hour                         ) \
    F_ ('l',  2, "%2d",  ((t)->ngx_tm_hour % 12) + 1              ) \
    F_ ('m',  2, "%02d", (t)->ngx_tm_mon                          ) \
    F_ ('M',  2, "%02d", (t)->ngx_tm_min                          ) \
    F_ ('p',  2, "%2s",  (((t)->ngx_tm_hour < 12) ? "AM" : "PM")  ) \
    F_ ('P',  2, "%2s",  (((t)->ngx_tm_hour < 12) ? "am" : "pm")  ) \
    F_ ('r', 11, "%02d:%02d:%02d %2s",                              \
                 ((t)->ngx_tm_hour % 12) + 1,                       \
                 (t)->ngx_tm_min,                                   \
                 (t)->ngx_tm_sec,                                   \
                 (((t)->ngx_tm_hour < 12) ? "AM" : "PM")          ) \
    F_ ('R',  5, "%02d:%02d", (t)->ngx_tm_hour, (t)->ngx_tm_min   ) \
    F_ ('S',  2, "%02d", (t)->ngx_tm_sec                          ) \
    F_ ('T',  8, "%02d:%02d:%02d",                                  \
                 (t)->ngx_tm_hour,                                  \
                 (t)->ngx_tm_min,                                   \
                 (t)->ngx_tm_sec                                  ) \
    F_ ('u',  1, "%1d", (((t)->ngx_tm_wday + 6) % 7) + 1          ) \
    F_ ('w',  1, "%1d", ((t)->ngx_tm_wday + 6) % 7                ) \
    F_ ('y',  2, "%02d", (t)->ngx_tm_year % 100                   ) \
    F_ ('Y',  4, "%04d", (t)->ngx_tm_year                         )


static size_t
ngx_fancyindex_timefmt_calc_size (const ngx_str_t *fmt)
{
#define DATETIME_CASE(letter, fmtlen, fmt, ...) \
        case letter: result += (fmtlen); break;

    size_t i, result = 0;
    for (i = 0; i < fmt->len; i++) {
        if (fmt->data[i] == '%') {
            if (++i >= fmt->len) {
                result++;
                break;
            }
            switch (fmt->data[i]) {
                DATETIME_FORMATS(DATETIME_CASE,)
                default:
                    result++;
            }
        } else {
            result++;
        }
    }
    return result;

#undef DATETIME_CASE
}


static u_char*
ngx_fancyindex_timefmt (u_char *buffer, const ngx_str_t *fmt, const ngx_tm_t *tm)
{
#define DATETIME_CASE(letter, fmtlen, fmt, ...) \
        case letter: buffer = ngx_snprintf(buffer, fmtlen, fmt, ##__VA_ARGS__); break;

    size_t i;
    for (i = 0; i < fmt->len; i++) {
        if (fmt->data[i] == '%') {
            if (++i >= fmt->len) {
                *buffer++ = '%';
                break;
            }
            switch (fmt->data[i]) {
                DATETIME_FORMATS(DATETIME_CASE, tm)
                default:
                    *buffer++ = fmt->data[i];
            }
        } else {
            *buffer++ = fmt->data[i];
        }
    }
    return buffer;

#undef DATETIME_CASE
}

typedef struct {
    ngx_str_t path;
    ngx_str_t local;
} ngx_fancyindex_headerfooter_conf_t;

/**
 * Configuration structure for the fancyindex module. The configuration
 * commands defined in the module do fill in the members of this structure.
 */
typedef struct {
    ngx_flag_t enable;         /**< Module is enabled. */
    ngx_uint_t default_sort;   /**< Default sort criterion. */
    ngx_flag_t dirs_first;     /**< Group directories together first when sorting */
    ngx_flag_t localtime;      /**< File mtime dates are sent in local time. */
    ngx_flag_t exact_size;     /**< Sizes are sent always in bytes. */
    ngx_flag_t hide_symlinks;  /**< Hide symbolic links in listings. */
    ngx_flag_t show_path;      /**< Whether to display or not the path + '</h1>' after the header */
    ngx_flag_t hide_parent;    /**< Hide parent directory. */
    ngx_flag_t show_dot_files; /**< Show files that start with a dot.*/

    ngx_str_t  css_href;       /**< Link to a CSS stylesheet, or empty if none. */
    ngx_str_t  time_format;    /**< Format used for file timestamps. */

    ngx_array_t *ignore;       /**< List of files to ignore in listings. */

    ngx_fancyindex_headerfooter_conf_t header;
    ngx_fancyindex_headerfooter_conf_t footer;
} ngx_http_fancyindex_loc_conf_t;

#define NGX_HTTP_FANCYINDEX_SORT_CRITERION_NAME       0
#define NGX_HTTP_FANCYINDEX_SORT_CRITERION_SIZE       1
#define NGX_HTTP_FANCYINDEX_SORT_CRITERION_DATE       2
#define NGX_HTTP_FANCYINDEX_SORT_CRITERION_NAME_DESC  3
#define NGX_HTTP_FANCYINDEX_SORT_CRITERION_SIZE_DESC  4
#define NGX_HTTP_FANCYINDEX_SORT_CRITERION_DATE_DESC  5

static ngx_conf_enum_t ngx_http_fancyindex_sort_criteria[] = {
    { ngx_string("name"), NGX_HTTP_FANCYINDEX_SORT_CRITERION_NAME },
    { ngx_string("size"), NGX_HTTP_FANCYINDEX_SORT_CRITERION_SIZE },
    { ngx_string("date"), NGX_HTTP_FANCYINDEX_SORT_CRITERION_DATE },
    { ngx_string("name_desc"), NGX_HTTP_FANCYINDEX_SORT_CRITERION_NAME_DESC },
    { ngx_string("size_desc"), NGX_HTTP_FANCYINDEX_SORT_CRITERION_SIZE_DESC },
    { ngx_string("date_desc"), NGX_HTTP_FANCYINDEX_SORT_CRITERION_DATE_DESC },
    { ngx_null_string, 0 }
};

enum {
    NGX_HTTP_FANCYINDEX_HEADERFOOTER_SUBREQUEST,
    NGX_HTTP_FANCYINDEX_HEADERFOOTER_LOCAL,
};

static ngx_uint_t
headerfooter_kind(const ngx_str_t *value)
{
    static const struct {
        ngx_str_t name;
        ngx_uint_t value;
    } values[] = {
        { ngx_string("subrequest"), NGX_HTTP_FANCYINDEX_HEADERFOOTER_SUBREQUEST },
        { ngx_string("local"), NGX_HTTP_FANCYINDEX_HEADERFOOTER_LOCAL },
    };

    unsigned i;

    for (i = 0; i < sizeof(values) / sizeof(values[0]); i++) {
        if (value->len == values[i].name.len &&
            ngx_strcasecmp(value->data, values[i].name.data) == 0)
        {
            return values[i].value;
        }
    }

    return NGX_CONF_UNSET_UINT;
}

static char*
ngx_fancyindex_conf_set_headerfooter(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_fancyindex_headerfooter_conf_t *item =
        (void*) (((char*) conf) + cmd->offset);
    ngx_str_t *values = cf->args->elts;

    if (item->path.data)
        return "is duplicate";

    item->path = values[1];

    /* Kind of path. Default is "subrequest". */
    ngx_uint_t kind = NGX_HTTP_FANCYINDEX_HEADERFOOTER_SUBREQUEST;
    if (cf->args->nelts == 3) {
        kind = headerfooter_kind(&values[2]);
        if (kind == NGX_CONF_UNSET_UINT) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                               "unknown header/footer kind \"%V\"", &values[2]);
            return NGX_CONF_ERROR;
        }
    }

    if (kind == NGX_HTTP_FANCYINDEX_HEADERFOOTER_LOCAL) {
        ngx_file_t file;
        ngx_file_info_t fi;
        ssize_t n;

        ngx_memzero(&file, sizeof(ngx_file_t));
        file.log = cf->log;
        file.fd = ngx_open_file(item->path.data, NGX_FILE_RDONLY, 0, 0);
        if (file.fd == NGX_INVALID_FILE) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, ngx_errno,
                               "cannot open file \"%V\"", &values[1]);
            return NGX_CONF_ERROR;
        }

        if (ngx_fd_info(file.fd, &fi) == NGX_FILE_ERROR) {
            ngx_close_file(file.fd);
            ngx_conf_log_error(NGX_LOG_EMERG, cf, ngx_errno,
                               "cannot get info for file \"%V\"", &values[1]);
            return NGX_CONF_ERROR;
        }

        item->local.len = ngx_file_size(&fi);
        item->local.data = ngx_pcalloc(cf->pool, item->local.len + 1);
        if (item->local.data == NULL) {
            ngx_close_file(file.fd);
            return NGX_CONF_ERROR;
        }

        n = item->local.len;
        while (n > 0) {
            ssize_t r = ngx_read_file(&file,
                                      item->local.data + file.offset,
                                      n,
                                      file.offset);
            if (r == NGX_ERROR) {
                ngx_close_file(file.fd);
                ngx_conf_log_error(NGX_LOG_EMERG, cf, ngx_errno,
                                   "cannot read file \"%V\"", &values[1]);
                return NGX_CONF_ERROR;
            }

            n -= r;
        }
        item->local.data[item->local.len] = '\0';
    }

    return NGX_CONF_OK;
}

#define NGX_HTTP_FANCYINDEX_PREALLOCATE  50


/**
 * Calculates the length of a NULL-terminated string. It is ugly having to
 * remember to substract 1 from the sizeof result.
 */
#define ngx_sizeof_ssz(_s)  (sizeof(_s) - 1)

/**
 * Compute the length of a statically allocated array
 */
#define DIM(x) (sizeof(x)/sizeof(*(x)))

/**
 * Copy a static zero-terminated string. Useful to output template
 * string pieces into a temporary buffer.
 */
#define ngx_cpymem_ssz(_p, _t) \
	(ngx_cpymem((_p), (_t), sizeof(_t) - 1))

/**
 * Copy a ngx_str_t.
 */
#define ngx_cpymem_str(_p, _s) \
	(ngx_cpymem((_p), (_s).data, (_s).len))

/**
 * Check whether a particular bit is set in a particular value.
 */
#define ngx_has_flag(_where, _what) \
	(((_where) & (_what)) == (_what))




typedef struct {
    ngx_str_t      name;
    size_t         utf_len;
    ngx_uint_t     escape;
    ngx_uint_t     escape_html;
    ngx_uint_t     dir;
    time_t         mtime;
    off_t          size;
} ngx_http_fancyindex_entry_t;



static int ngx_libc_cdecl
    ngx_http_fancyindex_cmp_entries_name_desc(const void *one, const void *two);
static int ngx_libc_cdecl
    ngx_http_fancyindex_cmp_entries_size_desc(const void *one, const void *two);
static int ngx_libc_cdecl
    ngx_http_fancyindex_cmp_entries_mtime_desc(const void *one, const void *two);
static int ngx_libc_cdecl
    ngx_http_fancyindex_cmp_entries_name_asc(const void *one, const void *two);
static int ngx_libc_cdecl
    ngx_http_fancyindex_cmp_entries_size_asc(const void *one, const void *two);
static int ngx_libc_cdecl
    ngx_http_fancyindex_cmp_entries_mtime_asc(const void *one, const void *two);

static ngx_int_t ngx_http_fancyindex_error(ngx_http_request_t *r,
    ngx_dir_t *dir, ngx_str_t *name);

static ngx_int_t ngx_http_fancyindex_init(ngx_conf_t *cf);

static void *ngx_http_fancyindex_create_loc_conf(ngx_conf_t *cf);

static char *ngx_http_fancyindex_merge_loc_conf(ngx_conf_t *cf,
    void *parent, void *child);

static char *ngx_http_fancyindex_ignore(ngx_conf_t    *cf,
                                        ngx_command_t *cmd,
                                        void          *conf);

static uintptr_t
    ngx_fancyindex_escape_filename(u_char *dst, u_char*src, size_t size);

/*
 * These are used only once per handler invocation. We can tell GCC to
 * inline them always, if possible (see how ngx_force_inline is defined
 * above).
 */
static ngx_inline ngx_buf_t*
    make_header_buf(ngx_http_request_t *r, const ngx_str_t css_href)
    ngx_force_inline;


static ngx_command_t  ngx_http_fancyindex_commands[] = {

    { ngx_string("fancyindex"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fancyindex_loc_conf_t, enable),
      NULL },

    { ngx_string("fancyindex_default_sort"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_enum_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fancyindex_loc_conf_t, default_sort),
      &ngx_http_fancyindex_sort_criteria },

    { ngx_string("fancyindex_directories_first"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fancyindex_loc_conf_t, dirs_first),
      NULL },

    { ngx_string("fancyindex_localtime"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fancyindex_loc_conf_t, localtime),
      NULL },

    { ngx_string("fancyindex_exact_size"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fancyindex_loc_conf_t, exact_size),
      NULL },

    { ngx_string("fancyindex_header"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE12,
      ngx_fancyindex_conf_set_headerfooter,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fancyindex_loc_conf_t, header),
      NULL },

    { ngx_string("fancyindex_footer"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE12,
      ngx_fancyindex_conf_set_headerfooter,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fancyindex_loc_conf_t, footer),
      NULL },

    { ngx_string("fancyindex_css_href"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_str_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fancyindex_loc_conf_t, css_href),
      NULL },

    { ngx_string("fancyindex_ignore"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
      ngx_http_fancyindex_ignore,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("fancyindex_hide_symlinks"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fancyindex_loc_conf_t, hide_symlinks),
      NULL },

    { ngx_string("fancyindex_show_path"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fancyindex_loc_conf_t, show_path),
      NULL },

    { ngx_string("fancyindex_show_dotfiles"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fancyindex_loc_conf_t, show_dot_files),
      NULL },

    { ngx_string("fancyindex_hide_parent_dir"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fancyindex_loc_conf_t, hide_parent),
      NULL },

    { ngx_string("fancyindex_time_format"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_str_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fancyindex_loc_conf_t, time_format),
      NULL },

    ngx_null_command
};


static ngx_http_module_t  ngx_http_fancyindex_module_ctx = {
    NULL,                                  /* preconfiguration */
    ngx_http_fancyindex_init,              /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */

    ngx_http_fancyindex_create_loc_conf,   /* create location configuration */
    ngx_http_fancyindex_merge_loc_conf     /* merge location configuration */
};


ngx_module_t  ngx_http_fancyindex_module = {
    NGX_MODULE_V1,
    &ngx_http_fancyindex_module_ctx,       /* module context */
    ngx_http_fancyindex_commands,          /* module directives */
    NGX_HTTP_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};



static const ngx_str_t css_href_pre =
    ngx_string("<link rel=\"stylesheet\" href=\"");
static const ngx_str_t css_href_post =
    ngx_string("\" type=\"text/css\"/>\n");


#ifdef NGX_ESCAPE_URI_COMPONENT
static inline uintptr_t
ngx_fancyindex_escape_filename(u_char *dst, u_char *src, size_t size)
{
    return ngx_escape_uri(dst, src, size, NGX_ESCAPE_URI_COMPONENT);
}
#else /* !NGX_ESCAPE_URI_COMPONENT */
static uintptr_t
ngx_fancyindex_escape_filename(u_char *dst, u_char *src, size_t size)
{
    /*
     * The ngx_escape_uri() function will not escape colons or the
     * ? character, which signals the beginning of the query string.
     * So we handle those characters ourselves.
     *
     * TODO: Get rid of this once ngx_escape_uri() works as expected!
     */

    u_int escapes = 0;
    u_char *psrc = src;
    size_t psize = size;

    while (psize--) {
        switch (*psrc++) {
            case ':':
            case '?':
            case '[':
            case ']':
                escapes++;
                break;
        }
    }

    if (dst == NULL) {
        return escapes + ngx_escape_uri(NULL, src, size, NGX_ESCAPE_HTML);
    }
    else if (escapes == 0) {
        /* No need to do extra escaping, avoid the temporary buffer */
        return ngx_escape_uri(dst, src, size, NGX_ESCAPE_HTML);
    }
    else {
        uintptr_t uescapes = ngx_escape_uri(NULL, src, size, NGX_ESCAPE_HTML);
        size_t bufsz = size + 2 * uescapes;

        /*
         * GCC and CLANG both support stack-allocated variable length
         * arrays. Take advantage of that to avoid a malloc-free cycle.
         */
#if defined(__GNUC__) || defined(__clang__)
        u_char cbuf[bufsz];
        u_char *buf = cbuf;
#else  /* __GNUC__ || __clang__ */
        u_char *buf = (u_char*) malloc(sizeof(u_char) * bufsz);
#endif /* __GNUC__ || __clang__ */

        ngx_escape_uri(buf, src, size, NGX_ESCAPE_HTML);

        while (bufsz--) {
            switch (*buf) {
                case ':':
                    *dst++ = '%';
                    *dst++ = '3';
                    *dst++ = 'A';
                    break;
                case '?':
                    *dst++ = '%';
                    *dst++ = '3';
                    *dst++ = 'F';
                    break;
                case '[':
                    *dst++ = '%';
                    *dst++ = '5';
                    *dst++ = 'B';
                    break;
                case ']':
                    *dst++ = '%';
                    *dst++ = '5';
                    *dst++ = 'D';
                    break;
                default:
                    *dst++ = *buf;
            }
            buf++;
        }

#if !defined(__GNUC__) && !defined(__clang__)
        free(buf);
#endif /* !__GNUC__ && !__clang__ */

        return escapes + uescapes;
    }
}
#endif /* NGX_ESCAPE_URI_COMPONENT */


static ngx_inline ngx_buf_t*
make_header_buf(ngx_http_request_t *r, const ngx_str_t css_href)
{
    ngx_buf_t *b;
    size_t blen = r->uri.len
        + ngx_sizeof_ssz(t01_head1)
        + ngx_sizeof_ssz(t02_head2)
        + ngx_sizeof_ssz(t03_head3)
        + ngx_sizeof_ssz(t04_body1)
        ;

    if (css_href.len) {
        blen += css_href_pre.len \
              + css_href.len \
              + css_href_post.len
              ;
    }

    if ((b = ngx_create_temp_buf(r->pool, blen)) == NULL)
        return NULL;

    b->last = ngx_cpymem_ssz(b->last, t01_head1);

    if (css_href.len) {
        b->last = ngx_cpymem_str(b->last, css_href_pre);
        b->last = ngx_cpymem_str(b->last, css_href);
        b->last = ngx_cpymem_str(b->last, css_href_post);
    }

    b->last = ngx_cpymem_ssz(b->last, t02_head2);
    b->last = ngx_cpymem_str(b->last, r->uri);
    b->last = ngx_cpymem_ssz(b->last, t03_head3);
    b->last = ngx_cpymem_ssz(b->last, t04_body1);

    return b;
}


static ngx_inline ngx_int_t
make_content_buf(
        ngx_http_request_t *r, ngx_buf_t **pb,
        ngx_http_fancyindex_loc_conf_t *alcf)
{
    ngx_http_fancyindex_entry_t *entry;

    int (*sort_cmp_func)(const void *, const void *);
    const char  *sort_url_args = "";

    off_t        length;
    size_t       len, root, allocated, escape_html;
    int64_t      multiplier;
    u_char      *filename, *last;
    ngx_tm_t     tm;
    ngx_array_t  entries;
    ngx_time_t  *tp;
    ngx_uint_t   i, j;
    ngx_str_t    path;
    ngx_dir_t    dir;
    ngx_buf_t   *b;

    static const char    *sizes[]  = { "EiB", "PiB", "TiB", "GiB", "MiB", "KiB", "B" };
    static const int64_t  exbibyte = 1024LL * 1024LL * 1024LL *
                                     1024LL * 1024LL * 1024LL;

    /*
     * NGX_DIR_MASK_LEN is lesser than NGX_HTTP_FANCYINDEX_PREALLOCATE
     */
    if ((last = ngx_http_map_uri_to_path(r, &path, &root,
                    NGX_HTTP_FANCYINDEX_PREALLOCATE)) == NULL)
        return NGX_HTTP_INTERNAL_SERVER_ERROR;

    allocated = path.len;
    path.len = last - path.data;
    if (path.len > 1) {
        path.len--;
    }
    path.data[path.len] = '\0';

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http fancyindex: \"%s\"", path.data);

    if (ngx_open_dir(&path, &dir) == NGX_ERROR) {
        ngx_int_t rc, err = ngx_errno;
        ngx_uint_t level;

        if (err == NGX_ENOENT || err == NGX_ENOTDIR || err == NGX_ENAMETOOLONG) {
            level = NGX_LOG_ERR;
            rc = NGX_HTTP_NOT_FOUND;
        } else if (err == NGX_EACCES) {
            level = NGX_LOG_ERR;
            rc = NGX_HTTP_FORBIDDEN;
        } else {
            level = NGX_LOG_CRIT;
            rc = NGX_HTTP_INTERNAL_SERVER_ERROR;
        }

        ngx_log_error(level, r->connection->log, err,
                ngx_open_dir_n " \"%s\" failed", path.data);

        return rc;
    }

#if (NGX_SUPPRESS_WARN)
    /* MSVC thinks 'entries' may be used without having been initialized */
    ngx_memzero(&entries, sizeof(ngx_array_t));
#endif /* NGX_SUPPRESS_WARN */


    if (ngx_array_init(&entries, r->pool, 40,
                sizeof(ngx_http_fancyindex_entry_t)) != NGX_OK)
        return ngx_http_fancyindex_error(r, &dir, &path);

    filename = path.data;
    filename[path.len] = '/';

    /* Read directory entries and their associated information. */
    for (;;) {
        ngx_set_errno(0);

        if (ngx_read_dir(&dir) == NGX_ERROR) {
            ngx_int_t err = ngx_errno;

            if (err != NGX_ENOMOREFILES) {
                ngx_log_error(NGX_LOG_CRIT, r->connection->log, err,
                        ngx_read_dir_n " \"%V\" failed", &path);
                return ngx_http_fancyindex_error(r, &dir, &path);
            }
            break;
        }

        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                       "http fancyindex file: \"%s\"", ngx_de_name(&dir));

        len = ngx_de_namelen(&dir);

        if (!alcf->show_dot_files && ngx_de_name(&dir)[0] == '.')
            continue;

        if (alcf->hide_symlinks && ngx_de_is_link (&dir))
            continue;

#if NGX_PCRE
        {
            ngx_str_t str;
            str.len = len;
            str.data = ngx_de_name(&dir);

            if (alcf->ignore && ngx_regex_exec_array(alcf->ignore, &str,
                                                     r->connection->log)
                != NGX_DECLINED)
            {
                continue;
            }
        }
#else /* !NGX_PCRE */
        if (alcf->ignore) {
            u_int match_found = 0;
            ngx_str_t *s = alcf->ignore->elts;

            for (i = 0; i < alcf->ignore->nelts; i++, s++) {
                if (ngx_strcmp(ngx_de_name(&dir), s->data) == 0) {
                    match_found = 1;
                    break;
                }
            }

            if (match_found) {
                continue;
            }
        }
#endif /* NGX_PCRE */

        if (!dir.valid_info) {
            /* 1 byte for '/' and 1 byte for terminating '\0' */
            if (path.len + 1 + len + 1 > allocated) {
                allocated = path.len + 1 + len + 1
                          + NGX_HTTP_FANCYINDEX_PREALLOCATE;

                if ((filename = ngx_palloc(r->pool, allocated)) == NULL)
                    return ngx_http_fancyindex_error(r, &dir, &path);

                last = ngx_cpystrn(filename, path.data, path.len + 1);
                *last++ = '/';
            }

            ngx_cpystrn(last, ngx_de_name(&dir), len + 1);

            if (ngx_de_info(filename, &dir) == NGX_FILE_ERROR) {
                ngx_int_t err = ngx_errno;

                if (err != NGX_ENOENT) {
                    ngx_log_error(NGX_LOG_ERR, r->connection->log, err,
                            ngx_de_info_n " \"%s\" failed", filename);
                    continue;
                }

                if (ngx_de_link_info(filename, &dir) == NGX_FILE_ERROR) {
                    ngx_log_error(NGX_LOG_CRIT, r->connection->log, ngx_errno,
                            ngx_de_link_info_n " \"%s\" failed", filename);
                    return ngx_http_fancyindex_error(r, &dir, &path);
                }
            }
        }

        if ((entry = ngx_array_push(&entries)) == NULL)
            return ngx_http_fancyindex_error(r, &dir, &path);

        entry->name.len  = len;
        entry->name.data = ngx_palloc(r->pool, len + 1);
        if (entry->name.data == NULL)
            return ngx_http_fancyindex_error(r, &dir, &path);

        ngx_cpystrn(entry->name.data, ngx_de_name(&dir), len + 1);
        entry->escape = 2 * ngx_fancyindex_escape_filename(NULL,
                                                           ngx_de_name(&dir),
                                                           len);
        entry->escape_html = ngx_escape_html(NULL,
                                             entry->name.data,
                                             entry->name.len);

        entry->dir     = ngx_de_is_dir(&dir);
        entry->mtime   = ngx_de_mtime(&dir);
        entry->size    = ngx_de_size(&dir);
        entry->utf_len = (r->headers_out.charset.len == 5 &&
                ngx_strncasecmp(r->headers_out.charset.data, (u_char*) "utf-8", 5) == 0)
            ?  ngx_utf8_length(entry->name.data, entry->name.len)
            : len;
    }

    if (ngx_close_dir(&dir) == NGX_ERROR) {
        ngx_log_error(NGX_LOG_ALERT, r->connection->log, ngx_errno,
                ngx_close_dir_n " \"%s\" failed", &path);
    }

    /*
     * Calculate needed buffer length.
     */

    escape_html = ngx_escape_html(NULL, r->uri.data, r->uri.len);

    if (alcf->show_path)
        len = r->uri.len + escape_html
          + ngx_sizeof_ssz(t05_body2)
          + ngx_sizeof_ssz(t06_list1)
          + ngx_sizeof_ssz(t_parentdir_entry)
          + ngx_sizeof_ssz(t07_list2)
          + ngx_fancyindex_timefmt_calc_size (&alcf->time_format) * entries.nelts
          ;
   else
        len = r->uri.len + escape_html
          + ngx_sizeof_ssz(t06_list1)
          + ngx_sizeof_ssz(t_parentdir_entry)
          + ngx_sizeof_ssz(t07_list2)
          + ngx_fancyindex_timefmt_calc_size (&alcf->time_format) * entries.nelts
          ;

    /*
     * If we are a the root of the webserver (URI =  "/" --> length of 1),
     * do not display the "Parent Directory" link.
     */
    if (r->uri.len == 1) {
        len -= ngx_sizeof_ssz(t_parentdir_entry);
    }

    entry = entries.elts;
    for (i = 0; i < entries.nelts; i++) {
        /*
         * Genearated table rows are as follows, unneeded whitespace
         * is stripped out:
         *
         *   <tr>
         *     <td><a href="U[?sort]">fname</a></td>
         *     <td>size</td><td>date</td>
         *   </tr>
         */
        len += ngx_sizeof_ssz("<tr><td colspan=\"2\" class=\"link\"><a href=\"")
            + entry[i].name.len + entry[i].escape /* Escaped URL */
            + ngx_sizeof_ssz("?C=x&amp;O=y") /* URL sorting arguments */
            + ngx_sizeof_ssz("\" title=\"")
            + entry[i].name.len + entry[i].utf_len + entry[i].escape_html
            + ngx_sizeof_ssz("\">")
            + entry[i].name.len + entry[i].utf_len + entry[i].escape_html
            + ngx_sizeof_ssz("</a></td><td class=\"size\">")
            + 20 /* File size */
            + ngx_sizeof_ssz("</td><td class=\"date\">")    /* Date prefix */
            + ngx_sizeof_ssz("</td></tr>\n") /* Date suffix */
            + 2 /* CR LF */
            ;
    }

    if ((b = ngx_create_temp_buf(r->pool, len)) == NULL)
        return NGX_HTTP_INTERNAL_SERVER_ERROR;

    /*
     * Determine the sorting criteria. URL arguments look like:
     *
     *    C=x[&O=y]
     *
     * Where x={M,S,N} and y={A,D}
     */
    if ((r->args.len == 3 || (r->args.len == 7 && r->args.data[3] == '&')) &&
        r->args.data[0] == 'C' && r->args.data[1] == '=')
    {
        /* Determine whether the direction of the sorting */
        ngx_int_t sort_descending = r->args.len == 7
                                 && r->args.data[4] == 'O'
                                 && r->args.data[5] == '='
                                 && r->args.data[6] == 'D';

        /* Pick the sorting criteria */
        switch (r->args.data[2]) {
            case 'M': /* Sort by mtime */
                if (sort_descending) {
                    sort_cmp_func = ngx_http_fancyindex_cmp_entries_mtime_desc;
                    if (alcf->default_sort != NGX_HTTP_FANCYINDEX_SORT_CRITERION_DATE_DESC)
                        sort_url_args = "?C=M&amp;O=D";
                }
                else {
                    sort_cmp_func = ngx_http_fancyindex_cmp_entries_mtime_asc;
                    if (alcf->default_sort != NGX_HTTP_FANCYINDEX_SORT_CRITERION_DATE)
                        sort_url_args = "?C=M&amp;O=A";
                }
                break;
            case 'S': /* Sort by size */
                if (sort_descending) {
                    sort_cmp_func = ngx_http_fancyindex_cmp_entries_size_desc;
                    if (alcf->default_sort != NGX_HTTP_FANCYINDEX_SORT_CRITERION_SIZE_DESC)
                        sort_url_args = "?C=S&amp;O=D";
                }
                else {
                    sort_cmp_func = ngx_http_fancyindex_cmp_entries_size_asc;
                        if (alcf->default_sort != NGX_HTTP_FANCYINDEX_SORT_CRITERION_SIZE)
                    sort_url_args = "?C=S&amp;O=A";
                }
                break;
            case 'N': /* Sort by name */
            default:
                if (sort_descending) {
                    sort_cmp_func = ngx_http_fancyindex_cmp_entries_name_desc;
                    if (alcf->default_sort != NGX_HTTP_FANCYINDEX_SORT_CRITERION_NAME_DESC)
                        sort_url_args = "?C=N&amp;O=D";
                }
                else {
                    sort_cmp_func = ngx_http_fancyindex_cmp_entries_name_asc;
                    if (alcf->default_sort != NGX_HTTP_FANCYINDEX_SORT_CRITERION_NAME)
                        sort_url_args = "?C=N&amp;O=A";
                }
                break;
        }
    }
    else {
        switch (alcf->default_sort) {
            case NGX_HTTP_FANCYINDEX_SORT_CRITERION_DATE_DESC:
                sort_cmp_func = ngx_http_fancyindex_cmp_entries_mtime_desc;
                break;
            case NGX_HTTP_FANCYINDEX_SORT_CRITERION_DATE:
                sort_cmp_func = ngx_http_fancyindex_cmp_entries_mtime_asc;
                break;
            case NGX_HTTP_FANCYINDEX_SORT_CRITERION_SIZE_DESC:
                sort_cmp_func = ngx_http_fancyindex_cmp_entries_size_desc;
                break;
            case NGX_HTTP_FANCYINDEX_SORT_CRITERION_SIZE:
                sort_cmp_func = ngx_http_fancyindex_cmp_entries_size_asc;
                break;
            case NGX_HTTP_FANCYINDEX_SORT_CRITERION_NAME_DESC:
                sort_cmp_func = ngx_http_fancyindex_cmp_entries_name_desc;
                break;
            case NGX_HTTP_FANCYINDEX_SORT_CRITERION_NAME:
            default:
                sort_cmp_func = ngx_http_fancyindex_cmp_entries_name_asc;
                break;
        }
    }

    /* Sort entries, if needed */
    if (entries.nelts > 1) {
        if (alcf->dirs_first)
        {
            ngx_http_fancyindex_entry_t *l, *r;

            l = entry;
            r = entry + entries.nelts - 1;
            while (l < r)
            {
                while (l < r && l->dir)
                    l++;
                while (l < r && !r->dir)
                    r--;
                if (l < r) {
                    /* Now l points a file while r points a directory */
                    ngx_http_fancyindex_entry_t tmp;
                    tmp = *l;
                    *l = *r;
                    *r = tmp;
                }
            }
            if (r->dir)
                r++;

            if (r > entry)
                /* Sort directories */
                ngx_qsort(entry, (size_t)(r - entry),
                        sizeof(ngx_http_fancyindex_entry_t), sort_cmp_func);
            if (r < entry + entries.nelts)
                /* Sort files */
                ngx_qsort(r, (size_t)(entry + entries.nelts - r),
                        sizeof(ngx_http_fancyindex_entry_t), sort_cmp_func);
        } else {
            ngx_qsort(entry, (size_t)entries.nelts,
                    sizeof(ngx_http_fancyindex_entry_t), sort_cmp_func);
        }
    }

    /* Display the path, if needed */
    if (alcf->show_path){
        b->last = last = (u_char *) ngx_escape_html(b->last, r->uri.data, r->uri.len);
        b->last = ngx_cpymem_ssz(b->last, t05_body2);
    }

    /* Open the <table> tag */
    b->last = ngx_cpymem_ssz(b->last, t06_list1);

    tp = ngx_timeofday();

    /* "Parent dir" entry, always first if displayed */
    if (r->uri.len > 1 && alcf->hide_parent == 0) {
        b->last = ngx_cpymem_ssz(b->last,
                                 "<tr>"
                                 "<td colspan=\"2\" class=\"link\"><a href=\"../");
        if (*sort_url_args) {
            b->last = ngx_cpymem(b->last,
                                 sort_url_args,
                                 ngx_sizeof_ssz("?C=N&amp;O=A"));
        }
        b->last = ngx_cpymem_ssz(b->last,
                                 "\">Parent directory/</a></td>"
                                 "<td class=\"size\">-</td>"
                                 "<td class=\"date\">-</td>"
                                 "</tr>"
                                 CRLF);
    }

    /* Entries for directories and files */
    for (i = 0; i < entries.nelts; i++) {
        b->last = ngx_cpymem_ssz(b->last, "<tr><td colspan=\"2\" class=\"link\"><a href=\"");

        if (entry[i].escape) {
            ngx_fancyindex_escape_filename(b->last,
                                           entry[i].name.data,
                                           entry[i].name.len);

            b->last += entry[i].name.len + entry[i].escape;

        } else {
            b->last = ngx_cpymem_str(b->last, entry[i].name);
        }

        if (entry[i].dir) {
            *b->last++ = '/';
            if (*sort_url_args) {
                b->last = ngx_cpymem(b->last,
                                     sort_url_args,
                                     ngx_sizeof_ssz("?C=x&amp;O=y"));
            }
        }

        *b->last++ = '"';
        b->last = ngx_cpymem_ssz(b->last, " title=\"");
        b->last = (u_char *) ngx_escape_html(b->last, entry[i].name.data, entry[i].name.len);
        *b->last++ = '"';
        *b->last++ = '>';

        len = entry[i].utf_len;

        b->last = (u_char *) ngx_escape_html(b->last, entry[i].name.data, entry[i].name.len);
        last = b->last - 3;

        if (entry[i].dir) {
            *b->last++ = '/';
            len++;
        }

        b->last = ngx_cpymem_ssz(b->last, "</a></td><td class=\"size\">");

        if (alcf->exact_size) {
            if (entry[i].dir) {
                *b->last++ = '-';
            } else {
                b->last = ngx_sprintf(b->last, "%19O", entry[i].size);
            }

        } else {
            if (entry[i].dir) {
                *b->last++ = '-';
            } else {
                length = entry[i].size;
                multiplier = exbibyte;

                for (j = 0; j < DIM(sizes) - 1 && length < multiplier; j++)
                    multiplier /= 1024;

                /* If we are showing the filesize in bytes, do not show a decimal */
                if (j == DIM(sizes) - 1)
                    b->last = ngx_sprintf(b->last, "%O %s", length, sizes[j]);
                else
                    b->last = ngx_sprintf(b->last, "%.1f %s",
                                          (float) length / multiplier, sizes[j]);
            }
        }

        ngx_gmtime(entry[i].mtime + tp->gmtoff * 60 * alcf->localtime, &tm);
        b->last = ngx_cpymem_ssz(b->last, "</td><td class=\"date\">");
        b->last = ngx_fancyindex_timefmt(b->last, &alcf->time_format, &tm);
        b->last = ngx_cpymem_ssz(b->last, "</td></tr>");

        *b->last++ = CR;
        *b->last++ = LF;
    }

    /* Output table bottom */
    b->last = ngx_cpymem_ssz(b->last, t07_list2);

    *pb = b;
    return NGX_OK;
}



static ngx_int_t
ngx_http_fancyindex_handler(ngx_http_request_t *r)
{
    ngx_http_request_t             *sr;
    ngx_str_t                      *sr_uri;
    ngx_str_t                       rel_uri;
    ngx_int_t                       rc;
    ngx_http_fancyindex_loc_conf_t *alcf;
    ngx_chain_t                     out[3] = {
        { NULL, NULL }, { NULL, NULL}, { NULL, NULL }};


    if (r->uri.data[r->uri.len - 1] != '/') {
        return NGX_DECLINED;
    }

    /* TODO: Win32 */
#if defined(nginx_version) \
    && ((nginx_version < 7066) \
        || ((nginx_version > 8000) && (nginx_version < 8038)))
    if (r->zero_in_uri) {
        return NGX_DECLINED;
    }
#endif

    if (!(r->method & (NGX_HTTP_GET|NGX_HTTP_HEAD))) {
        return NGX_DECLINED;
    }

    alcf = ngx_http_get_module_loc_conf(r, ngx_http_fancyindex_module);

    if (!alcf->enable) {
        return NGX_DECLINED;
    }

    if ((rc = make_content_buf(r, &out[0].buf, alcf)) != NGX_OK)
        return rc;

    out[0].buf->last_in_chain = 1;

    r->headers_out.status = NGX_HTTP_OK;
    r->headers_out.content_type_len  = ngx_sizeof_ssz("text/html");
    r->headers_out.content_type.len  = ngx_sizeof_ssz("text/html");
    r->headers_out.content_type.data = (u_char *) "text/html";

    rc = ngx_http_send_header(r);
    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only)
        return rc;

    if (alcf->header.path.len > 0 && alcf->header.local.len == 0) {
        /* URI is configured, make Nginx take care of with a subrequest. */
        sr_uri = &alcf->header.path;

        if (*sr_uri->data != '/') {
            /* Relative path */
            rel_uri.len  = r->uri.len + alcf->header.path.len;
            rel_uri.data = ngx_palloc(r->pool, rel_uri.len);
            if (rel_uri.data == NULL) {
                return NGX_HTTP_INTERNAL_SERVER_ERROR;
            }
            ngx_memcpy(ngx_cpymem(rel_uri.data, r->uri.data, r->uri.len),
                    alcf->header.path.data, alcf->header.path.len);
            sr_uri = &rel_uri;
        }

        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                "http fancyindex: header subrequest \"%V\"", sr_uri);

        rc = ngx_http_subrequest(r, sr_uri, NULL, &sr, NULL, 0);
        if (rc == NGX_ERROR || rc == NGX_DONE) {
            ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                    "http fancyindex: header subrequest for \"%V\" failed", sr_uri);
            return rc;
        }

        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                "http fancyindex: header subrequest status = %i",
                sr->headers_out.status);
        /* ngx_http_subrequest returns NGX_OK(0), not NGX_HTTP_OK(200) */
        if (sr->headers_out.status != NGX_OK) {
            /*
             * XXX: Should we write a message to the error log just in case
             * we get something different from a 404?
             */
            goto add_builtin_header;
        }
    }
    else {
add_builtin_header:
        /* Make space before */
        out[1].next = out[0].next;
        out[1].buf  = out[0].buf;
        /* Chain header buffer */
        out[0].next = &out[1];
        if (alcf->header.local.len > 0) {
            /* Header buffer is local, make a buffer pointing to the data. */
            out[0].buf = ngx_calloc_buf(r->pool);
            if (out[0].buf == NULL)
                return NGX_ERROR;
            out[0].buf->memory = 1;
            out[0].buf->pos = alcf->header.local.data;
            out[0].buf->last = alcf->header.local.data + alcf->header.local.len;
        } else {
            /* Prepare a buffer with the contents of the builtin header. */
            out[0].buf = make_header_buf(r, alcf->css_href);
        }
    }

    /* If footer is disabled, chain up footer buffer. */
    if (alcf->footer.path.len == 0 || alcf->footer.local.len > 0) {
        ngx_uint_t last = (alcf->header.path.len == 0) ? 2 : 1;

        out[last-1].next = &out[last];
        out[last].buf = ngx_calloc_buf(r->pool);
        if (out[last].buf == NULL)
            return NGX_ERROR;

        out[last].buf->memory = 1;
        if (alcf->footer.local.len > 0) {
            out[last].buf->pos = alcf->footer.local.data;
            out[last].buf->last = alcf->footer.local.data + alcf->footer.local.len;
        } else {
            out[last].buf->pos = (u_char*) t08_foot1;
            out[last].buf->last = (u_char*) t08_foot1 + sizeof(t08_foot1) - 1;
        }

        out[last-1].buf->last_in_chain = 0;
        out[last].buf->last_in_chain   = 1;
        out[last].buf->last_buf        = 1;
        /* Send everything with a single call :D */
        return ngx_http_output_filter(r, &out[0]);
    }

    /*
     * If we reach here, we were asked to send a custom footer. We need to:
     * partially send whatever is referenced from out[0] and then send the
     * footer as a subrequest. If the subrequest fails, we should send the
     * standard footer as well.
     */
    rc = ngx_http_output_filter(r, &out[0]);

    if (rc != NGX_OK && rc != NGX_AGAIN)
        return NGX_HTTP_INTERNAL_SERVER_ERROR;

    /* URI is configured, make Nginx take care of with a subrequest. */
    sr_uri = &alcf->footer.path;

    if (*sr_uri->data != '/') {
        /* Relative path */
        rel_uri.len  = r->uri.len + alcf->footer.path.len;
        rel_uri.data = ngx_palloc(r->pool, rel_uri.len);
        if (rel_uri.data == NULL) {
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
        ngx_memcpy(ngx_cpymem(rel_uri.data, r->uri.data, r->uri.len),
                alcf->footer.path.data, alcf->footer.path.len);
        sr_uri = &rel_uri;
    }

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
            "http fancyindex: footer subrequest \"%V\"", sr_uri);

    rc = ngx_http_subrequest(r, sr_uri, NULL, &sr, NULL, 0);
    if (rc == NGX_ERROR || rc == NGX_DONE) {
        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                "http fancyindex: footer subrequest for \"%V\" failed", sr_uri);
        return rc;
    }

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
            "http fancyindex: header subrequest status = %i",
            sr->headers_out.status);

    /* see above: ngx_http_subrequest resturns NGX_OK (0) not NGX_HTTP_OK (200) */
    if (sr->headers_out.status != NGX_OK) {
        /*
         * XXX: Should we write a message to the error log just in case
         * we get something different from a 404?
         */
        out[0].next = NULL;
        out[0].buf = ngx_calloc_buf(r->pool);
        if (out[0].buf == NULL)
            return NGX_ERROR;
        out[0].buf->memory = 1;
        out[0].buf->pos = (u_char*) t08_foot1;
        out[0].buf->last = (u_char*) t08_foot1 + sizeof(t08_foot1) - 1;
        out[0].buf->last_in_chain = 1;
        out[0].buf->last_buf = 1;
        /* Directly send out the builtin footer */
        return ngx_http_output_filter(r, &out[0]);
    }

    return (r != r->main) ? rc : ngx_http_send_special(r, NGX_HTTP_LAST);
}


static int ngx_libc_cdecl
ngx_http_fancyindex_cmp_entries_name_desc(const void *one, const void *two)
{
    ngx_http_fancyindex_entry_t *first = (ngx_http_fancyindex_entry_t *) one;
    ngx_http_fancyindex_entry_t *second = (ngx_http_fancyindex_entry_t *) two;

    return (int) ngx_strcmp(second->name.data, first->name.data);
}


static int ngx_libc_cdecl
ngx_http_fancyindex_cmp_entries_size_desc(const void *one, const void *two)
{
    ngx_http_fancyindex_entry_t *first = (ngx_http_fancyindex_entry_t *) one;
    ngx_http_fancyindex_entry_t *second = (ngx_http_fancyindex_entry_t *) two;

    return (first->size < second->size) - (first->size > second->size);
}


static int ngx_libc_cdecl
ngx_http_fancyindex_cmp_entries_mtime_desc(const void *one, const void *two)
{
    ngx_http_fancyindex_entry_t *first = (ngx_http_fancyindex_entry_t *) one;
    ngx_http_fancyindex_entry_t *second = (ngx_http_fancyindex_entry_t *) two;

    return (int) (second->mtime - first->mtime);
}


static int ngx_libc_cdecl
ngx_http_fancyindex_cmp_entries_name_asc(const void *one, const void *two)
{
    ngx_http_fancyindex_entry_t *first = (ngx_http_fancyindex_entry_t *) one;
    ngx_http_fancyindex_entry_t *second = (ngx_http_fancyindex_entry_t *) two;

    return (int) ngx_strcmp(first->name.data, second->name.data);
}


static int ngx_libc_cdecl
ngx_http_fancyindex_cmp_entries_size_asc(const void *one, const void *two)
{
    ngx_http_fancyindex_entry_t *first = (ngx_http_fancyindex_entry_t *) one;
    ngx_http_fancyindex_entry_t *second = (ngx_http_fancyindex_entry_t *) two;

    return (first->size > second->size) - (first->size < second->size);
}


static int ngx_libc_cdecl
ngx_http_fancyindex_cmp_entries_mtime_asc(const void *one, const void *two)
{
    ngx_http_fancyindex_entry_t *first = (ngx_http_fancyindex_entry_t *) one;
    ngx_http_fancyindex_entry_t *second = (ngx_http_fancyindex_entry_t *) two;

    return (int) (first->mtime - second->mtime);
}


static ngx_int_t
ngx_http_fancyindex_error(ngx_http_request_t *r, ngx_dir_t *dir, ngx_str_t *name)
{
    if (ngx_close_dir(dir) == NGX_ERROR) {
        ngx_log_error(NGX_LOG_ALERT, r->connection->log, ngx_errno,
                      ngx_close_dir_n " \"%V\" failed", name);
    }

    return NGX_HTTP_INTERNAL_SERVER_ERROR;
}


static void *
ngx_http_fancyindex_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_fancyindex_loc_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_fancyindex_loc_conf_t));
    if (conf == NULL) {
        return NGX_CONF_ERROR;
    }

    /*
     * Set by ngx_pcalloc:
     *    conf->header.*.len     = 0
     *    conf->header.*.data    = NULL
     *    conf->footer.*.len     = 0
     *    conf->footer.*.data    = NULL
     *    conf->css_href.len     = 0
     *    conf->css_href.data    = NULL
     *    conf->time_format.len  = 0
     *    conf->time_format.data = NULL
     */
    conf->enable         = NGX_CONF_UNSET;
    conf->default_sort   = NGX_CONF_UNSET_UINT;
    conf->dirs_first     = NGX_CONF_UNSET;
    conf->localtime      = NGX_CONF_UNSET;
    conf->exact_size     = NGX_CONF_UNSET;
    conf->ignore         = NGX_CONF_UNSET_PTR;
    conf->hide_symlinks  = NGX_CONF_UNSET;
    conf->show_path      = NGX_CONF_UNSET;
    conf->hide_parent    = NGX_CONF_UNSET;
    conf->show_dot_files = NGX_CONF_UNSET;

    return conf;
}


static char *
ngx_http_fancyindex_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_fancyindex_loc_conf_t *prev = parent;
    ngx_http_fancyindex_loc_conf_t *conf = child;

    (void) cf; /* unused */

    ngx_conf_merge_value(conf->enable, prev->enable, 0);
    ngx_conf_merge_uint_value(conf->default_sort, prev->default_sort, NGX_HTTP_FANCYINDEX_SORT_CRITERION_NAME);
    ngx_conf_merge_value(conf->dirs_first, prev->dirs_first, 1);
    ngx_conf_merge_value(conf->localtime, prev->localtime, 0);
    ngx_conf_merge_value(conf->exact_size, prev->exact_size, 1);
    ngx_conf_merge_value(conf->show_path, prev->show_path, 1);
    ngx_conf_merge_value(conf->show_dot_files, prev->show_dot_files, 0);

    ngx_conf_merge_str_value(conf->header.path, prev->header.path, "");
    ngx_conf_merge_str_value(conf->header.path, prev->header.local, "");
    ngx_conf_merge_str_value(conf->footer.path, prev->footer.path, "");
    ngx_conf_merge_str_value(conf->footer.path, prev->footer.local, "");

    ngx_conf_merge_str_value(conf->css_href, prev->css_href, "");
    ngx_conf_merge_str_value(conf->time_format, prev->time_format, "%Y-%b-%d %H:%M");

    ngx_conf_merge_ptr_value(conf->ignore, prev->ignore, NULL);
    ngx_conf_merge_value(conf->hide_symlinks, prev->hide_symlinks, 0);
    ngx_conf_merge_value(conf->hide_parent, prev->hide_parent, 0);

    /* Just make sure we haven't disabled the show_path directive without providing a custom header */
    if (conf->show_path == 0 && conf->header.path.len == 0)
    {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "FancyIndex : cannot set show_path to off without providing a custom header !");
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}


static char*
ngx_http_fancyindex_ignore(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_fancyindex_loc_conf_t *alcf = conf;
    ngx_str_t *value;

    (void) cmd; /* unused */

#if (NGX_PCRE)
    ngx_uint_t          i;
    ngx_regex_elt_t    *re;
    ngx_regex_compile_t rc;
    u_char              errstr[NGX_MAX_CONF_ERRSTR];

    if (alcf->ignore == NGX_CONF_UNSET_PTR) {
        alcf->ignore = ngx_array_create(cf->pool, 2, sizeof(ngx_regex_elt_t));
        if (alcf->ignore == NULL) {
            return NGX_CONF_ERROR;
        }
    }

    value = cf->args->elts;

    ngx_memzero(&rc, sizeof(ngx_regex_compile_t));

    rc.err.data = errstr;
    rc.err.len  = NGX_MAX_CONF_ERRSTR;
    rc.pool     = cf->pool;

    for (i = 1; i < cf->args->nelts; i++) {
        re = ngx_array_push(alcf->ignore);
        if (re == NULL) {
            return NGX_CONF_ERROR;
        }

        rc.pattern = value[i];
        rc.options = NGX_REGEX_CASELESS;

        if (ngx_regex_compile(&rc) != NGX_OK) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "%V", &rc.err);
            return NGX_CONF_ERROR;
        }

        re->name  = value[i].data;
        re->regex = rc.regex;
    }

    return NGX_CONF_OK;
#else /* !NGX_PCRE */
    ngx_uint_t i;
    ngx_str_t *str;

    if (alcf->ignore == NGX_CONF_UNSET_PTR) {
        alcf->ignore = ngx_array_create(cf->pool, 2, sizeof(ngx_str_t));
        if (alcf->ignore == NULL) {
            return NGX_CONF_ERROR;
        }
    }

    value = cf->args->elts;

    for (i = 1; i < cf->args->nelts; i++) {
        str = ngx_array_push(alcf->ignore);
        if (str == NULL) {
            return NGX_CONF_ERROR;
        }

        str->data = value[i].data;
        str->len  = value[i].len;
    }

    return NGX_CONF_OK;
#endif /* NGX_PCRE */

}


static ngx_int_t
ngx_http_fancyindex_init(ngx_conf_t *cf)
{
    ngx_http_handler_pt        *h;
    ngx_http_core_main_conf_t  *cmcf;

    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

    h = ngx_array_push(&cmcf->phases[NGX_HTTP_CONTENT_PHASE].handlers);
    if (h == NULL) {
        return NGX_ERROR;
    }

    *h = ngx_http_fancyindex_handler;

    return NGX_OK;
}

/* vim:et:sw=4:ts=4:
 */
