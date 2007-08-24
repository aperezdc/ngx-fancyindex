#! /usr/bin/awk -f
#
# Copyright © Adrian Perez <adrianperez@udc.es>
#
# Converts an HTML template into a C header suitable for inclusion.
# Take a look at the HACKING.rst file to know how to use it :-)
#
# This code is placed in the public domain.

BEGIN {
	varname = 0;
	print "/* Automagically generated, do not edit! */"
}

/^<!--[[:space:]]*var[[:space:]]+[^[:space:]]+[[:space:]]*-->$/ {
	if (varname) print ";";
	if ($3 == "NONE") {
		varname = 0;
		next;
	}
	varname = $3;
	print "static const u_char " varname "[] = \"\"";
	next;
}

{
	if (!varname) next;
	# Order matters
	gsub(/[\t\v\n\r\f\g]+/, "");
	gsub(/\\/, "\\\\");
	gsub(/"/, "\\\"");
	print "\"" $0 "\""
}


END {
	if (varname) print ";";
}

