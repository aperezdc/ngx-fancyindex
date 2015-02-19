# Change Log
All notable changes to this project will be documented in this file.

## Unreleased
### Added
- New feature: Allow setting the default sort criterion using the
  `fancyindex_default_sort` configuration directive. (Patch by
  Алексей Урбанский).
- New feature: Allos changing the maximum length of file names, using
  the `fancyindex_name_length` configuration directive. (Patch by
  Martin Herkt).
### Changed
- Renames `NEWS.rst` to `CHANGELOG.md`, which follows the recommendations
	from [Keep a Change Log](http://keepachangelog.com/).

## 0.3.4 - 2014-09-03
### Added
- Viewport is now defined in the generated HTML, which works better
  for mobile devices.
### Changed
- Even-odd row styling moved to the CSS using :nth-child(). This
  makes the HTML served to clients smaller.

## 0.3.3 - 2013-10-25
### Added
- New feature: table headers in the default template are now clickable
  to set the sorting criteria and direction of the index entries.
  (https://github.com/aperezdc/ngx-fancyindex/issues/7)

## 0.3.2 - 2013-06-05
### Fixed
- Solved a bug that would leave certain clients stalled forever.
- Improved handling of subrequests for non-builtin headers/footers.

## 0.3.1 - 2011-04-04
### Added
- `NEWS.rst` file, to act as change log.
