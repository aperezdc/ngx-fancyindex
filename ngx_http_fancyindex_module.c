/*
 * ngx_http_fancyindex_module.c
 * Copyright © 2007 Adrian Perez <adrianperez@udc.es>
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
 * Distributed under terms of the BSD license.
 */

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include "ngx_http_fancyindex_module.h"

#if 0

typedef struct {
    ngx_buf_t     *buf;
    size_t         size;
    ngx_pool_t    *pool;
    size_t         alloc_size;
    ngx_chain_t  **last_out;
} ngx_http_fancyindex_ctx_t;

#endif

#define NGX_HTTP_FANCYINDEX_README_TOP    0x00
#define NGX_HTTP_FANCYINDEX_README_PRE    0x00
#define NGX_HTTP_FANCYINDEX_README_ASIS   0x01
#define NGX_HTTP_FANCYINDEX_README_BOTTOM 0x02
#define NGX_HTTP_FANCYINDEX_README_DIV    0x04
#define NGX_HTTP_FANCYINDEX_README_IFRAME 0x08


typedef struct {
    ngx_str_t      name;
    size_t         utf_len;
    ngx_uint_t     escape;
    ngx_uint_t     dir;
    time_t         mtime;
    off_t          size;
} ngx_http_fancyindex_entry_t;


typedef struct {
    ngx_flag_t     enable;
    ngx_flag_t     localtime;
    ngx_flag_t     exact_size;

    ngx_str_t      header;
    ngx_str_t      footer;
    ngx_str_t      readme;

    ngx_uint_t     readme_flags;
} ngx_http_fancyindex_loc_conf_t;


#define NGX_HTTP_FANCYINDEX_PREALLOCATE  50
#define NGX_HTTP_FANCYINDEX_NAME_LEN     50


static int ngx_libc_cdecl ngx_http_fancyindex_cmp_entries(const void *one,
    const void *two);
static ngx_int_t ngx_http_fancyindex_error(ngx_http_request_t *r,
    ngx_dir_t *dir, ngx_str_t *name);
static ngx_int_t ngx_http_fancyindex_init(ngx_conf_t *cf);
static void *ngx_http_fancyindex_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_fancyindex_merge_loc_conf(ngx_conf_t *cf,
    void *parent, void *child);



static ngx_conf_bitmask_t ngx_http_fancyindex_readme_flags[] = {
    { ngx_string("pre"),    NGX_HTTP_FANCYINDEX_README_PRE    },
    { ngx_string("asis"),   NGX_HTTP_FANCYINDEX_README_ASIS   },
    { ngx_string("top"),    NGX_HTTP_FANCYINDEX_README_TOP    },
    { ngx_string("bottom"), NGX_HTTP_FANCYINDEX_README_BOTTOM },
    { ngx_string("div"),    NGX_HTTP_FANCYINDEX_README_DIV    },
    { ngx_string("iframe"), NGX_HTTP_FANCYINDEX_README_IFRAME },
    { ngx_null_string,      0                                 },
};


static ngx_command_t  ngx_http_fancyindex_commands[] = {

    { ngx_string("fancyindex"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fancyindex_loc_conf_t, enable),
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
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_str_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fancyindex_loc_conf_t, header),
      NULL },

    { ngx_string("fancyindex_footer"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_str_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fancyindex_loc_conf_t, footer),
      NULL },

    { ngx_string("fancyindex_readme"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fancyindex_loc_conf_t, readme),
      NULL },

    { ngx_string("fancyindex_readme_options"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_bitmask_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_fancyindex_loc_conf_t, readme_flags),
      &ngx_http_fancyindex_readme_flags },

      ngx_null_command
};


