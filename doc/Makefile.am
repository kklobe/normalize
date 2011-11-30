## Makefile for the doc directory of normalize
## Copyright (C) 1999--2005 Chris Vaill
##
## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2 of the License, or
## (at your option) any later version.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program; if not, write to the Free Software
## Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

## Process this file with automake to produce Makefile.in

SUBDIRS = fr
man_MANS = normalize.1 normalize-mp3.1
EXTRA_DIST = $(man_MANS) README.sgml normalize.1.sgml frontend.txt
JADE=openjade
LYNX=lynx

default:

normalize.1: normalize.1.sgml
	-docbook2man $<
	-[ -f normalize.en.1 ] && mv normalize.en.1 normalize.1

../README: README.html
	-$(LYNX) -force_html -dump -nolist -width=80 -display_charset=us-ascii $< > $@

##	-docbook2txt $<
##	-mv README.txt README

README.html: README.sgml
	-$(JADE) -V nochunks -t sgml -i html -d $(top_srcdir)/doc/custom.dsl $< > README.html

##	-docbook2html --nochunks $<
##	-sed 's/index.html/README.html/g' index.html > README.html

README.rtf: README.sgml
	-docbook2rtf $<
