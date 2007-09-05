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
	void *dummy;
	size_t  root;
	u_char *last;
	ngx_str_t resolved;
	ngx_buf_t *buf;
	ngx_file_t *bfile;
	ngx_file_info_t bfileinfo;

	resolved.len  = path->len;
	resolved.data = ngx_palloc(r->pool, resolved.len + 1);
	if (resolved.data == NULL)
		return NULL;

	last = ngx_http_map_uri_to_path(r, &resolved, &root, 0);
	if (last == NULL)
		return NULL;

	/*
	 * Append final '\0' so we can use it to call ngx_file_info() below.
	 *
	 * If stat() fails, we know the file does *not* exist. We also check
	 * whether the file is readable. Note that if the file exists and is
	 * readable, the file info will be in the kernel inode cache so we
	 * do not incur in a lot of overhead by retrieving the information
	 * first.
	 */
	resolved.data[resolved.len] = '\0';
	if (ngx_file_info(resolved.data, &bfileinfo) != 0)
		return NULL;

	if (!ngx_is_file(&bfileinfo) ||           /* we read regular files... */
			!(ngx_file_access(&bfileinfo) & 0444) /* ...which can be read...  */
			|| ngx_is_link(&bfileinfo))           /* ...and are not symlinks. */
		return NULL;

	bfile = ngx_pcalloc(r->pool, sizeof(ngx_file_t));
	if (bfile == NULL)
		return NULL;

	/* Fill in the file structure with sensible values. */
	bfile->fd = -1;
	bfile->name.len = resolved.len;
	bfile->name.data = resolved.data;
	dummy = ngx_cpymem(&bfile->info, &bfileinfo, sizeof(ngx_file_info_t));
	bfile->valid_info = 1;

	buf = ngx_pcalloc(r->pool, sizeof(ngx_buf_t));
	if (buf == NULL)
		return NULL;

	/*
	 * ngx_pcalloc() makes zeros, which are good defaults for most values, we
	 * tell nginx that the file is on-disk, not in memory, and we associate
	 * the corresponding file information (ngx_file_t) structure.
	 */
	buf->file = bfile;
	buf->in_file = 1;

	return buf;
}

