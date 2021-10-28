# Change Log
All notable changes to this project will be documented in this file.

## [Unreleased]

## [0.5.2] - 2021-10-28
### Fixed
- Properly escape file names to ensure that file names are never renreded
  as HTML. (Patch by Anthony Ryan <<anthonyryan1@gmail.com>>,
  [#128](https://github.com/aperezdc/ngx-fancyindex/pull/128).)

## [0.5.1] - 2020-10-26
### Fixed
- Properly handle optional second argument to `fancyindex_header` and
  `fancyindex_footer`
  ([#117](https://github.com/aperezdc/ngx-fancyindex/issues/117)).

## [0.5.0] - 2020-10-24
### Added
- New option `fancyindex_show_dotfiles`. (Path by Joshua Shaffer
  <<joshua.shaffer@icmrl.net>>.)
- The `fancyindex_header` and `fancyindex_footer` options now support local
  files properly, by means of a `local` flag. (Patches by JoungKyun Kim
  <<joungkyun@gmail.com>> and Adrián Pérez <<aperez@igalia.com>>.)

### Changed
- Improved performance of directory entry sorting, which should be quite
  noticeable for directories with thousands of files. (Patch by
  [Yuxiang Zhang](https://github.com/z4yx).)
- The minimum Nginx version supported by the module is now 0.8.x.

### Fixed
- Properly escape square brackets in directory entry names when the module
  is built with older versions of Nginx. (Patch by Adrián Pérez
  <<aperez@igalia.com>>.)
- Fix directory entry listing not being shown when using the
  [nginx-auth-ldap](https://github.com/kvspb/nginx-auth-ldap) module. (Patch
  by JoungKyun Kim <<joungkyun@gmail.com>>.)

## [0.4.4] - 2020-02-19
### Added
- New option `fancyindex_hide_parent_dir`, which disables generating
  links to parent directories in listings. (Patch by Kawai Ryota
  <<admin@mail.kr-kp.com>>.)

### Changed
- Each table row is now separated by a new line (as a matter of fact,
  a `CRLF` sequence), which makes it easier to parse output using simple
  text tools. (Patch by Anders Trier <<anders.trier.olesen@gmail.com>>.)
- Some corrections and additions to the README file. (Patches by Nicolas
  Carpi <<nicolas.carpi@curie.fr>> and David Beitey <<david@davidjb.com>>.)

### Fixed
- Use correct character references for `&` characters in table sorter URLs
  within the template (Patch by David Beitey <<david@davidjb.com>>.)
- Properly encode filenames when used as URI components.

## [0.4.3] - 2018-07-03
### Added
- Table cells now have class names, which allows for better CSS styling.
  (Patch by qjqqyy <<gyula@nyirfalvi.hu>>.)
- The test suite now can parse and check elements from the HTML returned
  by the module, thanks to the [pup](https://github.com/EricChiang/pup)
  tool.

### Fixed
- Sorting by file size now works correctly.
  (Patch by qjqqyy <<gyula@nyirfalvi.hu>>.)

## [0.4.2] - 2017-08-19
### Changed
- Generated HTML from the default template is now proper HTML5, and it should
  pass validation (#52).
- File sizes now have decimal positions when using `fancyindex_exact_size off`.
  (Patch by Anders Trier <<anders.trier.olesen@gmail.com>>.)
- Multiple updates to `README.rst` (Patches by Danila Vershinin
  <<ciapnz@gmail.com>>, Iulian Onofrei, Lilian Besson, and Nick Geoghegan
  <<nick@nickgeoghegan.net>>.)

### Fixed
- Sorting by file size now also works correctly for directories which contain
  files of sizes bigger than `INT_MAX`. (#74, fix suggestion by Chris Young.)
- Custom headers which fail to declare an UTF-8 encoding no longer cause table
  header arrows to be rendered incorrectly by browsers (#50).
- Fix segmentation fault when opening directories with empty files (#61, patch
  by Catgirl <<cat@wolfgirl.org>>.)

## [0.4.1] - 2016-08-18
### Added
- New `fancyindex_directories_first` configuration directive (enabled by
  default), which allows setting whether directories are sorted before other
  files. (Patch by Luke Zapart <<luke@zapart.org>>.)

### Fixed
- Fix index files not working when the fancyindex module is in use (#46).


## [0.4.0] - 2016-06-08
### Added
- The module can now be built as a [dynamic
  module](https://www.nginx.com/resources/wiki/extending/converting/).
  (Patch by Róbert Nagy <<vrnagy@gmail.com>>.)
- New configuration directive `fancyindex_show_path`, which allows hiding the
  `<h1>` header which contains the current path.
  (Patch by Thomas P.  <<tpxp@live.fr>>.)

### Changed
- Directory and file links in listings now have a title="..." attribute.
  (Patch by `@janglapuk` <<trusdi.agus@gmail.com>>.)

### Fixed
- Fix for hung requests when the module is used along with `ngx_pagespeed`.
  (Patch by Otto van der Schaaf <<oschaaf@we-amp.com>>.)


## [0.3.6] - 2016-01-26
### Added
- New feature: Allow filtering out symbolic links using the
  `fancyindex_hide_symlinks` configuration directive. (Idea and prototype
  patch by Thomas Wemm.)
- New feature: Allow specifying the format of timestamps using the
  `fancyindex_time_format` configuration directive. (Idea suggested by Xiao
  Meng <<novoreorx@gmail.com>>).

### Changed
- Listings in top-level directories will not generate a "Parent Directory"
  link as first element of the listing. (Patch by Thomas P.)

### Fixed
- Fix propagation and overriding of the `fancyindex_css_href` setting inside
  nested locations.
- Minor changes in the code to allow building cleanly under Windows with
  Visual Studio 2013. (Patch by Y. Yuan <<yzwduck@gmail.com>>).


## [0.3.5] - 2015-02-19
### Added
- New feature: Allow setting the default sort criterion using the
  `fancyindex_default_sort` configuration directive. (Patch by
  Алексей Урбанский).
- New feature: Allow changing the maximum length of file names, using
  the `fancyindex_name_length` configuration directive. (Patch by
  Martin Herkt).

### Changed
- Renames `NEWS.rst` to `CHANGELOG.md`, which follows the recommendations
	from [Keep a Change Log](http://keepachangelog.com/).
- Configuring Nginx without the `http_addition_module` will generate a
  warning during configuration, as it is needed for the `fancyindex_footer`
  and `fancyindex_header` directives.


## [0.3.4] - 2014-09-03

### Added
- Viewport is now defined in the generated HTML, which works better
  for mobile devices.

### Changed
- Even-odd row styling moved to the CSS using :nth-child(). This
  makes the HTML served to clients smaller.


## [0.3.3] - 2013-10-25

### Added
- New feature: table headers in the default template are now clickable
  to set the sorting criteria and direction of the index entries.
  (https://github.com/aperezdc/ngx-fancyindex/issues/7)


## [0.3.2] - 2013-06-05

### Fixed
- Solved a bug that would leave certain clients stalled forever.
- Improved handling of subrequests for non-builtin headers/footers.


## [0.3.1] - 2011-04-04

### Added
- `NEWS.rst` file, to act as change log.


[Unreleased]: https://github.com/aperezdc/ngx-fancyindex/compare/v0.5.2...HEAD
[0.5.2]: https://github.com/aperezdc/ngx-fancyindex/compare/v0.5.1...v0.5.2
[0.5.1]: https://github.com/aperezdc/ngx-fancyindex/compare/v0.5.0...v0.5.1
[0.5.0]: https://github.com/aperezdc/ngx-fancyindex/compare/v0.4.4...v0.5.0
[0.4.4]: https://github.com/aperezdc/ngx-fancyindex/compare/v0.4.3...v0.4.4
[0.4.3]: https://github.com/aperezdc/ngx-fancyindex/compare/v0.4.2...v0.4.3
[0.4.2]: https://github.com/aperezdc/ngx-fancyindex/compare/v0.4.1...v0.4.2
[0.4.1]: https://github.com/aperezdc/ngx-fancyindex/compare/v0.4.0...v0.4.1
[0.4.0]: https://github.com/aperezdc/ngx-fancyindex/compare/v0.3.6...v0.4.0
[0.3.6]: https://github.com/aperezdc/ngx-fancyindex/compare/v0.3.5...v0.3.6
[0.3.5]: https://github.com/aperezdc/ngx-fancyindex/compare/v0.3.4...v0.3.5
[0.3.4]: https://github.com/aperezdc/ngx-fancyindex/compare/v0.3.3...v0.3.4
[0.3.3]: https://github.com/aperezdc/ngx-fancyindex/compare/v0.3.2...v0.3.3
[0.3.2]: https://github.com/aperezdc/ngx-fancyindex/compare/v0.3.1...v0.3.2
[0.3.1]: https://github.com/aperezdc/ngx-fancyindex/compare/v0.3...v0.3.1
