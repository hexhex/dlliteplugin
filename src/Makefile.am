#
# replace 'plugin' on the right side by your plugin's name
#
plugin_LTLIBRARIES = libdlvhexplugin-dllite.la

#
# replace 'plugin' on the left side as above and
# add all sources of your plugin
#
libdlvhexplugin_dllite_la_SOURCES = DLLitePlugin.cpp

#
# extend compiler flags by CFLAGS of other needed libraries
#
AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	$(BOOST_CPPFLAGS) \
	$(DLVHEX_CFLAGS)

libdlvhexplugin_dllite_la_LDFLAGS = -avoid-version -module


libdlvhexplugin-dllite-static.la: $(libdlvhexplugin_dllite_la_OBJECTS)
	$(CXXLINK) -avoid-version -module -rpath $(plugindir) $(libdlvhexplugin_dllite_la_OBJECTS)

install-static: libdlvhexplugin-dllite-static.la
	$(LIBTOOL) --mode=install $(INSTALL) -s libdlvhexplugin-dllite-static.la $(DESTDIR)$(plugindir)/libdlvhexplugin-dllite-static.la

uninstall-static:
	$(LIBTOOL) --mode=uninstall rm -f $(DESTDIR)$(plugindir)/libdlvhexplugin-dllite-static.la
