# // **************************************************************************
# // * Copyright (C) 2015 abcdec @GitHub - All Rights Reserved                *
# // * This file is part of a modified version of FreeFileSync, MinFFS.       *
# // * The original FreeFileSync program and source code are distributed by   *
# // * the FreeFileSync project: http://www.freefilesync.org/                 *
# // * This particular file is created by by abcdec @GitHub as part of the    *
# // * MinFFS project: https://github.com/abcdec/MinFFS                       *
# // *                          --EXPERIMENTAL--                              *
# // * This program is experimental and not recommended for general use.      *
# // * Please consider using the original FreeFileSync program unless there   *
# // * are specific needs to use this experimental MinFFS version.            *
# // *                          --EXPERIMENTAL--                              *
# // * This file is distributed under GNU General Public License:             *
# // * http://www.gnu.org/licenses/gpl-3.0 per the FreeFileSync License.      *
# // * This modified program is distributed in the hope that it will be       *
# // * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of *
# // * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU       *
# // * General Public License for more details.                               *
# // **************************************************************************

include Makefile-common.mk

OBJECT_LIST = $(CPP_LIST:%.cpp=%.o) res.o


all: FreeFileSync

.cpp.o:
	$(CXX) $(CXXFLAGS) -c $< -o $@

.rc.o:
	windres $< $@

res.o:	res.rc

FreeFileSync:	Ex$(APPNAME)

Ex$(APPNAME):	$(OBJECT_LIST)
	$(CXX) -o Ex$(APPNAME) $(OBJECT_LIST) $(LINKFLAGS)

#TODO for native cmd.exe+MinGW build
#clean:
#	del $(APPNAME) $(OBJECT_LIST)
#

#
#install:
#	mkdir -p $(BINDIR)
#	cp Build/$(APPNAME) $(BINDIR)
#
#	mkdir -p $(APPSHAREDIR)
#	cp -R ../Build/Languages/ \
#	../Build/Help/ \
#	../Build/Sync_Complete.wav \
#	../Build/Resources.zip \
#	../Build/styles.gtk_rc \
#	$(APPSHAREDIR)
#	mkdir -p $(DOCSHAREDIR)
#	cp ../Build/Changelog.txt $(DOCSHAREDIR)/changelog
#	gzip $(DOCSHAREDIR)/changelog

# TODO_MinFFS DEBUGGING ONLY TO BE REMOVED Begin

#filecheck:
#	@echo TODO files
#	@grep -l TODO_MinFFS $(CPP_LIST)
#	@echo
#	@echo List of Object files compiled
#	@ls -1 $(OBJECT_LIST)

sanity.exe: sanity.o
	$(CXX) -o sanity.exe sanity.o $(LINKFLAGS)

#cleansanity:
#	rm -rf sanity.exe sanity.o

# TODO_MinFFS DEBUGGING ONLY TO BE REMOVED End
