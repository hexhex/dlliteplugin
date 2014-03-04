OWLCPPMAINDIR=$(cd $(dirname $0)/owlcpp/; pwd)

echo "Downloading  owlcpp to $OWLCPPMAINDIR"
LIBXML2V=2.9.0
RAPTOR2V=2.0.8
FACTPPV=1.6.2
OWLCPPV=0.3.3
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

osx=$(uname -a | grep "Darwin" | wc -l)

if [ ! -f $OWLCPPMAINDIR/owlcpp-v$OWLCPPV.zip ]
then
	echo "Downloading owlcpp source version $OWLCPPV"
	wget -O $OWLCPPMAINDIR/owlcpp-v$OWLCPPV.zip http://downloads.sourceforge.net/project/owl-cpp/v0.3.3/owlcpp-v$OWLCPPV.zip
	if [ $? -gt 0 ]
	then
		echo "Error while downloading owlcpp; aborting"
		exit 1
	fi
fi

BOOSTVU=$(echo $BOOSTV | sed 's/\./_/g')
if [ ! -f $OWLCPPMAINDIR/boost-$BOOSTV.tar.gz ]
then
	echo "Downloading boost source version $BOOSTV"
	wget -O $OWLCPPMAINDIR/boost-$BOOSTV.tar.gz http://downloads.sourceforge.net/project/boost/boost/$BOOSTV/boost_$BOOSTVU.tar.gz
	if [ $? -gt 0 ]
	then
		echo "Error while downloading boost; aborting"
		exit 1
	fi
fi

if [ ! -f $OWLCPPMAINDIR/libxml2-$LIBXML2V.zip ]
then
	echo "Downloading libxml2 source version $LIBXML2V"
	if [ $osx == 1 ]; then
		# OS X does not trust this server
		wget --no-check-certificate -O $OWLCPPMAINDIR/libxml2-$LIBXML2V.zip https://git.gnome.org/browse/libxml2/snapshot/libxml2-$LIBXML2V.zip
	else
		wget -O $OWLCPPMAINDIR/libxml2-$LIBXML2V.zip https://git.gnome.org/browse/libxml2/snapshot/libxml2-$LIBXML2V.zip
	fi
	if [ $? -gt 0 ]
	then
		echo "Error while downloading libxml2; aborting"
		exit 1
	fi
fi

if [ ! -f $OWLCPPMAINDIR/raptor2-$RAPTOR2V.tar.gz ]
then
	echo "Downloading raptor2 source version $RAPTOR2V"
	wget -O $OWLCPPMAINDIR/raptor2-$RAPTOR2V.tar.gz http://download.librdf.org/source/raptor2-$RAPTOR2V.tar.gz
	if [ $? -gt 0 ]
	then
		echo "Error while downloading raptor2; aborting"
		exit 1
	fi
fi

if [ ! -f $OWLCPPMAINDIR/FaCTpp-src-v$FACTPPV.tar.gz ]
then
	echo "Downloading FaCT++ source version $FACTPPV"
	wget -O $OWLCPPMAINDIR/FaCTpp-src-v$FACTPPV.tar.gz http://factplusplus.googlecode.com/files/FaCTpp-src-v$FACTPPV.tgz
	if [ $? -gt 0 ]
	then
		echo "Error while downloading FaCT++; aborting"
		exit 1
	fi
fi

if [ ! -f $OWLCPPMAINDIR/libiconv-$ICONVV-dev.zip ] || [ ! -f $OWLCPPMAINDIR/libiconv-$ICONVV-bin.zip ]
then
	echo "Downloading libiconv source version $ICONVV"
	wget -O $OWLCPPMAINDIR/libiconv-$ICONVV-dev.zip http://downloads.sourceforge.net/project/gnuwin32/libiconv/$ICONVV-1/libiconv-$ICONVV-1-lib.zip
	wget -O $OWLCPPMAINDIR/libiconv-$ICONVV-bin.zip http://downloads.sourceforge.net/project/gnuwin32/libiconv/$ICONVV-1/libiconv-$ICONVV-1-bin.zip
	if [ $? -gt 0 ]
	then
		echo "Error while downloading libiconv; aborting"
		exit 1
	fi