static ngx_http_module_t  ngx_http_fancyindex_module_ctx = {
    NULL,                                  /* preconfiguration */
    ngx_http_fancyindex_init,              /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */

    ngx_http_fancyindex_create_loc_conf,   /* create location configration */
    ngx_http_fancyindex_merge_loc_conf     /* merge location configration */
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




static ngx_int_t
ngx_http_fancyindex_handler(ngx_http_request_t *r)
{
    u_char                         *last, *filename, scale;
    off_t                           length;
    size_t                          len, copy, allocated, root;
    ngx_tm_t                        tm;
    ngx_err_t                       err;
    ngx_buf_t                      *b;
    ngx_int_t                       rc, size;
    ngx_str_t                       path;
    ngx_dir_t                       dir;
    ngx_uint_t                      i, level;
    ngx_pool_t                     *pool;
    ngx_time_t                     *tp;
    ngx_chain_t                     out;
    ngx_array_t                     entries;
    ngx_http_fancyindex_entry_t    *entry;
    ngx_http_fancyindex_loc_conf_t *alcf;

    static char  *months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                               "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

    if (r->uri.data[r->uri.len - 1] != '/') {
        return NGX_DECLINED;
    }

    /* TODO: Win32 */
    if (r->zero_in_uri) {
        return NGX_DECLINED;
    }

    if (!(r->method & (NGX_HTTP_GET|NGX_HTTP_HEAD))) {
        return NGX_DECLINED;
    }

    alcf = ngx_http_get_module_loc_conf(r, ngx_http_fancyindex_module);

    if (!alcf->enable) {
        return NGX_DECLINED;
    }

    /* NGX_DIR_MASK_LEN is lesser than NGX_HTTP_FANCYINDEX_PREALLOCATE */

    last = ngx_http_map_uri_to_path(r, &path, &root,
                                    NGX_HTTP_FANCYINDEX_PREALLOCATE);
    if (last == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    allocated = path.len;
    path.len = last - path.data - 1;
    path.data[path.len] = '\0';

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "http fancyindex: \"%s\"", path.data);

    if (ngx_open_dir(&path, &dir) == NGX_ERROR) {
        err = ngx_errno;

        if (err == NGX_ENOENT
            || err == NGX_ENOTDIR
            || err == NGX_ENAMETOOLONG)
        {
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

#endif

    /* TODO: pool should be temporary pool */
    pool = r->pool;

    if (ngx_array_init(&entries, pool, 40, sizeof(ngx_http_fancyindex_entry_t))
        != NGX_OK)
    {
        return ngx_http_fancyindex_error(r, &dir, &path);
    }

    r->headers_out.status = NGX_HTTP_OK;
    r->headers_out.content_type_len = sizeof("text/html") - 1;
    r->headers_out.content_type.len = sizeof("text/html") - 1;
    r->headers_out.content_type.data = (u_char *) "text/html";

    rc = ngx_http_send_header(r);

    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
        return rc;
    }

    filename = path.data;
    filename[path.len] = '/';

    for ( ;; ) {
        ngx_set_errno(0);

        if (ngx_read_dir(&dir) == NGX_ERROR) {
            err = ngx_errno;

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

        if (ngx_de_name(&dir)[0] == '.') {
            continue;
        }

        if (!dir.valid_info) {

            /* 1 byte for '/' and 1 byte for terminating '\0' */

            if (path.len + 1 + len + 1 > allocated) {
                allocated = path.len + 1 + len + 1
                                     + NGX_HTTP_FANCYINDEX_PREALLOCATE;

                filename = ngx_palloc(pool, allocated);
                if (filename == NULL) {
                    return ngx_http_fancyindex_error(r, &dir, &path);
                }

                last = ngx_cpystrn(filename, path.data, path.len + 1);
                *last++ = '/';
            }

            ngx_cpystrn(last, ngx_de_name(&dir), len + 1);

            if (ngx_de_info(filename, &dir) == NGX_FILE_ERROR) {
                err = ngx_errno;

                if (err != NGX_ENOENT) {
                    ngx_log_error(NGX_LOG_CRIT, r->connection->log, err,
                                  ngx_de_info_n " \"%s\" failed", filename);
                    return ngx_http_fancyindex_error(r, &dir, &path);
                }

                if (ngx_de_link_info(filename, &dir) == NGX_FILE_ERROR) {
                    ngx_log_error(NGX_LOG_CRIT, r->connection->log, ngx_errno,
                                  ngx_de_link_info_n " \"%s\" failed",
                                  filename);
                    return ngx_http_fancyindex_error(r, &dir, &path);
                }
            }
        }

        entry = ngx_array_push(&entries);
        if (entry == NULL) {
            return ngx_http_fancyindex_error(r, &dir, &path);
        }

        entry->name.len = len;

        entry->name.data = ngx_palloc(pool, len + 1);
        if (entry->name.data == NULL) {
            return ngx_http_fancyindex_error(r, &dir, &path);
        }

        ngx_cpystrn(entry->name.data, ngx_de_name(&dir), len + 1);

        entry->escape = 2 * ngx_escape_uri(NULL, ngx_de_name(&dir), len,
                                           NGX_ESCAPE_HTML);

        if (r->utf8) {
            entry->utf_len = ngx_utf_length(entry->name.data, entry->name.len);
        } else {
            entry->utf_len = len;
        }

        entry->dir = ngx_de_is_dir(&dir);
        entry->mtime = ngx_de_mtime(&dir);
        entry->size = ngx_de_size(&dir);
    }

    if (ngx_close_dir(&dir) == NGX_ERROR) {
        ngx_log_error(NGX_LOG_ALERT, r->connection->log, ngx_errno,
                      ngx_close_dir_n " \"%s\" failed", &path);
    }

    len = NFI_TEMPLATE_SIZE
          + r->uri.len /* URI is included two times: as <title> in the */
          + r->uri.len /* HTML head and as an <h1> in the HTML body    */
          ;

    entry = entries.elts;
    for (i = 0; i < entries.nelts; i++) {
        len += sizeof("<a href=\"") - 1
            + entry[i].name.len + entry[i].escape
            + 1                                          /* 1 is for "/" */
            + sizeof("\">") - 1
            + entry[i].name.len - entry[i].utf_len
            + NGX_HTTP_FANCYINDEX_NAME_LEN + sizeof("&gt;") - 2
            + sizeof("</a>") - 1
            + sizeof(" 28-Sep-1970 12:00 ") - 1
            + 20                                         /* the file size */
            + 2;
    }

    b = ngx_create_temp_buf(r->pool, len);
    if (b == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    if (entries.nelts > 1) {
        ngx_qsort(entry, (size_t) entries.nelts,
                  sizeof(ngx_http_fancyindex_entry_t),
                  ngx_http_fancyindex_cmp_entries);
    }

    b->last = nfi_cpymem_ssz(b->last, t01_head1);
    b->last = nfi_cpymem_str(b->last, r->uri);
    b->last = nfi_cpymem_ssz(b->last, t02_head2);

    b->last = nfi_cpymem_ssz(b->last, t03_body1);
    b->last = nfi_cpymem_str(b->last, r->uri);
    b->last = nfi_cpymem_ssz(b->last, t04_body2);

    /* Output table header */
    b->last = nfi_cpymem_ssz(b->last, t05_list1);

    tp = ngx_timeofday();

    for (i = 0; i < entries.nelts; i++) {
        b->last = nfi_cpymem_ssz(b->last, "<a href=\"");

        if (entry[i].escape) {
            ngx_escape_uri(b->last, entry[i].name.data, entry[i].name.len,
                           NGX_ESCAPE_HTML);

            b->last += entry[i].name.len + entry[i].escape;

        } else {
            b->last = ngx_cpymem(b->last, entry[i].name.data,
                                 entry[i].name.len);
        }

        if (entry[i].dir) {
            *b->last++ = '/';
        }

        *b->last++ = '"';
        *b->last++ = '>';

        len = entry[i].utf_len;

        if (entry[i].name.len - len) {
            if (len > NGX_HTTP_FANCYINDEX_NAME_LEN) {
                copy = NGX_HTTP_FANCYINDEX_NAME_LEN - 3 + 1;

            } else {
                copy = NGX_HTTP_FANCYINDEX_NAME_LEN + 1;
            }

            b->last = ngx_utf_cpystrn(b->last, entry[i].name.data, copy);
            last = b->last;

        } else {
            b->last = ngx_cpystrn(b->last, entry[i].name.data,
                                  NGX_HTTP_FANCYINDEX_NAME_LEN + 1);
            last = b->last - 3;
        }

        if (len > NGX_HTTP_FANCYINDEX_NAME_LEN) {
            b->last = ngx_cpymem(last, "..&gt;</a>", sizeof("..&gt;</a>") - 1);

        } else {
            if (entry[i].dir && NGX_HTTP_FANCYINDEX_NAME_LEN - len > 0) {
                *b->last++ = '/';
                len++;
            }

            b->last = ngx_cpymem(b->last, "</a>", sizeof("</a>") - 1);
            ngx_memset(b->last, ' ', NGX_HTTP_FANCYINDEX_NAME_LEN - len);
            b->last += NGX_HTTP_FANCYINDEX_NAME_LEN - len;
        }

        *b->last++ = ' ';

        ngx_gmtime(entry[i].mtime + tp->gmtoff * 60 * alcf->localtime, &tm);

        b->last = ngx_sprintf(b->last, "%02d-%s-%d %02d:%02d ",
                              tm.ngx_tm_mday,
                              months[tm.ngx_tm_mon - 1],
                              tm.ngx_tm_year,
                              tm.ngx_tm_hour,
                              tm.ngx_tm_min);

        if (alcf->exact_size) {
            if (entry[i].dir) {
                b->last = nfi_cpymem_ssz(b->last, "                  -");
            } else {
                b->last = ngx_sprintf(b->last, "%19O", entry[i].size);
            }

        } else {
            if (entry[i].dir) {
                b->last = nfi_cpymem_ssz(b->last,  "      -");

            } else {
                length = entry[i].size;

                if (length > 1024 * 1024 * 1024 - 1) {
                    size = (ngx_int_t) (length / (1024 * 1024 * 1024));
                    if ((length % (1024 * 1024 * 1024))
                                                > (1024 * 1024 * 1024 / 2 - 1))
                    {
                        size++;
                    }
                    scale = 'G';

                } else if (length > 1024 * 1024 - 1) {
                    size = (ngx_int_t) (length / (1024 * 1024));
                    if ((length % (1024 * 1024)) > (1024 * 1024 / 2 - 1)) {
                        size++;
                    }
                    scale = 'M';

                } else if (length > 9999) {
                    size = (ngx_int_t) (length / 1024);
                    if (length % 1024 > 511) {
                        size++;
                    }
                    scale = 'K';

                } else {
                    size = (ngx_int_t) length;
                    scale = '\0';
                }

                if (scale) {
                    b->last = ngx_sprintf(b->last, "%6i%c", size, scale);

                } else {
                    b->last = ngx_sprintf(b->last, " %6i", size);
                }
            }
        }

        *b->last++ = CR;
        *b->last++ = LF;
    }

    /* TODO: free temporary pool */

    /* Output table bottom */
    b->last = nfi_cpymem_ssz(b->last, t06_list2);

    /*
     * Output body end, including readme if requested. The t07_body3 and
     * t08_body4 templates may be empty, so use ngx_http_fancyindex_tcpy_if
     * TODO: Output readme
     */
    b->last = nfi_cpymem_ssz(b->last, t07_body3);
    b->last = nfi_cpymem_ssz(b->last, t08_body4);

    /* Output page footer */
    b->last = nfi_cpymem_ssz(b->last, t09_foot1);

    if (r == r->main) {
        b->last_buf = 1;
    }

    b->last_in_chain = 1;

    out.buf = b;
    out.next = NULL;

    return ngx_http_output_filter(r, &out);
}


static int ngx_libc_cdecl
ngx_http_fancyindex_cmp_entries(const void *one, const void *two)
{
    ngx_http_fancyindex_entry_t *first = (ngx_http_fancyindex_entry_t *) one;
    ngx_http_fancyindex_entry_t *second = (ngx_http_fancyindex_entry_t *) two;

    if (first->dir && !second->dir) {
        /* move the directories to the start */
        return -1;
    }

    if (!first->dir && second->dir) {
        /* move the directories to the start */
        return 1;
    }

    return (int) ngx_strcmp(first->name.data, second->name.data);
}


#if 0

static ngx_buf_t *
ngx_http_fancyindex_alloc(ngx_http_fancyindex_ctx_t *ctx, size_t size)
{
    ngx_chain_t  *cl;

    if (ctx->buf) {

        if ((size_t) (ctx->buf->end - ctx->buf->last) >= size) {
            return ctx->buf;
        }

        ctx->size += ctx->buf->last - ctx->buf->pos;
    }

    ctx->buf = ngx_create_temp_buf(ctx->pool, ctx->alloc_size);
    if (ctx->buf == NULL) {
        return NULL;
    }

    cl = ngx_alloc_chain_link(ctx->pool);
    if (cl == NULL) {
        return NULL;
    }

    cl->buf = ctx->buf;
    cl->next = NULL;

    *ctx->last_out = cl;
    ctx->last_out = &cl->next;

    return ctx->buf;
}

#endif


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
     *    conf->header.len  = 0
     *    conf->header.data = NULL
     *    conf->footer.len  = 0
     *    conf->footer.data = NULL
     *    conf->readme.len  = 0
     *    conf->readme.data = NULL
     */
    conf->enable = NGX_CONF_UNSET;
    conf->localtime = NGX_CONF_UNSET;
    conf->exact_size = NGX_CONF_UNSET;
    conf->readme_flags = NGX_CONF_UNSET;

    return conf;
}


static char *
ngx_http_fancyindex_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_fancyindex_loc_conf_t *prev = parent;
    ngx_http_fancyindex_loc_conf_t *conf = child;

    ngx_conf_merge_value(conf->enable, prev->enable, 0);
    ngx_conf_merge_value(conf->localtime, prev->localtime, 0);
    ngx_conf_merge_value(conf->exact_size, prev->exact_size, 1);

    ngx_conf_merge_str_value(conf->header, prev->header, "");
    ngx_conf_merge_str_value(conf->footer, prev->footer, "");
    ngx_conf_merge_str_value(conf->readme, prev->readme, "");

    ngx_conf_merge_bitmask_value(conf->readme_flags, prev->readme_flags,
            (NGX_HTTP_FANCYINDEX_README_TOP | NGX_HTTP_FANCYINDEX_README_PRE));

    return NGX_CONF_OK;
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
