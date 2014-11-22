OWLCPPMAINDIR=$(cd $(dirname $0)/owlcpp/; pwd)

echo "Downloading  owlcpp to $OWLCPPMAINDIR"
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

osx=$(uname -a | grep "Darwin" | wc -l)

if [ ! -f $OWLCPPMAINDIR/owlcpp-$OWLCPPV.zip ]
then
	echo "Downloading owlcpp source version $OWLCPPV"
	wget -O $OWLCPPMAINDIR/owlcpp-$OWLCPPV.zip http://downloads.sourceforge.net/project/owl-cpp/v$OWLCPPV/owlcpp-v$OWLCPPV.zip
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

