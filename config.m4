dnl $Id: config.m4,v 1.4 2008/01/11 10:01:42 tony Exp $

PHP_ARG_WITH(pssh, for pssh support,
[  --with-pssh             Include pssh support])

if test "$PHP_PSSH" != "no"; then

  SEARCH_PATH="/usr/local /usr /local"
  SEARCH_FOR="/include/pssh.h"

  if test -r $PHP_PSSH/; then
    AC_MSG_CHECKING([for libpssh headers in $PHP_PSSH])
    if test -r "$PHP_PSSH/$SEARCH_FOR"; then
      PSSH_DIR=$PHP_PSSH
      AC_MSG_RESULT([found])
    fi
  else
    AC_MSG_CHECKING([for libpssh headers in default path])
    for i in $SEARCH_PATH ; do
      if test -r $i/$SEARCH_FOR; then
        PSSH_DIR=$i
        AC_MSG_RESULT([found in $i])
		break;
      fi
    done
  fi

  if test -z "$PSSH_DIR"; then
    AC_MSG_RESULT([not found])
    AC_MSG_ERROR([Please (re)install libpssh])
  fi

  PHP_ADD_INCLUDE($PSSH_DIR/include)

  LIBNAME=pssh
  LIBSYMBOL=pssh_connect

  PSSH_LIBDIR=lib

  PHP_CHECK_LIBRARY($LIBNAME,$LIBSYMBOL,
  [
    PHP_ADD_LIBRARY_WITH_PATH($LIBNAME, $PSSH_DIR/$PSSH_LIBDIR, PSSH_SHARED_LIBADD)
    AC_DEFINE(HAVE_PSSHLIB,1,[ ])
  ],[
    AC_MSG_ERROR([invalid libpssh version or libpssh not found, see more details in config.log])
  ],[
    -L$PSSH_DIR/$PSSH_LIBDIR -lpssh
  ])

  PHP_SUBST(PSSH_SHARED_LIBADD)

  PHP_NEW_EXTENSION(pssh, pssh.c, $ext_shared)
fi
