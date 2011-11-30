## Makefile for the test directory of normalize
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

TESTS = test-tools test8bit.sh test16bit.sh test24bit.sh

EXTRA_DIST = $(TESTS)

CLEANFILES = test-tools mono.wav stereo.wav test.log

test-tools: ../src/mktestwav
	-rm -f test.log
	echo "#!/bin/sh" > test-tools
	echo "> test.log" >> test-tools
	echo "exit 77" >> test-tools
	chmod 755 test-tools

../src/mktestwav:
	(cd ../src && $(MAKE) mktestwav)
