# -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

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

AC_PREREQ(2.62)
AC_INIT([pavucontrol],[5.0],[mzcnihpbageby (at) 0pointer (dot) de])
AC_CONFIG_SRCDIR([src/pavucontrol.cc])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([foreign 1.11 -Wall dist-xz tar-ustar])
AM_SILENT_RULES([yes])

AC_SUBST(PACKAGE_URL, [http://freedesktop.org/software/pulseaudio/pavucontrol/])

if type -p stow > /dev/null && test -d /usr/local/stow ; then
   AC_MSG_NOTICE([*** Found /usr/local/stow: default install prefix set to /usr/local/stow/${PACKAGE_NAME}-${PACKAGE_VERSION} ***])
   ac_default_prefix="/usr/local/stow/${PACKAGE_NAME}-${PACKAGE_VERSION}"
fi

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_LN_S
AC_TYPE_SIGNAL
AC_HEADER_STDC
AX_CXX_COMPILE_STDCXX_11

PKG_CHECK_MODULES(GUILIBS, [ gtkmm-3.0 >= 3.22 sigc++-2.0 libcanberra-gtk3 >= 0.16 json-glib-1.0 ])
AC_SUBST(GUILIBS_CFLAGS)
AC_SUBST(GUILIBS_LIBS)

if test -d ../pulseaudio ; then
   PULSE_CFLAGS='-I$(top_srcdir)/../pulseaudio/src'
   PULSE_LIBS='-L$(top_srcdir)/../pulseaudio/src/.libs -lpulse -lpulse-mainloop-glib'
   echo "*** Found pulseaudio in ../pulseaudio, using that version ***"
else
   PKG_CHECK_MODULES(PULSE, [ libpulse >= 5.0 libpulse-mainloop-glib >= 0.9.16 ])
fi

AC_SUBST(PULSE_LIBS)
AC_SUBST(PULSE_CFLAGS)

AC_CHECK_LIB(pulse, pa_context_send_message_to_object, [HAVE_PULSE_MESSAGING_API=yes], [HAVE_PULSE_MESSAGING_API=no])
AS_IF([test "x$HAVE_PULSE_MESSAGING_API" = "xyes"], AC_DEFINE([HAVE_PULSE_MESSAGING_API], 1, [Have messaging API.]))

# If using GCC specifiy some additional parameters
if test "x$GCC" = "xyes" ; then
   CFLAGS="$CFLAGS -pipe -Wall -W -Wno-unused-parameter"
   CXXFLAGS="$CXXFLAGS -pipe -Wall -W"
fi

ZP_LYNX_DOC

IT_PROG_INTLTOOL([0.35.0])
GETTEXT_PACKAGE=pavucontrol
AC_SUBST([GETTEXT_PACKAGE])
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE],["$GETTEXT_PACKAGE"],[Gettext package])
AM_GLIB_GNU_GETTEXT

AC_CONFIG_FILES([
po/Makefile.in
Makefile
src/Makefile
doc/Makefile
doc/README.html])

AC_OUTPUT
