#
# Makefile
# acastro, 2007-08-21 18:41
#

all_t   := $(wildcard *.t)
all_inc := $(patsubst %.t,%.inc,$(all_t))


all: $(all_inc)

t2inc: t2inc.c

.INTERMEDIATE: t2inc

$(all_inc): t2inc

%.inc: %.t
	./t2inc < $< > $@


# vim:ft=make
#

