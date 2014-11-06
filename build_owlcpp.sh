# $1: $(OWLCPP_ROOT): If equivalept to $OWLCPPMAINDIR, owlcpp is actually downloaded and built. Otherwise we create only symbolic links to the libraries.
# $2: Boost version to use

if [ $# -eq 0 ]; then
	echo "Error: \$1 must point to OWLCPP_ROOT"
	exit 1
fi

LIBXML2V=2.9.0
RAPTOR2V=2.0.8
FACTPPV=1.6.2
OWLCPPV=0.3.3
if [ $# -gt 1 ]; then
	boost_major_version=$2
	BOOSTV="${boost_major_version:0:1}.${boost_major_version:1:2}.0"
	if [ $boost_major_version -lt 155 ]; then
		echo -e "Warning: dlvhex was built with boost version $BOOSTV, but owlcpp needs at least 1.55.0.\nWill build owlcpp with boost 1.55.0, but it might be incompatible with dlvhex!"
		BOOSTV="1.55.0"
	fi
else
	# default
	BOOSTV=1.55.0
fi
BOOSTVU=$(echo $BOOSTV | sed 's/\./_/g')

OWLCPPMAINDIR=`cd ./$TOP_SRCDIR/owlcpp; pwd`
OWLCPP_ROOT=$1

if [ -f $OWLCPPMAINDIR/successfully_built ]; then
	echo "Skipping owlcpp because it was already built without errors"
	exit 0
fi

osx=$(uname -a | grep "Darwin" | wc -l)

echo "Building owlcpp in $OWLCPPMAINDIR"
if test $OWLCPPMAINDIR == $OWLCPP_ROOT; then

	if [ ! -f $OWLCPPMAINDIR/boost_$BOOSTVU/tools/build/v2/b2 ]; then
		echo "Building boost.build"
		pushd $OWLCPPMAINDIR/boost_$BOOSTVU/tools/build/v2 > /dev/null
		./bootstrap.sh > $OWLCPPMAINDIR/output.out 2>&1
		if [ $? -gt 0 ]; then
			cat $OWLCPPMAINDIR/output.out
			echo "Building boost.build failed; aborting"
			popd > /dev/null
			exit 1
		fi
		popd > /dev/null
		cp $OWLCPPMAINDIR/boost_$BOOSTVU/tools/build/v2/b2 $OWLCPPMAINDIR/owlcpp-$OWLCPPV/
	fi

	if [ ! -f $OWLCPPMAINDIR/boost_$BOOSTVU/b2 ]; then
		echo "Bootstrapping boost"
		pushd $OWLCPPMAINDIR/boost_$BOOSTVU > /dev/null
		./bootstrap.sh > $OWLCPPMAINDIR/output.out 2>&1
		if [ $? -gt 0 ]; then
			cat $OWLCPPMAINDIR/output.out
			echo "Building boost failed; aborting"
			popd > /dev/null
			exit 1
		fi
		echo "Building boost"
		./b2 "$params" > $OWLCPPMAINDIR/output.out 2>&1
		if [ $? -gt 0 ]; then
			cat $OWLCPPMAINDIR/output.out
			echo "Building boost failed; aborting"
			popd > /dev/null
			exit 1
		fi
		popd > /dev/null
	fi

	if [ ! -f $OWLCPPMAINDIR/libxml2-$LIBXML2V/include/libxml/xmlversion.h ]; then
		echo "Building libxml2"
		pushd $OWLCPPMAINDIR/libxml2-$LIBXML2V > /dev/null
		./autogen.sh > $OWLCPPMAINDIR/output.out 2>&1
		if [ $? -gt 0 ]; then
			cat $OWLCPPMAINDIR/output.out
			echo "Building libxml2 failed; aborting"
			popd > /dev/null
			exit 1
		fi
		popd > /dev/null
	fi

	echo "Building owlcpp"
	REALHOME=$HOME
	export HOME=$OWLCPPMAINDIR/owlcpp-$OWLCPPV
	export BOOST_ROOT=$OWLCPPMAINDIR/boost_$BOOSTVU
	export BOOST_BUILD_PATH=$OWLCPPMAINDIR/boost_$BOOSTVU/tools/build/v2
	pushd $OWLCPPMAINDIR/owlcpp-$OWLCPPV > /dev/null
	$BOOST_ROOT/tools/build/v2/b2 release cxxflags="-fpic -export-dynamic" cflags="-fpic -export-dynamic" "$params" > $OWLCPPMAINDIR/output.out 2>&1
	ret=$?
	export HOME=$REALHOME
	if [ $ret -gt 0 ]; then
		cat $OWLCPPMAINDIR/output.out
		echo "Building owlcpp failed; aborting"
		exit 1
	fi

	OWLCPP_ROOT=$OWLCPPMAINDIR/owlcpp-$OWLCPPV
fi

# scan $(OWLCPP_ROOT) for .a and header files and create symbolic links in $OWLCPPMAINDIR/include and $OWLCPPMAINDIR/libs
echo "Creating symbolic links to owlcpp"
mkdir $OWLCPPMAINDIR/include 2> /dev/null
rm $OWLCPPMAINDIR/include/*
ln -s $OWLCPP_ROOT/include/owlcpp $OWLCPPMAINDIR/include/owlcpp
ln -s $OWLCPP_ROOT/out/include/factpp $OWLCPPMAINDIR/include/factpp
ln -s $OWLCPP_ROOT/out/include/libxml $OWLCPPMAINDIR/include/libxml
ln -s $OWLCPP_ROOT/out/include/raptor $OWLCPPMAINDIR/include/raptor
mkdir $OWLCPPMAINDIR/libs 2> /dev/null
rm $OWLCPPMAINDIR/libs/*.a

if [ $(ls $OWLCPP_ROOT/out/bin/io/*/release/link-static/libowlcpp_io.a | wc -l) -eq 0 ]; then
	echo "Error: libowlcpp_io.a not found in $OWLCPP_ROOT/out/bin/io/*/release/link-static"
	exit 1
fi
ln -s $(ls $OWLCPP_ROOT/out/bin/io/*/release/link-static/libowlcpp_io.a | head) $OWLCPPMAINDIR/libs/libowlcpp_io.a

if [ $(ls $OWLCPP_ROOT/out/bin/logic/*/release/link-static/libowlcpp_logic.a | wc -l) -eq 0 ]; then
        echo "Error: libowlcpp_logic.a not found in $OWLCPP_ROOT/out/bin/logic/*/release/link-static"
        exit 1
fi
ln -s $(ls $OWLCPP_ROOT/out/bin/logic/*/release/link-static/libowlcpp_logic.a | head) $OWLCPPMAINDIR/libs/libowlcpp_logic.a

if [ $(ls $OWLCPP_ROOT/out/bin/rdf/*/release/link-static/libowlcpp_rdf.a | wc -l) -eq 0 ]; then
        echo "Error: libowlcpp_rdf.a not found in $OWLCPP_ROOT/out/bin/rdf/*/release/link-static"
        exit 1
fi
ln -s $(ls $OWLCPP_ROOT/out/bin/rdf/*/release/link-static/libowlcpp_rdf.a | head) $OWLCPPMAINDIR/libs/libowlcpp_rdf.a

if [ $(ls $OWLCPP_ROOT/out/ext/factpp/factpp/*/release/link-static/libfactpp_kernel*.a | wc -l) -eq 0 ]; then
        echo "Error: libfactpp_kernel*.a not found in $OWLCPP_ROOT/out/ext/factpp/factpp/*/release/link-static/libfactpp_kernel*.a"
        exit 1
fi
ln -s $(ls $OWLCPP_ROOT/out/ext/factpp/factpp/*/release/link-static/libfactpp_kernel*.a | head) $OWLCPPMAINDIR/libs/libfactpp_kernel.a

if [ $(ls $OWLCPP_ROOT/out/ext/libxml2/libxml2/*/release/libxml2-version-*/link-static/libxml2*.a | wc -l) -eq 0 ]; then
        echo "Error: libxml2*.a not found in $OWLCPP_ROOT/out/ext/libxml2/libxml2/*/release/libxml2-version-*/link-static/libxml2*.a"
        exit 1
fi
ln -s $(ls $OWLCPP_ROOT/out/ext/libxml2/libxml2/*/release/libxml2-version-*/link-static/libxml2*.a | head) $OWLCPPMAINDIR/libs/libxml2.a

if [ $(ls $OWLCPP_ROOT/out/ext/raptor/raptor/*/release/link-static/raptor-*/libraptor*.a | wc -l) -eq 0 ]; then
        echo "Error: libraptor*.a not found in $OWLCPP_ROOT/out/ext/raptor/raptor/*/release/link-static/raptor-*/libraptor*.a"
        exit 1
fi
ln -s $(ls $OWLCPP_ROOT/out/ext/raptor/raptor/*/release/link-static/raptor-*/libraptor*.a | head) $OWLCPPMAINDIR/libs/libraptor2.a

echo "This file just marks that owlcpp was successfully built. Remove it to rebuild." > $OWLCPPMAINDIR/successfully_built

