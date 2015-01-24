#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([dchat-gui], [0.9], [christoph.mahrl@gmail.com])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_CONFIG_SRCDIR([src/dchat-gui.c])
AC_CONFIG_HEADERS([config.h])

PREFIX=`test "$prefix" = NONE && prefix=""; eval echo "${prefix}"`

AC_DEFINE_UNQUOTED([INP_SOCK_PATH], ["$PREFIX/var/run/dinp.sock"], [Location of user interface input socket])
AC_DEFINE_UNQUOTED([OUT_SOCK_PATH], ["$PREFIX/var/run/dout.sock"], [Location of user interface output socket])
AC_DEFINE_UNQUOTED([LOG_SOCK_PATH], ["$PREFIX/var/run/dlog.sock"], [Location of user interface logging socket])

# Checks for programs.
AC_PROG_CC
AC_PROG_CC_STDC

#Doxygen
#DX_HTML_FEATURE(ON)
#DX_CHM_FEATURE(OFF)
#DX_CHI_FEATURE(OFF)
#DX_MAN_FEATURE(OFF)
#DX_RTF_FEATURE(OFF)
#DX_XML_FEATURE(OFF)
#DX_PDF_FEATURE(OFF)
#DX_PS_FEATURE(OFF)
#DX_INIT_DOXYGEN([dchat], [doxygen.cfg])

# Checks for libraries.
AX_WITH_CURSES

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h string.h])

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([memset])

AC_CONFIG_FILES([Makefile
                 src/Makefile])

AC_OUTPUT
