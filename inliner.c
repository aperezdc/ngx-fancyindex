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
	ngx_str_t resolved;
	ngx_buf_t *buf;
	ngx_file_t *bfile;
	ngx_file_info_t bfileinfo;

	ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
			"http nfi_inline_getbuf path: \"%V\"", path);

	resolved.len  = r->uri.len + path->len;
	resolved.data = ngx_palloc(r->pool, resolved.len + 1);
	if (resolved.data == NULL)
		return NULL;

	ngx_memcpy(
			ngx_cpymem(resolved.data, r->uri.data, r->uri.len),
			path->data, path->len);

	ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
			"http nfi_inline_getbuf resolved: \"%V\"", &resolved);

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

	if (ngx_file_info(resolved.data, &bfileinfo) != 0) {
		ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, ngx_errno,
				"http nfi_inline_getbuf no file stats");
		return NULL;
	}

	if (!ngx_is_file(&bfileinfo) ||           /* we read regular files... */
			!(ngx_file_access(&bfileinfo) & 0444) /* ...which can be read...  */
			|| ngx_is_link(&bfileinfo))           /* ...and are not symlinks. */
		return NULL;

	bfile = ngx_pcalloc(r->pool, sizeof(ngx_file_t));
	if (bfile == NULL)
		return NULL;

	/* Fill in the file structure with sensible values. */
	bfile->fd = NGX_INVALID_FILE;
	bfile->name.len = resolved.len;
	bfile->name.data = resolved.data;
	ngx_memcpy(&bfile->info, &bfileinfo, sizeof(ngx_file_info_t));
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

	/*
	 * Data is in the file, so we set the flag. Nginx will decide whether to
	 * read and send the contents or issue a call to snedfile() or whatever.
	 */
	buf->in_file = 1;

	/*
	 * We want to send all the contents of the file, so set the offset of the
	 * last sent byte to the length of the file minus one (remeber: use offset)
	 */
	buf->file_last = ngx_file_size(&bfileinfo) - 1;

	ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
			"nfi_inline_getbuf returned buf %p", buf);

	return buf;
}

