#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([GDFS], [1.0.0], [robinthomas2591@gmail.com])
AC_CONFIG_AUX_DIR(config)
AC_CONFIG_SRCDIR([src/main.cc])
LT_INIT
AC_CANONICAL_TARGET
AC_CONFIG_MACRO_DIR([m4])
AM_SILENT_RULES([yes])
AM_INIT_AUTOMAKE([foreign subdir-objects])
AC_CONFIG_HEADERS(config/config.h)

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_MAKE_SET
AC_PROG_LIBTOOL
AC_SYS_LARGEFILE
AC_PROG_MKDIR_P
AM_PROG_CC_C_O

# Checks for libraries.
AC_CHECK_LIB([curl], [curl_easy_init])
AC_CHECK_LIB([fuse], [fuse_main_real])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDC
AC_CHECK_HEADERS([stdlib.h stdbool.h])
AC_CHECK_MEMBERS([struct stat.st_atim])
AC_CHECK_MEMBERS([struct stat.st_atimespec])
AC_TYPE_SIZE_T
AC_C_CONST

# Checks for functions.
AC_CHECK_FUNCS([fork])

GDFS_PATH=/opt/gdfs
AC_SUBST(GDFS_PATH)

if test -z "$INIT_D_PATH"; then
	INIT_D_PATH=/etc/init.d
	AC_MSG_NOTICE([INIT_D_PATH env var not set, using default $INIT_D_PATH])
fi
AC_SUBST(INIT_D_PATH)

AC_CONFIG_FILES([Makefile src/Makefile lib/Makefile util/Makefile])
AC_OUTPUT
