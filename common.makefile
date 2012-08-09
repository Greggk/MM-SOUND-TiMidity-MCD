# TiMidity++ -- MIDI to WAVE converter and player
# Copyright (C) 1999-2001 Masanao Izumo <mo@goice.co.jp>
# Copyright (C) 1995 Tuukka Toivonen <tt@cgs.fi>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

# Define follows if you want to change.
# Note that the definition of beginning with just one `#' implies
# default value from configure.

SHELL=/bin/sh


## Package compilations
## Please modify and uncomment if you want to change.
#CC= gcc
#CFLAGS =  -g -Zmt -Zdll
# For pentium gcc
##CFLAGS = -O3 -mpentium -march=pentium -fomit-frame-pointer \
##         -funroll-all-loops -malign-double -ffast-math
# For PGCC
##CFLAGS = -O6 -mpentium -fomit-frame-pointer -funroll-all-loops -ffast-math \
##         -fno-peep-spills \
##         -fno-exceptions -malign-jumps=0 -malign-loops=0 -malign-functions=0
#CPPFLAGS =  -Ic -I/emx/include;c -I/xfree86/include -I/usr/local/include -DAU_DART -DMCD -D__ST_MT_ERRNO__ -DLAGRANGE_INTERPOLATION $(DEF_PKGDATADIR) $(DEF_PKGLIBDIR) $(DEF_DEFAULT_PATH) -DTIMIDITY_OUTPUT_ID=\"d\"

#DEFS = -DHAVE_CONFIG_H
#LDFLAGS =  -Lc -L/emx/lib;c -L/xfree86/lib -L/usr/local/lib
#LIBS = -lsocket -lm  timidmcd.def -ldl      -los2me
#SHLD = gcc -shared  -Lc -L/emx/lib;c -L/xfree86/lib -L/usr/local/lib
#SHCFLAGS =  -fPIC
#


## Package installations:
#prefix = /usr/local
#exec_prefix = ${prefix}
#bindir = ${exec_prefix}/bin
#libdir = ${exec_prefix}/lib
#datadir = c:
#mandir = ${prefix}/man
pkglibdir = $(libdir)/timidity
pkgdatadir = $(datadir)/timidity
#INSTALL = c:/utils/ginstall -c

# Where to install the patches, config files.
PKGDATADIR = $(pkgdatadir)

# Where to install the Tcl code and the bitmaps.
# It also contains bitmaps which are shared with XAW interface.
PKGLIBDIR = $(pkglibdir)

# Where to install the dynamic link interface.
SHLIB_DIR = $(pkglibdir)

# Where to install timidity.el
ELISP_DIR = $(pkgdatadir)

# If you want to change TCL_DIR, please do follows.
# * Add -DTKPROGPATH=\"$(TCL_DIR)/tkmidity.tcl\" to CPPFLAGS.
# * Make a symbolic link $(PKGLIBDIR)/bitmaps to $(TCL_DIR)/bitmaps
TCL_DIR = $(PKGLIBDIR)
##CPPFLAGS += -DTKPROGPATH=\"$(TCL_DIR)/tkmidity.tcl\"

# Define the timidity default file search path.
DEF_DEFAULT_PATH = -DDEFAULT_PATH=\"$(PKGDATADIR)\"

# You sould not change follows definitions. 
DEF_PKGDATADIR = -DPKGDATADIR=\"$(PKGDATADIR)\"
DEF_PKGLIBDIR = -DPKGLIBDIR=\"$(PKGLIBDIR)\"
DEF_SHLIB_DIR = -DSHLIB_DIR=\"$(SHLIB_DIR)\"
BITMAP_DIR = $(TCL_DIR)/bitmaps
