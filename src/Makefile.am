## src/Makefile.am

bin_PROGRAMS  = mount.gdfs

mount_gdfs_SOURCES = main.cc \
                     mount.cc \
                     main.h \
                     mount.h \
                     ../lib/log.h \
                     ../lib/conf.h \
                     ../lib/fuse/fuse_common.h \
                     ../lib/fuse/fuse_opt.h

mount_gdfs_CPPFLAGS = -g -Wall --std=c++11 -I$(top_srcdir)/lib -I$(top_srcdir)/lib/fuse -D_FILE_OFFSET_BITS=64
mount_gdfs_LDFLAGS = -L$(top_srcdir)/lib -L/usr/lib64/
mount_gdfs_LDADD = -ldl -lcurl -lfuse -lpthread -lauth -lgdfs -lgdapi -ljson -lrequest
