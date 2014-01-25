AC_PREREQ([2.61])
AC_INIT([dlvhex-dlliteplugin],[2.0.0],[dlvhex-devel@lists.sourceforge.net])
AC_CONFIG_SRCDIR([src/DLLitePlugin.cpp])
AC_CONFIG_HEADERS([src/config.h])

# for documentation of the versioning scheme see
# https://sourceforge.net/apps/trac/dlvhex/wiki/LibraryVersions
DLVHEX_DEFINE_VERSION([DLLITEPLUGIN],[$PACKAGE_VERSION])
DLVHEX_DEFINE_VERSION([DLVHEX_ABI],[7.3.0])

AM_INIT_AUTOMAKE

AC_CANONICAL_HOST

# by default we look into /opt/local and /sw for include files (macports vs. fink)
case $host_os in
     darwin*|macosx*)
	CPPFLAGS="$CPPFLAGS -I/opt/local/include -I/sw/include"
     ;;
esac 


# Checks for programs.
AC_PROG_CXX
AC_LANG([C++])

# Checks for header files.
AC_HEADER_DIRENT
AC_CHECK_HEADERS([sys/param.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST

# Checks for library functions.
AC_LIBTOOL_DLOPEN # we build a module library
AC_DISABLE_STATIC # we don't need a static library
AC_PROG_LIBTOOL

#
# check for other libraries here
#

#
# default checkout, build, configure for owlcpp
#
AC_ARG_WITH([owlcpp],
  AS_HELP_STRING([--with-owlcpp=<owlcpp-trunk-directory> or --with-owlcpp=auto],
                 [use owlcpp]),
  [with_owlcpp=$withval],
  [with_owlcpp=auto])

# check for dlvhex, sets various variables
DLVHEX_REQUIRE([2.3.0])

# check owlcpp support
AM_CONDITIONAL([DO_BUILDOWLCPP],[test "x${with_owlcpp}" == xauto])
if test "x$with_owlcpp" == xauto; then
	OWLCPP_ROOT=`cd ./owlcpp; pwd`
	EXTSOLVER_LIBADD="${EXTSOLVER_LIBADD} -lowlcpp_io -lowlcpp_logic -lowlcpp_rdf -lfactpp_kernel -lraptor2 -lxml2"
	EXTSOLVER_LDFLAGS="${EXTSOLVER_LDFLAGS} -L$OWLCPP_ROOT/libs"
	EXTSOLVER_CPPFLAGS="${EXTSOLVER_CPPFLAGS} -I$OWLCPP_ROOT/include"

	AC_SUBST([DO_BUILDOWLCPP], ["yes"])
	AC_DEFINE([HAVE_OWLCPP], [1], [Defined if we found owlcpp.])
	AM_CONDITIONAL([HAVE_OWLCPP],[true])

	# for owlcpp we need 1.55 for functional/factory
	BOOST_REQUIRE([1.55.0])
elif test "x$with_owlcpp" != xauto -a "x$with_owlcpp" != x; then
	OWLCPP_ROOT=$with_owlcpp
	LOCAL_OWLCPP_ROOT=`cd ./owlcpp; pwd`
	EXTSOLVER_LIBADD="${EXTSOLVER_LIBADD} -lowlcpp_io -lowlcpp_logic -lowlcpp_rdf -lfactpp_kernel -lraptor2 -lxml2"
	EXTSOLVER_LDFLAGS="${EXTSOLVER_LDFLAGS} -L$LOCAL_OWLCPP_ROOT/libs"
	EXTSOLVER_CPPFLAGS="${EXTSOLVER_CPPFLAGS} -I$LOCAL_OWLCPP_ROOT/include"

	AC_SUBST([DO_BUILDOWLCPP], ["no"])
	AC_DEFINE([HAVE_OWLCPP], [1], [Defined if we found owlcpp.])
	AM_CONDITIONAL([HAVE_OWLCPP],[true])

	# we need 1.55 for functional/factory
	BOOST_REQUIRE([1.55.0])
else
	AC_SUBST([DO_BUILDOWLCPP], ["no"])
	AM_CONDITIONAL([HAVE_OWLCPP],[false])
	AC_MSG_ERROR([Cannot build without owlcpp!])
fi
AC_SUBST([OWLCPP_ROOT])
AC_SUBST(EXTSOLVER_CPPFLAGS)
AC_SUBST(EXTSOLVER_LDFLAGS)
AC_SUBST(EXTSOLVER_LIBS)
AC_SUBST(EXTSOLVER_LIBADD)

# checking for boost libs
BOOST_REQUIRE([1.43.0])
BOOST_SMART_PTR
BOOST_STRING_ALGO
BOOST_TOKENIZER

NESTED_BOOSTROOT=""
if test "x$with_boost" != xno -a "x$with_boost" != xyes -a "x$with_boost" != x; then
  NESTED_BOOSTROOT=$with_boost
fi
AC_SUBST([NESTED_BOOSTROOT])
AC_SUBST([BOOST_MAJOR_VERSION],[$boost_major_version])

# where the plugin will be installed by default.
# running configure will take sysplugindir from dlvhex.pc,
# running configure --enable-userinstall will take homeplugindir.
AC_ARG_ENABLE(userinstall, 
             [  --enable-userinstall    Install plugin into home directory],
             [plugindir=$HOME/$DLVHEX_USERPLUGINDIR],
             [plugindir=$DLVHEX_SYSPLUGINDIR]
             )
AC_SUBST([plugindir])
if test "x$plugindir" == "x"; then
	AC_MSG_ERROR([Cannot determine nonempty value for plugindir!])
fi

#
# all needed makefiles
#
AC_CONFIG_FILES([
           Makefile 
           src/Makefile
           include/Makefile
           examples/Makefile
           owlcpp/Makefile
])

AC_OUTPUT

dnl output configuration
if test "x${with_owlcpp}" == xauto ; then
	OWLCPP_OUT="$OWLCPP_ROOT (download and build automatically)"
else
	OWLCPP_OUT="$OWLCPP_ROOT"
fi

dnl output configuration
cat <<EOF
 
$PACKAGE_TARNAME $PACKAGE_VERSION configuration:

 - Compiler settings
 CC: $CC
 CXX: $CXX
 CFLAGS: $CFLAGS
 CPPFLAGS: $CPPFLAGS
 CXXFLAGS: $CXXFLAGS
 LDFLAGS: $LDFLAGS

 - owlcpp
 owlcpp: $OWLCPP_OUT

 - from pkg-config
 DLVHEX_CFLAGS: $DLVHEX_CFLAGS
 DLVHEX_LIBS: $DLVHEX_LIBS
 DLVHEX_BINDIR: $DLVHEX_BINDIR
 
 - Path settings
 prefix: $prefix
 plugindir: $plugindir
EOF