fi

echo "Extracting archives"
cd $OWLCPPMAINDIR
if [ ! -d $OWLCPPMAINDIR/owlcpp-v$OWLCPPV ]; then
	unzip $OWLCPPMAINDIR/owlcpp-v$OWLCPPV.zip > /dev/null 2> /dev/null
fi
OWLCPP_ROOT=$OWLCPPMAINDIR/owlcpp-v$OWLCPPV
if [ ! -d $OWLCPPMAINDIR/boost_$BOOSTVU ]; then
	tar -xzf $OWLCPPMAINDIR/boost-$BOOSTV.tar.gz > /dev/null 2> /dev/null
fi
if [ ! -d $OWLCPPMAINDIR/libxml2-$LIBXML2V ]; then
	unzip $OWLCPPMAINDIR/libxml2-$LIBXML2V.zip > /dev/null 2> /dev/null
fi
if [ ! -d $OWLCPPMAINDIR/raptor2-$RAPTOR2V ]; then
	tar -xzf $OWLCPPMAINDIR/raptor2-$RAPTOR2V.tar.gz > /dev/null 2> /dev/null
fi
if [ ! -d $OWLCPPMAINDIR/FaCT++-$FACTPPV ]; then
	tar -xzf $OWLCPPMAINDIR/FaCTpp-src-v$FACTPPV.tar.gz > /dev/null 2> /dev/null
fi
if [ ! -d $OWLCPPMAINDIR/libiconv-$ICONVV ]; then
	unzip -d $OWLCPPMAINDIR/libiconv-$ICONVV $OWLCPPMAINDIR/libiconv-$ICONVV-dev.zip > /dev/null 2> /dev/null
	unzip -d $OWLCPPMAINDIR/libiconv-$ICONVV $OWLCPPMAINDIR/libiconv-$ICONVV-bin.zip > /dev/null 2> /dev/null

	# some files have the wrong names
	cp $OWLCPPMAINDIR/libiconv-$ICONVV/bin/libiconv2.dll $OWLCPPMAINDIR/libiconv-$ICONVV/bin/iconv.dll
	cp $OWLCPPMAINDIR/libiconv-$ICONVV/lib/libiconv.lib $OWLCPPMAINDIR/libiconv-$ICONVV/lib/iconv.lib
	cp $OWLCPPMAINDIR/libiconv-$ICONVV/lib/libiconv.lib $OWLCPPMAINDIR/libiconv-$ICONVV/lib/iconv_a.lib
fi

echo "Bootstrapping libxml2"
pushd libxml2-$LIBXML2V
./autogen.sh
popd

echo "Generating user-config.jam"
cd owlcpp-v$OWLCPPV
cp doc/user-config.jam user-config.jam
echo " constant BOOST : \"$OWLCPPMAINDIR/boost_$BOOSTVU/\" $BOOSTV ;" >> user-config.jam
echo " constant LIBXML2 : \"$OWLCPPMAINDIR/libxml2-$LIBXML2V\" $LIBXML2V ;" >> user-config.jam
echo " constant RAPTOR : \"$OWLCPPMAINDIR/raptor2-$RAPTOR2V\" $RAPTOR2V ;" >> user-config.jam
echo " constant FACTPP : \"$OWLCPPMAINDIR/FaCT++-$FACTPPV\" $FACTPPV ;" >> user-config.jam
cd ..

echo "Updating jam files"
cd owlcpp-v$OWLCPPV
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
	cp $OWLCPPMAINDIR/libxml2-$LIBXML2V/autogen.sh $OWLCPPMAINDIR/libxml2-$LIBXML2V/autogen.sh.bak
	cat $OWLCPPMAINDIR/libxml2-$LIBXML2V/autogen.sh | sed 's/libtoolize --version/echo 0/' > $OWLCPPMAINDIR/libxml2-$LIBXML2V/autogen.sh.bak
	mv $OWLCPPMAINDIR/libxml2-$LIBXML2V/autogen.sh.bak $OWLCPPMAINDIR/libxml2-$LIBXML2V/autogen.sh
