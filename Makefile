NAME= SimplyVorbis
TYPE= APP
SRCS= AboutWindow.cpp App.cpp AutoTextControl.cpp CDAudioDevice.cpp CDDBSupport.cpp MainWindow.cpp Preferences.cpp PrefsWindow.cpp RipSupport.cpp RipView.cpp TypedList.cpp
RSRCS= SimplyVorbis.rsrc
LIBS= /boot/system/lib/libbe.so /boot/system/lib/libgame.so /boot/system/lib/libmedia.so /boot/system/lib/libnet.so /boot/system/lib/libnetapi.so /boot/system/lib/libroot.so /boot/system/lib/libtextencoding.so /boot/system/lib/libtracker.so /boot/system/lib/libtranslation.so ogg vorbis
LIBPATHS=
SYSTEM_INCLUDE_PATHS= /boot/develop/headers/be /boot/develop/headers/cpp /boot/develop/headers/posix /boot/develop/lib /boot/beos/system/lib /vorbis /ogg /
LOCAL_INCLUDE_PATHS= boot/home/config/lib
OPTIMIZE=NONE
#	specify any preprocessor symbols to be defined.  The symbols will not
#	have their values set automatically; you must supply the value (if any)
#	to use.  For example, setting DEFINES to "DEBUG=1" will cause the
#	compiler option "-DDEBUG=1" to be used.  Setting DEFINES to "DEBUG"
#	would pass "-DDEBUG" on the compiler's command line.
DEFINES=
#	specify special warning levels
#	if unspecified default warnings will be used
#	NONE = supress all warnings
#	ALL = enable all warnings
WARNINGS =
# Build with debugging symbols if set to TRUE
SYMBOLS=
COMPILER_FLAGS=-Woverloaded-virtual -funsigned-bitfields -Wwrite-strings 
LINKER_FLAGS= -lvorbisenc

## include the makefile-engine
DEVEL_DIRECTORY := $(shell findpaths -r "makefile_engine" B_FIND_PATH_DEVELOP_DIRECTORY)

include $(DEVEL_DIRECTORY)/etc/makefile-engine