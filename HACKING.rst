===================================
 Fancy Index module Hacking HOW-TO
===================================

.. contents::


How to modify the template
==========================

The template is in the ``template.html`` file. Note that comment markers are
used to control how the ``split-template`` Awk script generates the C header
which gets ultimately included in the compiled object code. Comment markers
have the ``<!-- var identifier -->`` format. Here ``identifier`` must be
a valid C identifier. All the text following the marker until the next
marker will be flattened into a C string.

Regenerating the C header
~~~~~~~~~~~~~~~~~~~~~~~~~
You will need Awk. I hope any decent implementation will do, but the GNU one
is known to work flawlessly. Just do::

  $ awk -f template.awk template.html > template.h


Template identifier order
~~~~~~~~~~~~~~~~~~~~~~~~~
t01_head1
  Outputs the HTML header and must end with something like
  ``<title>Index of``, because the code inserts the path of the URI just
  after this piece.
t02_head2
  Outputs the rest of the header, usually will close the ``</title>`` tag
  opened in the previous template and add further output until the closing
  ``</head>`` tag.
t03_body1
  -
t04_body2
  -
t05_list1
  -
t06_list2
  -
t07_body3
  -
t08_body4
  -
t09_foot1
  -


.. vim: spell spelllang=en expandtab

