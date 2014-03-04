
	echo "Bootstrapping boost"
	cd owlcpp\boost_1_55_0
	bootstrap.bat
	cd ..\..

	echo "Building owlcpp (including other necessary libraries"
	cd owlcpp\owlcpp-v0.3.3
	../boost_1_55_0\tools\build\v2\b2.exe release
	cd ..\..

	echo "Extracting files"
	cd owlcpp
	mkdir libs

	echo "   owlcpp libs"
	copy owlcpp-v0.3.3\out\bin\io\msvc-10.0\release\link-static\threading-multi\owlcpp_io.lib libs\
	copy owlcpp-v0.3.3\out\bin\logic\msvc-10.0\release\link-static\threading-multi\owlcpp_logic.lib libs\
	copy owlcpp-v0.3.3\out\bin\rdf\msvc-10.0\release\link-static\threading-multi\owlcpp_rdf.lib libs\
	echo "   factpp libs"
	copy owlcpp-v0.3.3\out\ext\factpp\factpp\msvc-10.0\release\link-static\threading-multi\libfactpp_kernel-vc100-mt.lib libs\
	echo "   libxml2 libs"
	copy owlcpp-v0.3.3\out\ext\libxml2\libxml2\msvc-10.0\release\libxml2-version-2.9.0\link-static\threading-multi\libxml2-vc100-mt-2_9.lib libs\
	echo "   libraptor libs"
	copy owlcpp-v0.3.3\out\ext\raptor\raptor\msvc-10.0\release\link-static\raptor-version-2.0.8\threading-multi\libraptor-vc100-mt-2_0.lib libs\

	echo "   owlcpp headers"
	mkdir include
	mkdir include\owlcpp
	mkdir include\owlcpp\detail
	mkdir include\owlcpp\io
	mkdir include\owlcpp\io\detail
	mkdir include\owlcpp\logic
	mkdir include\owlcpp\logic\detail
	mkdir include\owlcpp\rdf
	mkdir include\owlcpp\rdf\detail
	mkdir include\owlcpp\terms
	mkdir include\owlcpp\terms\detail
	copy owlcpp-v0.3.3\include\owlcpp\*.h include\owlcpp\
	copy owlcpp-v0.3.3\include\owlcpp\detail\*.h include\owlcpp\detail\
	copy owlcpp-v0.3.3\include\owlcpp\io\*.h include\owlcpp\io\
	copy owlcpp-v0.3.3\include\owlcpp\io\detail\*.h include\owlcpp\io\detail\
	copy owlcpp-v0.3.3\include\owlcpp\logic\*.h include\owlcpp\logic\
	copy owlcpp-v0.3.3\include\owlcpp\logic\detail\*.h include\owlcpp\logic\detail\
	copy owlcpp-v0.3.3\include\owlcpp\rdf\*.h include\owlcpp\rdf\
	copy owlcpp-v0.3.3\include\owlcpp\rdf\detail\*.h include\owlcpp\rdf\detail\
	copy owlcpp-v0.3.3\include\owlcpp\terms\*.h include\owlcpp\terms\
	copy owlcpp-v0.3.3\include\owlcpp\terms\detail\*.h include\owlcpp\terms\detail\
	echo "   factpp headers"
	mkdir include\factpp
	copy owlcpp-v0.3.3\out\include\factpp\*.h include\owlcpp\factpp\
	echo "   libxml2 headers"
	mkdir include\libxml
	copy owlcpp-v0.3.3\out\include\libxml\*.h include\owlcpp\libxml\
	echo "   raptor headers"
	mkdir include\raptor
	copy owlcpp-v0.3.3\out\include\raptor\*.h include\owlcpp\raptor\
	
