
APPNAME     := FreeFileSyncMod

#PROJECT_TOP := $(shell cygpath -m -a ../../..)
PROJECT_TOP := ../../..
#MINGW_ROOT  := C:/MinGW
WXWGT_ROOT  := C:/wxWidgets
BOOST_ROOT  := C:/Boost/include/boost-1_54
BOOST_VER   := 1_54

CXXFLAGS := -std=c++11 -pipe
CXXFLAGS += -D__WXMSW__
CXXFLAGS += -DZEN_WIN
CXXFLAGS += -DMinFFS
CXXFLAGS += -DWXINTL_NO_GETTEXT_MACRO
CXXFLAGS += -I$(PROJECT_TOP)/Source
CXXFLAGS += -I$(PROJECT_TOP)
CXXFLAGS += -I$(PROJECT_TOP)/zenXml
CXXFLAGS += -IC:/Boost/include/boost-1_54
CXXFLAGS += -include "zen/i18n.h"
CXXFLAGS += -include "zen/warn_static.h"
CXXFLAGS += -Wall
CXXFLAGS += -O3
CXXFLAGS += -DNDEBUG
CXXFLAGS += -I$(WXWGT_ROOT)/include
CXXFLAGS += -I$(WXWGT_ROOT)/lib/gcc_lib/mswud
CXXFLAGS += -I$(BOOST_ROOT)/include/boost-$(BOOST_VER)

LINKFLAGS := -s
LINKFLAGS += -L$(WXWGT_ROOT)/lib/gcc_lib

#CXX := $(MINGW_ROOT)/bin/g++
CXX := g++

