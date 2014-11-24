echo "Bootstrapping (bootstrap.sh)"
./bootstrap.sh

echo "Prepare owlcpp for building"
export TOP_SRCDIR=`pwd`
cd owlcpp
../build_owlcpp.sh "" 155 extractonly
