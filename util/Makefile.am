## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = -D_FILE_OFFSET_BITS=64 
bin_PROGRAMS = gauth

gauth_SOURCES = gauth.cc \
                ../lib/json.cc \
                ../lib/request.cc \
                ../lib/dir_tree.cc \
                ../lib/common.cc \
                ../lib/request.h \
                ../lib/conf.h \
                ../lib/auth.h \
                ../lib/json.h
gauth_CPPFLAGS = -g -Wall --std=c++11 -I$(top_srcdir)/lib -fPIE
gauth_LDFLAGS = -lcurl -pie

EXTRA_DIST = init_script gdfs.conf gdfs.service

GDFS_PATH = @GDFS_PATH@
INIT_D_PATH = @INIT_D_PATH@

install-exec-local:
	$(MKDIR_P) $(DESTDIR)$(GDFS_PATH)
	$(INSTALL_SCRIPT) $(srcdir)/gdfs.conf $(DESTDIR)$(GDFS_PATH)/gdfs.conf
	$(INSTALL_SCRIPT) $(srcdir)/init_script $(DESTDIR)$(GDFS_PATH)/gdfs.sh

uninstall-local:
	rm -rf $(DESTDIR)$(GDFS_PATH)/gdfs.auth
	rm -rf $(DESTDIR)$(GDFS_PATH)/gdfs.log
	rm -rf $(DESTDIR)$(GDFS_PATH)
	rm -rf /var/log/gdfs.log
