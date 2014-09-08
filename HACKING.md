# Fancy Index module Hacking HOW-TO

## How to modify the template

The template is in the `template.html` file. Note that comment markers are
used to control how the `template.awk` Awk script generates the C header
which gets ultimately included in the compiled object code. Comment markers
have the `<!-- var identifier -->` format. Here `identifier` must be
a valid C identifier. All the text following the marker until the next
marker will be flattened into a C string.

If the identifier is `NONE` (capitalized) the text from that marker up to
the next marker will be discarded.


## Regenerating the C header

You will need Awk. I hope any decent implementation will do, but the GNU one
is known to work flawlessly. Just do:

    $ awk -f template.awk template.html > template.h

If your copy of `awk` is not the GNU implementation, you will need to
install it and use `gawk` instead in the command line above.
