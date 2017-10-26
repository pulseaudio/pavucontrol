# This file is part of pavucontrol.
#
# Copyright 2006-2008 Lennart Poettering
#
# pavucontrol is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# pavucontrol is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with pavucontrol. If not, see <http://www.gnu.org/licenses/>.

desktopdir = $(datadir)/applications
gladedir = $(pkgdatadir)
localedir = $(datadir)/locale

bin_PROGRAMS=pavucontrol
glade_DATA=pavucontrol.glade
desktop_in_files=pavucontrol.desktop.in
desktop_DATA=$(desktop_in_files:.desktop.in=.desktop)

pavucontrol_SOURCES= \
  minimalstreamwidget.h minimalstreamwidget.cc \
  channelwidget.h channelwidget.cc \
  streamwidget.h streamwidget.cc \
  devicewidget.h devicewidget.cc \
  cardwidget.h cardwidget.cc \
  sinkwidget.h sinkwidget.cc \
  sourcewidget.h sourcewidget.cc \
  sinkinputwidget.h sinkinputwidget.cc \
  sourceoutputwidget.h sourceoutputwidget.cc \
  rolewidget.h rolewidget.cc \
  mainwindow.h mainwindow.cc \
  pavucontrol.h pavucontrol.cc \
  pavuapplication.cc pavuapplication.h \
  i18n.h

pavucontrol_LDADD=$(AM_LDADD) $(GUILIBS_LIBS) $(PULSE_LIBS)
pavucontrol_CXXFLAGS=$(AM_CXXFLAGS) $(GUILIBS_CFLAGS) $(PULSE_CFLAGS) -DLOCALEDIR=\"$(localedir)\"
pavucontrol_CXXFLAGS+=-DGLADE_FILE=\"$(gladedir)/pavucontrol.glade\"
#pavucontrol_CXXFLAGS+=-DGLADE_FILE=\"pavucontrol.glade\"

EXTRA_DIST = $(glade_DATA) $(desktop_in_files)

CLEANFILES = $(desktop_DATA)

@INTLTOOL_DESKTOP_RULE@