fi
cd ..

echo "	echo off
	set OWLCPPMAIN=%CD%\\\\owlcpp

	echo \"Bootstrapping boost build\"
	cd %OWLCPPMAIN%\\\\boost_$BOOSTVU\\\\tools\\\\build\\\\v2
	call bootstrap.bat

	echo \"Building owlcpp (including other necessary libraries\"
	cd %OWLCPPMAIN%\\\\owlcpp-v$OWLCPPV
	copy lib\\io\\jamfile_win32.jam lib\\io\\jamfile.jam
	set OWLCPPMAIN_ESC=%OWLCPPMAIN:\\\\=/%
	echo local utils = \"%OWLCPPMAIN_ESC%\" ; > %USERPROFILE%\\\\user-config.jam
	type user-config_win32.jam >> %USERPROFILE%\\\\user-config.jam
	set BOOST=%OWLCPPMAIN%\\\\boost_$BOOSTVU
	set BOOST_ROOT=%OWLCPPMAIN%\\\\boost_$BOOSTVU
	set BOOST_BUILD_PATH=%OWLCPPMAIN%\\\\boost_$BOOSTVU\\\\tools\\\\build\\\\v2
	set BOOST_BUILD_PATH_ESC=%BOOST_BUILD_PATH:\\\\=/%
	echo boost-build \"%BOOST_BUILD_PATH_ESC%\" ; > boost-build.jam
	call %OWLCPPMAIN%\\\\boost_$BOOSTVU\\\\tools\\\\build\\\\v2\\\\b2.exe release
	del %USERPROFILE%\\\\user-config.jam

	echo \"Extracting files\"
	cd %OWLCPPMAIN%
	mkdir libs

	echo \"   owlcpp libs\"
	copy owlcpp-v$OWLCPPV\\\\out\\\\bin\\\\io\\\\msvc-10.0\\\\release\\\\link-static\\\\threading-multi\\\\owlcpp_io.lib libs\\\\
	copy owlcpp-v$OWLCPPV\\\\out\\\\bin\\\\logic\\\\msvc-10.0\\\\release\\\\link-static\\\\threading-multi\\\\owlcpp_logic.lib libs\\\\
	copy owlcpp-v$OWLCPPV\\\\out\\\\bin\\\\rdf\\\\msvc-10.0\\\\release\\\\link-static\\\\threading-multi\\\\owlcpp_rdf.lib libs\\\\
	echo \"   factpp libs\"
	copy owlcpp-v$OWLCPPV\\\\out\\\\ext\\\\factpp\\\\factpp\\\\msvc-10.0\\\\release\\\\link-static\\\\threading-multi\\\\libfactpp_kernel-vc100-mt.lib libs\\\\
	echo \"   libxml2 libs\"
	copy owlcpp-v$OWLCPPV\\\\out\\\\ext\\\\libxml2\\\\libxml2\\\\msvc-10.0\\\\release\\\\libxml2-version-2.9.0\\\\link-static\\\\threading-multi\\\\libxml2-vc100-mt-2_9.lib libs\\\\
	echo \"   libraptor libs\"
	copy owlcpp-v$OWLCPPV\\\\out\\\\ext\\\\raptor\\\\raptor\\\\msvc-10.0\\\\release\\\\link-static\\\\raptor-version-2.0.8\\\\threading-multi\\\\libraptor-vc100-mt-2_0.lib libs\\\\

	echo \"   owlcpp headers\"
	mkdir include
	mkdir include\\\\owlcpp
	mkdir include\\\\owlcpp\\\\detail
	mkdir include\\\\owlcpp\\\\io
	mkdir include\\\\owlcpp\\\\io\\\\detail
	mkdir include\\\\owlcpp\\\\logic
	mkdir include\\\\owlcpp\\\\logic\\\\detail
	mkdir include\\\\owlcpp\\\\rdf
	mkdir include\\\\owlcpp\\\\rdf\\\\detail
	mkdir include\\\\owlcpp\\\\terms
	mkdir include\\\\owlcpp\\\\terms\\\\detail
	copy owlcpp-v$OWLCPPV\\\\include\\\\owlcpp\\\\*.h include\\\\owlcpp\\\\
	copy owlcpp-v$OWLCPPV\\\\include\\\\owlcpp\\\\detail\\\\*.h include\\\\owlcpp\\\\detail\\\\
	copy owlcpp-v$OWLCPPV\\\\include\\\\owlcpp\\\\io\\\\*.h include\\\\owlcpp\\\\io\\\\
	copy owlcpp-v$OWLCPPV\\\\include\\\\owlcpp\\\\io\\\\detail\\\\*.h include\\\\owlcpp\\\\io\\\\detail\\\\
	copy owlcpp-v$OWLCPPV\\\\include\\\\owlcpp\\\\logic\\\\*.h include\\\\owlcpp\\\\logic\\\\
	copy owlcpp-v$OWLCPPV\\\\include\\\\owlcpp\\\\logic\\\\detail\\\\*.h include\\\\owlcpp\\\\logic\\\\detail\\\\
	copy owlcpp-v$OWLCPPV\\\\include\\\\owlcpp\\\\rdf\\\\*.h include\\\\owlcpp\\\\rdf\\\\
	copy owlcpp-v$OWLCPPV\\\\include\\\\owlcpp\\\\rdf\\\\detail\\\\*.h include\\\\owlcpp\\\\rdf\\\\detail\\\\
	copy owlcpp-v$OWLCPPV\\\\include\\\\owlcpp\\\\terms\\\\*.h include\\\\owlcpp\\\\terms\\\\
	copy owlcpp-v$OWLCPPV\\\\include\\\\owlcpp\\\\terms\\\\detail\\\\*.h include\\\\owlcpp\\\\terms\\\\detail\\\\
	echo \"   factpp headers\"
	mkdir include\\\\factpp
	copy owlcpp-v$OWLCPPV\\\\out\\\\include\\\\factpp\\\\*.h include\\\\owlcpp\\\\factpp\\\\
	echo \"   libxml2 headers\"
	mkdir include\\\\libxml
	copy owlcpp-v$OWLCPPV\\\\out\\\\include\\\\libxml\\\\*.h include\\\\owlcpp\\\\libxml\\\\
	echo \"   raptor headers\"
	mkdir include\\\\raptor
	copy owlcpp-v$OWLCPPV\\\\out\\\\include\\\\raptor\\\\*.h include\\\\owlcpp\\\\raptor\\\\
	" > build_owlcpp.bat

echo "Fixing jam-file"
echo "	lib owlcpp_io
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
	;" > $OWLCPPMAINDIR/owlcpp-v$OWLCPPV/lib/io/jamfile_win32.jam

echo "Writing user-config.jam"
cp $OWLCPPMAINDIR/owlcpp-v$OWLCPPV/doc/user-config.jam $OWLCPPMAINDIR/owlcpp-v$OWLCPPV/
echo " constant BOOST : \"\$(utils)/boost_$BOOSTVU/\" $BOOSTV ;" > $OWLCPPMAINDIR/owlcpp-v$OWLCPPV/user-config_win32.jam
echo " constant ICONV : \"\$(utils)/libiconv-$ICONVV\" $ICONVV ;" >> $OWLCPPMAINDIR/owlcpp-v$OWLCPPV/user-config_win32.jam
echo " constant LIBXML2 : \"\$(utils)/libxml2-$LIBXML2V\" $LIBXML2V ;" >> $OWLCPPMAINDIR/owlcpp-v$OWLCPPV/user-config_win32.jam
echo " constant RAPTOR : \"\$(utils)/raptor2-$RAPTOR2V\" $RAPTOR2V ;" >> $OWLCPPMAINDIR/owlcpp-v$OWLCPPV/user-config_win32.jam
echo " constant FACTPP : \"\$(utils)/FaCT++-$FACTPPV\" $FACTPPV ;" >> $OWLCPPMAINDIR/owlcpp-v$OWLCPPV/user-config_win32.jam

