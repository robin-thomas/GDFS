## lib/Makefile.am

AM_CXXFLAGS = -g --std=c++11 -D_FILE_OFFSET_BITS=64 -I$(top_srcdir)/lib

noinst_LTLIBRARIES = libjson.la librequest.la libauth.la libgdfs.la libgdapi.la

libjson_la_SOURCES = json.cc \
                     json.h \
                     exception.h

librequest_la_SOURCES = request.cc \
                        request.h \
                        common.cc \
                        common.h

libauth_la_SOURCES = auth.cc \
                     auth.h

libgdfs_la_SOURCES = gdfs.cc \
                     auth.h \
                     gdfs.h \
                     gdapi.h \
                     threadpool.h \
                     conf.h \
                     log.h \
                     common.h \
                     fuse/fuse.h
libgdfs_la_CPPFLAGS = -I$(srcdir)/fuse

libgdapi_la_SOURCES = gdapi.cc \
                      log.cc \
                      auth.cc \
                      dir_tree.cc \
                      cache.cc \
                      threadpool.cc \
                      common.cc \
                      gdapi.h \
                      auth.h \
                      dir_tree.h \
                      cache.h \
                      common.h \
                      threadpool.h
