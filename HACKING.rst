===================================
 Fancy Index module Hacking HOW-TO
===================================

.. contents::


How to modify templates
=======================

Templates are in the ``templates/`` subdirectory of the source distribution.
They are included both as regular HTML text snippets (files ending in ``.t``)
and ready for inclusion in a C program (files ending in ``.inc``). Also
there is a small C program


Template order
~~~~~~~~~~~~~~
01-head1
  Outputs the HTML header and must end with something like
  ``<title>Index of``, because the code inserts the path of the URI just
  after this piece.
02-head2
  Outputs the rest of the header, usually will close the ``</title>`` tag
  opened in the previous template and add further output until the closing
  ``</head>`` tag.
03-body1
  -
04-body2
  -
05-list1
  -
06-list2
  -
07-body3
  -
08-body4
  -
09-foot1
  -


.. vim: spell spelllang=en expandtab