CPP_LIST=
CPP_LIST+=$(PROJECT_TOP)/FreeFileSync/Source/algorithm.cpp
CPP_LIST+=$(PROJECT_TOP)/FreeFileSync/Source/application.cpp
CPP_LIST+=$(PROJECT_TOP)/FreeFileSync/Source/comparison.cpp
CPP_LIST+=$(PROJECT_TOP)/FreeFileSync/Source/structures.cpp
CPP_LIST+=$(PROJECT_TOP)/FreeFileSync/Source/synchronization.cpp
CPP_LIST+=$(PROJECT_TOP)/FreeFileSync/Source/file_hierarchy.cpp
CPP_LIST+=$(PROJECT_TOP)/FreeFileSync/Source/ui/custom_grid.cpp
CPP_LIST+=$(PROJECT_TOP)/FreeFileSync/Source/ui/folder_history_box.cpp
CPP_LIST+=$(PROJECT_TOP)/FreeFileSync/Source/ui/on_completion_box.cpp
CPP_LIST+=$(PROJECT_TOP)/FreeFileSync/Source/ui/dir_name.cpp
CPP_LIST+=$(PROJECT_TOP)/FreeFileSync/Source/ui/batch_config.cpp
CPP_LIST+=$(PROJECT_TOP)/FreeFileSync/Source/ui/batch_status_handler.cpp
CPP_LIST+=$(PROJECT_TOP)/FreeFileSync/Source/ui/check_version.cpp
CPP_LIST+=$(PROJECT_TOP)/FreeFileSync/Source/ui/grid_view.cpp
CPP_LIST+=$(PROJECT_TOP)/FreeFileSync/Source/ui/tree_view.cpp
CPP_LIST+=$(PROJECT_TOP)/FreeFileSync/Source/ui/gui_generated.cpp
CPP_LIST+=$(PROJECT_TOP)/FreeFileSync/Source/ui/gui_status_handler.cpp
CPP_LIST+=$(PROJECT_TOP)/FreeFileSync/Source/ui/main_dlg.cpp
CPP_LIST+=$(PROJECT_TOP)/FreeFileSync/Source/ui/progress_indicator.cpp
CPP_LIST+=$(PROJECT_TOP)/FreeFileSync/Source/ui/search.cpp
CPP_LIST+=$(PROJECT_TOP)/FreeFileSync/Source/ui/small_dlgs.cpp
CPP_LIST+=$(PROJECT_TOP)/FreeFileSync/Source/ui/sync_cfg.cpp
CPP_LIST+=$(PROJECT_TOP)/FreeFileSync/Source/ui/taskbar.cpp
CPP_LIST+=$(PROJECT_TOP)/FreeFileSync/Source/ui/triple_splitter.cpp
CPP_LIST+=$(PROJECT_TOP)/FreeFileSync/Source/ui/tray_icon.cpp
CPP_LIST+=$(PROJECT_TOP)/FreeFileSync/Source/lib/binary.cpp
CPP_LIST+=$(PROJECT_TOP)/FreeFileSync/Source/lib/db_file.cpp
CPP_LIST+=$(PROJECT_TOP)/FreeFileSync/Source/lib/dir_lock.cpp
CPP_LIST+=$(PROJECT_TOP)/FreeFileSync/Source/lib/hard_filter.cpp
CPP_LIST+=$(PROJECT_TOP)/FreeFileSync/Source/lib/icon_buffer.cpp
CPP_LIST+=$(PROJECT_TOP)/FreeFileSync/Source/lib/localization.cpp
CPP_LIST+=$(PROJECT_TOP)/FreeFileSync/Source/lib/parallel_scan.cpp
CPP_LIST+=$(PROJECT_TOP)/FreeFileSync/Source/lib/process_xml.cpp
CPP_LIST+=$(PROJECT_TOP)/FreeFileSync/Source/lib/resolve_path.cpp
CPP_LIST+=$(PROJECT_TOP)/FreeFileSync/Source/lib/perf_check.cpp
CPP_LIST+=$(PROJECT_TOP)/FreeFileSync/Source/lib/status_handler.cpp
CPP_LIST+=$(PROJECT_TOP)/FreeFileSync/Source/lib/versioning.cpp
CPP_LIST+=$(PROJECT_TOP)/FreeFileSync/Source/lib/ffs_paths.cpp
CPP_LIST+=$(PROJECT_TOP)/zen/xml_io.cpp
CPP_LIST+=$(PROJECT_TOP)/zen/recycler.cpp
CPP_LIST+=$(PROJECT_TOP)/zen/file_access.cpp
CPP_LIST+=$(PROJECT_TOP)/zen/file_io.cpp
CPP_LIST+=$(PROJECT_TOP)/zen/file_traverser.cpp
CPP_LIST+=$(PROJECT_TOP)/zen/zstring.cpp
CPP_LIST+=$(PROJECT_TOP)/zen/format_unit.cpp
CPP_LIST+=$(PROJECT_TOP)/zen/process_priority.cpp
CPP_LIST+=$(PROJECT_TOP)/wx+/grid.cpp
CPP_LIST+=$(PROJECT_TOP)/wx+/image_tools.cpp
CPP_LIST+=$(PROJECT_TOP)/wx+/graph.cpp
CPP_LIST+=$(PROJECT_TOP)/wx+/tooltip.cpp
CPP_LIST+=$(PROJECT_TOP)/wx+/image_resources.cpp
CPP_LIST+=$(PROJECT_TOP)/wx+/popup_dlg.cpp
CPP_LIST+=$(PROJECT_TOP)/wx+/popup_dlg_generated.cpp
CPP_LIST+=$(PROJECT_TOP)/wx+/zlib_wrap.cpp

OBJECT_LIST = $(CPP_LIST:%.cpp=%.o)

all: launchpad

launchpad: FreeFileSync

.cpp.o:
	$(CXX) $(CXXFLAGS) -c $< -o $@


FreeFileSync: $(OBJECT_LIST)
	$(CXX) -o ../Build/$(APPNAME) $(OBJECT_LIST) $(LINKFLAGS)

clean:
	rm -f $(OBJECT_LIST)

filecheck:
	@ls $(OBJECT_LIST)

install:
	mkdir -p $(BINDIR)
	cp ../Build/$(APPNAME) $(BINDIR)

	mkdir -p $(APPSHAREDIR)
	cp -R ../Build/Languages/ \
	../Build/Help/ \
	../Build/Sync_Complete.wav \
	../Build/Resources.zip \
	../Build/styles.gtk_rc \
	$(APPSHAREDIR)

	mkdir -p $(DOCSHAREDIR)
	cp ../Build/Changelog.txt $(DOCSHAREDIR)/changelog
	gzip $(DOCSHAREDIR)/changelog
