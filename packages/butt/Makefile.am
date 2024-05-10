AUTOMAKE_OPTIONS = foreign
if CLIENT_ONLY
SUBDIRS = src 
else
SUBDIRS = po src
endif
EXTRA_DIST = config.rpath m4/ChangeLog  src/aac_dll.h src/xpm config.h src/butt.manifest player_plugins icons usr ChangeLog \
	     AUTHORS THANKS KNOWN_BUGS INSTALL README COPYING NEWS src/FLTK/flgui.fl \
	     src/FLTK/Fl_My_Native_File_Chooser_MAC.mm src/FLTK/Fl_My_Native_File_Chooser_WIN32.cxx \
	     src/FLTK/Fl_My_Native_File_Chooser_FLTK.cxx src/FLTK/Fl_My_Native_File_Chooser_common.cxx \
	     src/FLTK/Fl_My_Native_File_Chooser_GTK.cxx src/AskForMicPermission.m src/AskForMicPermission.h \
	     butt.1 gettext.h


ACLOCAL_AMFLAGS = -I m4

#install-exec-hook:
#	cp po/de.gmo locale/de/LC_MESSAGES/butt.mo
#	cp po/fr.gmo locale/fr/LC_MESSAGES/butt.mo
#	cp po/pt_BR.gmo locale/pt_BR/LC_MESSAGES/butt.mo
