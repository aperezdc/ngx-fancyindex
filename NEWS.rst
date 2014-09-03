==============================
 Major changes among versions
==============================

v0.3.4
======
- Viewport is now defined in the generated HTML, which works better
  for mobile devices.
- Even-odd row styling moved to the CSS using :nth-child(). This
  makes the HTML served to clients smaller.

v0.3.3
======
- New feature: table headers in the default template are now clickable
  to set the sorting criteria and direction of the index entries.
  (https://github.com/aperezdc/ngx-fancyindex/issues/7)

v0.3.2
======
- Solved a bug that would leave certain clients stalled forever.
- Improved handling of subrequests for non-builtin headers/footers.

v0.3.1
======
(First entry in the NEWS file.)
