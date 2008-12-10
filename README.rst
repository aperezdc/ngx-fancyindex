========================
Nginx Fancy Index module
========================

.. contents::

The Fancy Index module makes possible the generation of file listings, like
the built-in `autoindex <http://wiki.codemongers.com/NginxHttpAutoindexModule>`__
module does, but adding a touch of style. This is possible because the module
module allows a certain degree of customization of the generated content:

* Custom headers. Either local or stored remotely (*working*).
* Custom footers. Either local or stored remotely (*working*).
* Add you own CSS style rules (*not yet implemented*).

This module is designed to work with Nginx_, a high performance open source web
server written by `Igor Sysoev <http://sysoev.ru>`__.


Requirements
============
* Sources for Nginx_ 0.6.x, and its requirements.
* Patience, and some Coffee™.


Building
========

1. Unpack the Nginx_ sources::

    $ gunzip -c nginx-0.6.6.tar.gz | tar -xvf -

2. Unpack the sources for the fancy indexing module::

    $ gunzip -c nginx-fancyindex-?.?.?.tar.gz | tar -xvf -

3. Change to the directory which contains the Nginx_ sources, run the
   configuration script with the desired options and be sure to put an
   ``--add-module`` flag pointing to the directory which contains the source
   of the fancy indexing module::

    $ cd nginx-0.6.6
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

fancyindex_localtime
~~~~~~~~~~~~~~~~~~~~
:Syntax: *fancyindex_localtime* [*on* | *off*]
:Default: fancyindex_localtime off
:Context: http, server, location
:Description:
  Defines how to represent file sizes in the directory listing; either
  accurately, or rounding off to the kilobyte, the megabyte and the
  gigabyte.

fancyindex_exact_size
~~~~~~~~~~~~~~~~~~~~~
:Syntax: *fancyindex_exact_size* [*on* | *off*]
:Default: fancyindex_exact_size on
:Context: http, server, location
:Description:
  Enables showing file times as local time. Default is “off” (GMT time).

fancyindex_header
~~~~~~~~~~~~~~~~~
:Syntax: *fancyindex_header path*
:Default: fancyindex_header ""
:Context: http, server, location
:Description:
  Specifies which file should be inserted at the head of directory listings.
  If set to an empty string, the default header supplied by the module will
  be sent.

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
   `let me know <mailto:adrianperez@udc.es>`__.


fancyindex_readme
~~~~~~~~~~~~~~~~~
:Syntax: *fancyindex_readme path*
:Default: fancyindex_readme ""
:Context: http, server, location
:Description:
  Specifies which file should be inserted alongside with directory listings.
  If set to an empty string, no extra textual content is included. See
  `fancyindex_readme_mode`_ in order to learn more about how the readme
  file may be included.

fancyindex_readme_mode
~~~~~~~~~~~~~~~~~~~~~~
:Syntax:
  *fancyindex_readme_mode* *pre* | *asis* | *top* | *bottom* | *div* ...
:Default: fancyindex_readme_mode top
:Context: http, server, location
:Description:
  Controls how to include the readme file specified by `fancyindex_readme`_.
  Available options are:

    pre
      Send included readme file inside a preformatted text block (i.e. an
      HTML ``<pre>`` element.
    asis
      Send included readme file “as-is”, i.e. without altering its contents.
      This is useful to include raw HTML snippets in the generated listings.
    top
      Place readme file contents at the top, before the listings.
    bottom
      Place readme file contents at the bottom, after the listings.
    div
      Wrap up all the text generated for the readme (including the enclosing
      ``<pre>`` element, if configured) inside a ``<div>`` element. The
      layer will have the ``readme`` CSS class set.
    iframe
      Insert an ``<iframe>`` element which references the readme file
      instead if inlining it in the generated file index. Note that this
      will instruct the client to perform an additional request in order to
      fetch the contents of the frame.


.. _nginx: http://nginx.net

.. vim:ft=rst:spell:spelllang=en:

