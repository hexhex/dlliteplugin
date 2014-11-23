# When this script is called, the working directory must be the build directory of owlcpp
# 
# $1: $(OWLCPP_ROOT): If empty, then owlcpp is automatically downloaded and built. Otherwise it is expected to point to a pre-built version of owlcpp.
# $2: Boost version to use

if [ $# -eq 0 ]; then
	echo "Error: \$1 must point to OWLCPPBUILDDIR"
	exit 1
fi

LIBXML2V=2.9.1
RAPTOR2V=2.0.14
FACTPPV=1.6.2
OWLCPPV=0.3.4
ICONVV=1.9.2 # only for Windows
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

OWLCPP_ROOT=$1
OWLCPPSRCDIR=`cd $TOP_SRCDIR/owlcpp; pwd`
OWLCPPBUILDDIR=`pwd`
echo "OWLCPP_ROOT: $OWLCPP_ROOT"
echo "OWLCPPSRCDIR: $OWLCPPSRCDIR"
echo "OWLCPPBUILDDIR: $OWLCPPBUILDDIR"

osx=$(uname -a | grep "Darwin" | wc -l)
echo "Building owlcpp in $OWLCPPBUILDDIR"
if [[ $OWLCPP_ROOT == "" ]]; then

	if [ -f $OWLCPPBUILDDIR/successfully_built ]; then
		echo "Skipping owlcpp because it was already built without errors"
		exit 0
	fi

	echo "Extracting archives from $OWLCPPSRCDIR in $OWLCPPBUILDDIR"
	if [ ! -d $OWLCPPBUILDDIR/owlcpp-$OWLCPPV ]; then
		unzip $OWLCPPSRCDIR/owlcpp-$OWLCPPV.zip #> /dev/null 2> /dev/null
		# renaming might be necessary if downloaded as tar.gz (not necessary if cloned from git)
		mv $OWLCPPBUILDDIR/owlcpp-v$OWLCPPV $OWLCPPBUILDDIR/owlcpp-$OWLCPPV 2> /dev/null
		patchowlcpp=1
	else
		patchowlcpp=0
	fi
	OWLCPP_ROOT=$OWLCPPBUILDDIR/owlcpp-$OWLCPPV
	if [ ! -d $OWLCPPBUILDDIR/boost_$BOOSTVU ]; then
		tar -xzf $OWLCPPSRCDIR/boost-$BOOSTV.tar.gz #> /dev/null 2> /dev/null
	fi
	if [ ! -d $OWLCPPBUILDDIR/libxml2-$LIBXML2V ]; then
		unzip $OWLCPPSRCDIR/libxml2-$LIBXML2V.zip #> /dev/null 2> /dev/null
		bootstramlibxml2=1
	else
		bootstramlibxml2=0
	fi
	if [ ! -d $OWLCPPBUILDDIR/raptor2-$RAPTOR2V ]; then
		tar -xzf $OWLCPPSRCDIR/raptor2-$RAPTOR2V.tar.gz #> /dev/null 2> /dev/null
	fi
	if [ ! -d $OWLCPPBUILDDIR/FaCT++-$FACTPPV ]; then
		tar -xzf $OWLCPPSRCDIR/FaCTpp-src-v$FACTPPV.tar.gz #> /dev/null 2> /dev/null
	fi
	if [ ! -d $OWLCPPBUILDDIR/libiconv-$ICONVV ]; then
		unzip -d $OWLCPPSRCDIR/libiconv-$ICONVV $OWLCPPSRCDIR/libiconv-$ICONVV-dev.zip #> /dev/null 2> /dev/null
		unzip -d $OWLCPPSRCDIR/libiconv-$ICONVV $OWLCPPSRCDIR/libiconv-$ICONVV-bin.zip #> /dev/null 2> /dev/null

		# some files have the wrong names
		cp $OWLCPPBUILDDIR/libiconv-$ICONVV/bin/libiconv2.dll $OWLCPPBUILDDIR/libiconv-$ICONVV/bin/iconv.dll
		cp $OWLCPPBUILDDIR/libiconv-$ICONVV/lib/libiconv.lib $OWLCPPBUILDDIR/libiconv-$ICONVV/lib/iconv.lib
		cp $OWLCPPBUILDDIR/libiconv-$ICONVV/lib/libiconv.lib $OWLCPPBUILDDIR/libiconv-$ICONVV/lib/iconv_a.lib
	fi

	if [ $patchowlcpp -eq 1 ]; then
		echo "Patching triple_index.hpp"
		# make sure that we have Linux line endings
		tripleindexhpp=$(cat $OWLCPPBUILDDIR/owlcpp-$OWLCPPV/include/owlcpp/rdf/detail/triple_index.hpp | tr -d '\r')
		echo "$tripleindexhpp" > $OWLCPPBUILDDIR/owlcpp-$OWLCPPV/include/owlcpp/rdf/detail/triple_index.hpp
		# apply patch
		echo "78c78
	<       ind.erase(boost::find(ind, t));
	---
	>       ind.erase(std::find(ind.begin(), ind.end(), t));
	107c107
	<       ind.erase(boost::find(ind, t));
	---
	>       ind.erase(std::find(ind.begin(), ind.end(), t));
	" | patch $OWLCPPBUILDDIR/owlcpp-$OWLCPPV/include/owlcpp/rdf/detail/triple_index.hpp
	fi
	if [ $bootstramlibxml2 -eq 1 ]; then
		echo "Bootstrapping libxml2"
		cd $OWLCPPBUILDDIR/libxml2-$LIBXML2V
		./autogen.sh
	fi

	echo "Generating user-config.jam"
	cd $OWLCPPBUILDDIR/owlcpp-$OWLCPPV
	cp doc/user-config.jam user-config.jam
	echo " constant BOOST : \"$OWLCPPBUILDDIR/boost_$BOOSTVU/\" $BOOSTV ;" >> user-config.jam
	echo " constant LIBXML2 : \"$OWLCPPBUILDDIR/libxml2-$LIBXML2V\" $LIBXML2V ;" >> user-config.jam
	echo " constant RAPTOR : \"$OWLCPPBUILDDIR/raptor2-$RAPTOR2V\" $RAPTOR2V ;" >> user-config.jam
	echo " constant FACTPP : \"$OWLCPPBUILDDIR/FaCT++-$FACTPPV\" $FACTPPV ;" >> user-config.jam
	cd ..

	echo "Updating jam files"
	cd $OWLCPPBUILDDIR/owlcpp-$OWLCPPV
	cat jamroot.jam | sed 's/-fvisibility=hidden/-fvisibility=default/' | sed 's/-fvisibility-inlines-hidden//' > jamroot.jam.up
	mv jamroot.jam.up jamroot.jam
	for f in external/ext/*.jam
	do
		cat $f | sed 's/-fvisibility=hidden/-fvisibility=default/' | sed 's/-fvisibility-inlines-hidden//' > $f.up
		mv $f.up $f
	done
	cd ..

	if [ $osx -eq 1 ]; then
		echo "Fixing libtool on OS X"
		cp $OWLCPPBUILDDIR/libxml2-$LIBXML2V/autogen.sh $OWLCPPBUILDDIR/libxml2-$LIBXML2V/autogen.sh.bak
		cat $OWLCPPBUILDDIR/libxml2-$LIBXML2V/autogen.sh | sed 's/libtoolize --version/echo 0/' > $OWLCPPBUILDDIR/libxml2-$LIBXML2V/autogen.sh.bak
		mv $OWLCPPBUILDDIR/libxml2-$LIBXML2V/autogen.sh.bak $OWLCPPBUILDDIR/libxml2-$LIBXML2V/autogen.sh
	fi
	cd ..

	echo "	echo off
		set OWLCPPMAIN=%CD%\\owlcpp

		echo \"Bootstrapping boost build\"
		cd %OWLCPPMAIN%\\boost_$BOOSTVU\\tools\\build\\v2
		call bootstrap.bat

		echo \"Building owlcpp (including other necessary libraries\"
		cd %OWLCPPMAIN%\\owlcpp-$OWLCPPV
		copy lib\io\jamfile_win32.jam lib\io\jamfile.jam
		set OWLCPPMAIN_ESC=%OWLCPPMAIN:\\=/%
		echo local utils = \"%OWLCPPMAIN_ESC%\" ; > %USERPROFILE%\\user-config.jam
		type user-config_win32.jam >> %USERPROFILE%\\user-config.jam
		set BOOST=%OWLCPPMAIN%\\boost_$BOOSTVU
		set BOOST_ROOT=%OWLCPPMAIN%\\boost_$BOOSTVU
		set BOOST_BUILD_PATH=%OWLCPPMAIN%\\boost_$BOOSTVU\\tools\\build\\v2
		set BOOST_BUILD_PATH_ESC=%BOOST_BUILD_PATH:\\=/%
		echo boost-build \"%BOOST_BUILD_PATH_ESC%\" ; > boost-build.jam
		call %OWLCPPMAIN%\\boost_$BOOSTVU\\tools\\build\\v2\\b2.exe release
		call %OWLCPPMAIN%\\boost_$BOOSTVU\\tools\\build\\v2\\b2.exe debug
		del %USERPROFILE%\\user-config.jam

		echo \"Extracting files\"
		cd %OWLCPPMAIN%
		mkdir libs

		echo \"   owlcpp libs\"
		copy %OWLCPPMAIN%\\owlcpp-$OWLCPPV\\out\\bin\\io\\msvc-12.0\\release\\link-static\\threading-multi\\libowlcpp_io.lib %OWLCPPMAIN%\\libs\\
		copy %OWLCPPMAIN%\\owlcpp-$OWLCPPV\\out\\bin\\logic\\msvc-12.0\\release\\link-static\\libowlcpp_logic.lib %OWLCPPMAIN%\\libs\\
		copy %OWLCPPMAIN%\\owlcpp-$OWLCPPV\\out\\bin\\rdf\\msvc-12.0\\release\\link-static\\threading-multi\\libowlcpp_rdf.lib %OWLCPPMAIN%\\libs\\
		copy %OWLCPPMAIN%\\owlcpp-$OWLCPPV\\out\\bin\\io\\msvc-12.0\\debug\\link-static\\threading-multi\\libowlcpp_io.lib libs\\libowlcpp_io-dbg.lib %OWLCPPMAIN%\\libs\\
		copy %OWLCPPMAIN%\\owlcpp-$OWLCPPV\\out\\bin\\logic\\msvc-12.0\\debug\\link-static\\threading-multi\\libowlcpp_logic.lib libs\\libowlcpp_logic-dbg.lib %OWLCPPMAIN%\\libs\\
		copy %OWLCPPMAIN%\\owlcpp-$OWLCPPV\\out\\bin\\rdf\\msvc-12.0\\debug\\link-static\\threading-multi\\libowlcpp_rdf.lib libs\\libowlcpp_rdf-dbg.lib %OWLCPPMAIN%\\libs\\
		echo \"   libiconv libs\"
		copy %OWLCPPMAIN%\\libiconv-$ICONVV\\lib\\iconv.lib %OWLCPPMAIN%\\libs\\libiconv.lib
		echo \"   factpp libs\"
		copy %OWLCPPMAIN%\\owlcpp-$OWLCPPV\\out\\ext\\factpp\\factpp\\msvc-*\\release\\link-static\\libfactpp_kernel*.lib %OWLCPPMAIN%\\libs\\libfactpp_kernel.lib
		copy %OWLCPPMAIN%\\owlcpp-$OWLCPPV\\out\\ext\\factpp\\factpp\\msvc-*\\debug\\link-static\\libfactpp_kernel*-gd.lib %OWLCPPMAIN%\\libs\\libfactpp_kernel-dbg.lib
		echo \"   libxml2 libs\"
		copy %OWLCPPMAIN%\\owlcpp-$OWLCPPV\\out\\ext\\libxml2\\libxml2\\msvc-*\\release\\libxml2-version-$LIBXML2V\\link-static\\threading-multi\\libxml2*-mt-2_9.lib %OWLCPPMAIN%\\libs\\libxml2.lib
		copy %OWLCPPMAIN%\\owlcpp-$OWLCPPV\\out\\ext\\libxml2\\libxml2\\msvc-*\\debug\\libxml2-version-$LIBXML2V\\link-static\\threading-multi\\libxml2*-mt-gd-2_9.lib %OWLCPPMAIN%\\libs\\libxml2-dbg.lib
		echo \"   libraptor libs\"
		copy %OWLCPPMAIN%\\owlcpp-$OWLCPPV\\out\\ext\\raptor\\raptor\\msvc-*\\release\\link-static\\raptor-version-$RAPTOR2V\\threading-multi\\libraptor*-mt-2_0.lib %OWLCPPMAIN%\\libs\\libraptor.lib
		copy %OWLCPPMAIN%\\owlcpp-$OWLCPPV\\out\\ext\\raptor\\raptor\\msvc-*\\debug\\link-static\\raptor-version-$RAPTOR2V\\threading-multi\\libraptor*-mt-gd-2_0.lib %OWLCPPMAIN%\\libs\\libraptor-dbg.lib

		echo \"   owlcpp headers\"
		mkdir include
		mkdir include\\owlcpp
		mkdir include\\owlcpp\\detail
		mkdir include\\owlcpp\\io
		mkdir include\\owlcpp\\io\\detail
		mkdir include\\owlcpp\\logic
		mkdir include\\owlcpp\\logic\\detail
		mkdir include\\owlcpp\\rdf
		mkdir include\\owlcpp\\rdf\\detail
		mkdir include\\owlcpp\\terms
		mkdir include\\owlcpp\\terms\\detail
		copy owlcpp-$OWLCPPV\\include\\owlcpp\\*.h* include\\owlcpp\\
		copy owlcpp-$OWLCPPV\\include\\owlcpp\\detail\\*.h* include\\owlcpp\\detail\\
		copy owlcpp-$OWLCPPV\\include\\owlcpp\\io\\*.h* include\\owlcpp\\io\\
		copy owlcpp-$OWLCPPV\\include\\owlcpp\\io\\detail\\*.h* include\\owlcpp\\io\\detail\\
		copy owlcpp-$OWLCPPV\\include\\owlcpp\\logic\\*.h* include\\owlcpp\\logic\\
		copy owlcpp-$OWLCPPV\\include\\owlcpp\\logic\\detail\\*.h* include\\owlcpp\\logic\\detail\\
		copy owlcpp-$OWLCPPV\\include\\owlcpp\\rdf\\*.h* include\\owlcpp\\rdf\\
		copy owlcpp-$OWLCPPV\\include\\owlcpp\\rdf\\detail\\*.h* include\\owlcpp\\rdf\\detail\\
		copy owlcpp-$OWLCPPV\\include\\owlcpp\\terms\\*.h* include\\owlcpp\\terms\\
		copy owlcpp-$OWLCPPV\\include\\owlcpp\\terms\\detail\\*.h* include\\owlcpp\\terms\\detail\\
		echo \"   factpp headers\"
		mkdir include\\factpp
		copy owlcpp-$OWLCPPV\\out\\include\\factpp\\*.h* include\\factpp\\
		echo \"   libxml2 headers\"
		mkdir include\\libxml
		copy owlcpp-$OWLCPPV\\out\\include\\libxml\\*.h* include\\libxml\\
		echo \"   raptor headers\"
		mkdir include\\raptor
		copy owlcpp-$OWLCPPV\\out\\include\\raptor\\*.h* include\\raptor\\
		" > build_owlcpp.bat

	echo "Fixing jam-file"
	echo "	# owlcpp/lib/io/jamfile.jam
		# part of owlcpp project.
		# Distributed under the Boost Software License, Version 1.0; see doc/license.txt.
		# Copyright Mikhail K Levin 2011

		project lib/io
		   : requirements
		;

		lib owlcpp_io
		   :  #sources
		      [ glob *.cpp ]
		   :  #requirements
		      \$(LIB_REQUIREMENTS)
		      <include>.
		      <library>/owlcpp//rdf
		      <library>/boost//filesystem
		      <library>/owlcpp//raptor
		      <link>shared:<define>OWLCPP_IO_DYN_LINK
		      <toolset>msvc:<threading>multi #fix linking with libboost_filesystem-vc110-mt-XXX
		   :  #default build options
		   :  #usage requirements
		      <link>shared:<define>OWLCPP_IO_DYN_LINK
		      <library>/owlcpp//rdf
		      <library>/boost//filesystem
		      <library>/owlcpp//raptor
		;" > $OWLCPPBUILDDIR/owlcpp-$OWLCPPV/lib/io/jamfile_win32.jam

	echo "Writing user-config.jam"
	cp $OWLCPPBUILDDIR/owlcpp-$OWLCPPV/doc/user-config.jam $OWLCPPBUILDDIR/owlcpp-$OWLCPPV/user-config_win32.jam
	echo " constant BOOST : \"\$(utils)/boost_$BOOSTVU/\" $BOOSTV ;" > $OWLCPPBUILDDIR/owlcpp-$OWLCPPV/user-config_win32.jam
	echo " constant ICONV : \"\$(utils)/libiconv-$ICONVV\" $ICONVV ;" >> $OWLCPPBUILDDIR/owlcpp-$OWLCPPV/user-config_win32.jam
	echo " constant LIBXML2 : \"\$(utils)/libxml2-$LIBXML2V\" $LIBXML2V ;" >> $OWLCPPBUILDDIR/owlcpp-$OWLCPPV/user-config_win32.jam
	echo " constant RAPTOR : \"\$(utils)/raptor2-$RAPTOR2V\" $RAPTOR2V ;" >> $OWLCPPBUILDDIR/owlcpp-$OWLCPPV/user-config_win32.jam
	echo " constant FACTPP : \"\$(utils)/FaCT++-$FACTPPV\" $FACTPPV ;" >> $OWLCPPBUILDDIR/owlcpp-$OWLCPPV/user-config_win32.jam

	if [ ! -f $OWLCPPBUILDDIR/boost_$BOOSTVU/tools/build/v2/b2 ]; then
		echo "Building boost.build"
		pushd $OWLCPPBUILDDIR/boost_$BOOSTVU/tools/build/v2 > /dev/null
		./bootstrap.sh > $OWLCPPBUILDDIR/output.out 2>&1
		if [ $? -gt 0 ]; then
			cat $OWLCPPBUILDDIR/output.out
			echo "Building boost.build failed; aborting"
			popd > /dev/null
			exit 1
		fi
		popd > /dev/null
		cp $OWLCPPBUILDDIR/boost_$BOOSTVU/tools/build/v2/b2 $OWLCPPBUILDDIR/owlcpp-$OWLCPPV/
	fi

	if [ ! -f $OWLCPPBUILDDIR/boost_$BOOSTVU/b2 ]; then
		echo "Bootstrapping boost"
		pushd $OWLCPPBUILDDIR/boost_$BOOSTVU > /dev/null
		./bootstrap.sh > $OWLCPPBUILDDIR/output.out 2>&1
		if [ $? -gt 0 ]; then
			cat $OWLCPPBUILDDIR/output.out
			echo "Building boost failed; aborting"
			popd > /dev/null
			exit 1
		fi
		echo "Building boost"
		./b2 "$params" > $OWLCPPBUILDDIR/output.out 2>&1
		if [ $? -gt 0 ]; then
			cat $OWLCPPBUILDDIR/output.out
			echo "Building boost failed; aborting"
			popd > /dev/null
			exit 1
		fi
		popd > /dev/null
	fi

	if [ ! -f $OWLCPPBUILDDIR/libxml2-$LIBXML2V/include/libxml/xmlversion.h ]; then
		echo "Building libxml2"
		pushd $OWLCPPBUILDDIR/libxml2-$LIBXML2V > /dev/null
		./autogen.sh > $OWLCPPBUILDDIR/output.out 2>&1
		if [ $? -gt 0 ]; then
			cat $OWLCPPBUILDDIR/output.out
			echo "Building libxml2 failed; aborting"
			popd > /dev/null
			exit 1
		fi
		popd > /dev/null
	fi

	echo "Building owlcpp"
	REALHOME=$HOME
	export HOME=$OWLCPPBUILDDIR/owlcpp-$OWLCPPV
	export BOOST_ROOT=$OWLCPPBUILDDIR/boost_$BOOSTVU
	export BOOST_BUILD_PATH=$OWLCPPBUILDDIR/boost_$BOOSTVU/tools/build/v2
	pushd $OWLCPPBUILDDIR/owlcpp-$OWLCPPV > /dev/null
	$BOOST_ROOT/tools/build/v2/b2 release cxxflags="-fpic -export-dynamic" cflags="-fpic -export-dynamic" "$params" > $OWLCPPBUILDDIR/output.out 2>&1
	ret=$?
	export HOME=$REALHOME
	if [ $ret -gt 0 ]; then
		cat $OWLCPPBUILDDIR/output.out
		echo "Building owlcpp failed; aborting"
		exit 1
	fi
fi

# scan $(OWLCPPBUILDDIR) for .a and header files and create symbolic links in $OWLCPPBUILDDIR/include and $OWLCPPBUILDDIR/libs
echo "Creating symbolic links to owlcpp"
mkdir $OWLCPPBUILDDIR/include 2> /dev/null
rm $OWLCPPBUILDDIR/include/*
ln -s $OWLCPP_ROOT/include/owlcpp $OWLCPPBUILDDIR/include/owlcpp
ln -s $OWLCPP_ROOT/out/include/factpp $OWLCPPBUILDDIR/include/factpp
ln -s $OWLCPP_ROOT/out/include/libxml $OWLCPPBUILDDIR/include/libxml
ln -s $OWLCPP_ROOT/out/include/raptor $OWLCPPBUILDDIR/include/raptor
mkdir $OWLCPPBUILDDIR/libs 2> /dev/null
rm $OWLCPPBUILDDIR/libs/*.a

if [ $(ls $OWLCPP_ROOT/out/bin/io/*/release/link-static/libowlcpp_io.a | wc -l) -eq 0 ]; then
	echo "Error: libowlcpp_io.a not found in $OWLCPPBUILDDIR/out/bin/io/*/release/link-static"
	exit 1
fi
ln -s $(ls $OWLCPP_ROOT/out/bin/io/*/release/link-static/libowlcpp_io.a | head) $OWLCPPBUILDDIR/libs/libowlcpp_io.a

if [ $(ls $OWLCPP_ROOT/out/bin/logic/*/release/link-static/libowlcpp_logic.a | wc -l) -eq 0 ]; then
        echo "Error: libowlcpp_logic.a not found in $OWLCPP_ROOT/out/bin/logic/*/release/link-static"
        exit 1
fi
ln -s $(ls $OWLCPP_ROOT/out/bin/logic/*/release/link-static/libowlcpp_logic.a | head) $OWLCPPBUILDDIR/libs/libowlcpp_logic.a

if [ $(ls $OWLCPP_ROOT/out/bin/rdf/*/release/link-static/libowlcpp_rdf.a | wc -l) -eq 0 ]; then
        echo "Error: libowlcpp_rdf.a not found in $OWLCPP_ROOT/out/bin/rdf/*/release/link-static"
        exit 1
fi
ln -s $(ls $OWLCPP_ROOT/out/bin/rdf/*/release/link-static/libowlcpp_rdf.a | head) $OWLCPPBUILDDIR/libs/libowlcpp_rdf.a

if [ $(ls $OWLCPP_ROOT/out/ext/factpp/factpp/*/release/link-static/libfactpp_kernel*.a | wc -l) -eq 0 ]; then
        echo "Error: libfactpp_kernel*.a not found in $OWLCPP_ROOT/out/ext/factpp/factpp/*/release/link-static/libfactpp_kernel*.a"
        exit 1
fi
ln -s $(ls $OWLCPP_ROOT/out/ext/factpp/factpp/*/release/link-static/libfactpp_kernel*.a | head) $OWLCPPBUILDDIR/libs/libfactpp_kernel.a

if [ $(ls $OWLCPP_ROOT/out/ext/libxml2/libxml2/*/release/libxml2-version-*/link-static/libxml2*.a | wc -l) -eq 0 ]; then
        echo "Error: libxml2*.a not found in $OWLCPP_ROOT/out/ext/libxml2/libxml2/*/release/libxml2-version-*/link-static/libxml2*.a"
        exit 1
fi
ln -s $(ls $OWLCPP_ROOT/out/ext/libxml2/libxml2/*/release/libxml2-version-*/link-static/libxml2*.a | head) $OWLCPPBUILDDIR/libs/libxml2.a

if [ $(ls $OWLCPP_ROOT/out/ext/raptor/raptor/*/release/link-static/raptor-*/libraptor*.a | wc -l) -eq 0 ]; then
        echo "Error: libraptor*.a not found in $OWLCPP_ROOT/out/ext/raptor/raptor/*/release/link-static/raptor-*/libraptor*.a"
        exit 1
fi
ln -s $(ls $OWLCPP_ROOT/out/ext/raptor/raptor/*/release/link-static/raptor-*/libraptor*.a | head) $OWLCPPBUILDDIR/libs/libraptor2.a

echo "This file just marks that owlcpp was successfully built. Remove it to rebuild." > $OWLCPPBUILDDIR/successfully_built

