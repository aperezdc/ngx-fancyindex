========================
Nginx Fancy Index module
========================

.. contents::

The Fancy Index module makes possible the generation of file listings, like
the built-in `autoindex <http://wiki.nginx.org/NginxHttpAutoindexModule>`__
module does, but adding a touch of style. This is possible because the module
module allows a certain degree of customization of the generated content:

* Custom headers. Either local or stored remotely.
* Custom footers. Either local or stored remotely.
* Add you own CSS style rules.
* Allow choosing to sort elements by name (default), modification time, or
  size; both ascending (default), or descending.

This module is designed to work with Nginx_, a high performance open source web
server written by `Igor Sysoev <http://sysoev.ru>`__.


Requirements
============

You will need the sources for Nginx_. Any version starting from the 0.7
series onwards will work.  Note that the modules *might* compile with
versions in the 0.6 series by applying ``nginx-0.6-support.patch``, but this
is unsupported (YMMV).


Building
========

1. Unpack the Nginx_ sources::

    $ gunzip -c nginx-?.?.?.tar.gz | tar -xvf -

2. Unpack the sources for the fancy indexing module::

    $ gunzip -c nginx-fancyindex-?.?.?.tar.gz | tar -xvf -

3. Change to the directory which contains the Nginx_ sources, run the
   configuration script with the desired options and be sure to put an
   ``--add-module`` flag pointing to the directory which contains the source
   of the fancy indexing module::

    $ cd nginx-?.?.?
    $ ./configure --add-module=../nginx-fancyindex-?.?.?  [extra desired options]

4. Build and install the software::

    $ make

   And then, as ``root``::

    # make install

5. Configure Nginx_ by using the modules' configuration directives_.


Example
=======

You can test the default built-in style by adding the following lines into
a ``server`` section in your Nginx_ configuration file::

  location / {
    fancyindex on;              # Enable fancy indexes.
    fancyindex_exact_size off;  # Output human-readable file sizes.
  }


Directives
==========

fancyindex
~~~~~~~~~~
:Syntax: *fancyindex* [*on* | *off*]
:Default: fancyindex off
:Context: http, server, location
:Description:
  Enables or disables fancy directory indexes.

fancyindex_css_href
~~~~~~~~~~~~~~~~~~~
:Syntax: *fancyindex_css_href uri*
:Default: fancyindex_css_href ""
:Context: http, server, location
:Description:
  Allows inserting a link to a CSS style sheet in generated listings. The
  provided *uri* parameter will be inserted as-is in a ``<link>`` HTML tag.
  The link is inserted after the built-in CSS rules, so you can override the
  default styles.

fancyindex_exact_size
~~~~~~~~~~~~~~~~~~~~~
:Syntax: *fancyindex_exact_size* [*on* | *off*]
:Default: fancyindex_exact_size on
:Context: http, server, location
:Description:
  Defines how to represent file sizes in the directory listing; either
  accurately, or rounding off to the kilobyte, the megabyte and the
  gigabyte.

fancyindex_footer
~~~~~~~~~~~~~~~~~
:Syntax: *fancyindex_footer path*
:Default: fancyindex_footer ""
:Context: http, server, location
:Description:
  Specifies which file should be inserted at the foot of directory listings.
  If set to an empty string, the default footer supplied by the module will
  be sent.

.. warning:: When inserting custom header/footer a subrequest will be
   issued so potentially any URL can be used as source for them. Although it
   will work with external URLs, only using internal ones is supported.
   External URLs are totally untested and using them will make Nginx_ block
   while waiting for the subrequest to complete. If you feel like external
   header/footer is a must-have for you, please
   `let me know <mailto:aperez@igalia.com>`__.

fancyindex_header
~~~~~~~~~~~~~~~~~
:Syntax: *fancyindex_header path*
:Default: fancyindex_header ""
:Context: http, server, location
:Description:
  Specifies which file should be inserted at the head of directory listings.
  If set to an empty string, the default header supplied by the module will
  be sent.

fancyindex_ignore
~~~~~~~~~~~~~~~~~
:Syntax: *fancyindex_ignore string1 [string2 [... stringN]]*
:Default: No default.
:Context: http, server, location
:Description:
  Specifies a list of file names which will be not be shown in generated
  listings. If Nginx was built with PCRE support strings are interpreted as
  regular expressions.

fancyindex_localtime
~~~~~~~~~~~~~~~~~~~~
:Syntax: *fancyindex_localtime* [*on* | *off*]
:Default: fancyindex_localtime off
:Context: http, server, location
:Description:
  Enables showing file times as local time. Default is “off” (GMT time).

.. _nginx: http://nginx.net

.. vim:ft=rst:spell:spelllang=en:
