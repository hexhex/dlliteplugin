#
# replace 'plugin' on the right side by your plugin's name
#
plugin_LTLIBRARIES = libdlvhexplugin-dllite.la

#
# replace 'plugin' on the left side as above and
# add all sources of your plugin
#
libdlvhexplugin_dllite_la_SOURCES = DLLitePlugin.cpp ExternalAtoms.cpp DLRewriter.cpp RepairModelGenerator.cpp

#
# extend compiler flags by CFLAGS of other needed libraries
#
AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	$(BOOST_CPPFLAGS) \
	$(DLVHEX_CFLAGS) \
	$(EXTSOLVER_CPPFLAGS)

# for more information on `-version-info' see
# <http://www.gnu.org/software/libtool/manual.html#Versioning>, and
# the USENIX paper `Library Interface Versioning in Solaris and Linux'
# <http://www.usenix.org/publications/library/proceedings/als00/2000papers/papers/full_papers/browndavid/browndavid.pdf>
libdlvhexplugin_dllite_la_LDFLAGS = -version-info 2:0:0

libdlvhexplugin_dllite_la_LDFLAGS = -avoid-version -module $(EXTSOLVER_LDFLAGS) $(BOOST_FILESYSTEM_LDFLAGS)

libdlvhexplugin_dllite_la_LIBADD = $(EXTSOLVER_LIBADD) $(BOOST_FILESYSTEM_LIBS)


libdlvhexplugin-dllite-static.la: $(libdlvhexplugin_dllite_la_OBJECTS)
	$(CXXLINK) -avoid-version -module -rpath $(plugindir) $(libdlvhexplugin_dllite_la_OBJECTS)

install-static: libdlvhexplugin-dllite-static.la
	$(LIBTOOL) --mode=install $(INSTALL) -s libdlvhexplugin-dllite-static.la $(DESTDIR)$(plugindir)/libdlvhexplugin-dllite-static.la

install-data-hook:
	cp -r ../requiem $(DESTDIR)$(plugindir)

uninstall-static:
	$(LIBTOOL) --mode=uninstall rm -f $(DESTDIR)$(plugindir)/libdlvhexplugin-dllite-static.la

uninstall-hook:
	rm -r $(DESTDIR)$(plugindir)/requiem

