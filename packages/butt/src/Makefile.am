# what flags you want to pass to the C compiler & linker

AM_CPPFLAGS = -IFLTK
AM_CPPFLAGS += -DLOCALEDIR='"$(localedir)"'
AM_CPPFLAGS += -D_="gettext"

#
# # this lists the binaries to produce, the (non-PHONY, binary) targets in
# # the previous manual Makefile

bin_PROGRAMS = 

if !CLIENT_ONLY
bin_PROGRAMS += butt
butt_SOURCES = butt.cpp butt.h cfg.cpp cfg.h icecast.cpp icecast.h lame_encode.cpp tls.cpp tls.h \
			   lame_encode.h parseconfig.cpp parseconfig.h port_midi.cpp port_midi.h port_audio.cpp \
			   port_audio.h ringbuffer.cpp ringbuffer.h shoutcast.cpp shoutcast.h \
			   sockfuncs.cpp sockfuncs.h strfuncs.cpp strfuncs.h timer.cpp timer.h \
			   util.cpp util.h vorbis_encode.cpp vorbis_encode.h vu_meter.cpp vu_meter.h \
			   wav_header.cpp wav_header.h opus_encode.cpp opus_encode.h flac_encode.cpp flac_encode.h \
			   dsp.cpp dsp.hpp Biquad.cpp Biquad.h command.cpp command.h update.cpp update.h \
			   tray_agent.cpp tray_agent.h sha256.cpp sha256.h cJSON.cpp cJSON.h url.cpp url.h atom.h \
			   FLTK/Fl_ILM216.cpp FLTK/Fl_ILM216.h FLTK/fl_callbacks.cpp \
			   FLTK/fl_callbacks.h FLTK/fl_funcs.cpp FLTK/fl_funcs.h FLTK/flgui.cpp FLTK/flgui.h \
			   FLTK/FL/Fl_My_Double_Window.H FLTK/FL/Fl_My_Value_Slider.H \
			   FLTK/Fl_My_Native_File_Chooser.cxx FLTK/FL/Fl_My_Native_File_Chooser.H FLTK/Fl_vu_meter.cpp FLTK/Fl_vu_meter.h \
			   FLTK/fl_timer_funcs.cpp FLTK/fl_timer_funcs.h aac_encode.cpp aac_encode.h \
			   FLTK/Fl_LED.cpp FLTK/Fl_LED.h FLTK/FL/Fl_My_Invisible_Box.H 
if WITH_RADIOCO
nodist_butt_SOURCES = radioco.cpp radioco.h oauth.cpp oauth.h 
endif

if WINDOWS
butt_SOURCES += resource.rc currentTrack.h currentTrack.cpp
butt_LDADD = -lintl

# used only under MinGW to compile the resource.rc file (manifest and program icon)
#
.rc.o:
	windres $^ -o $@
%.o : %.rc
	windres $^ -o $@
endif

if OSX
butt_SOURCES += CurrentTrackOSX.h CurrentTrackOSX.m AskForMicPermission.h AskForMicPermission.m
endif

if LINUX
butt_SOURCES += currentTrack.h currentTrackLinux.cpp
endif

if FREEBSD
butt_SOURCES += currentTrack.h currentTrackLinux.cpp
endif 

endif #!CLIENT_ONLY


# To build the client on windows run ./configure --without-butt --disable-nls
if WITH_CLIENT 
bin_PROGRAMS += butt-client
butt_client_SOURCES = butt.cpp command.cpp sockfuncs.cpp
butt_client_CPPFLAGS = -DBUILD_CLIENT $(AM_CPPFLAGS)
endif

