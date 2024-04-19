AM_CFLAGS = -Wall -g \
        -Wundef \
        -Wstrict-prototypes \
        -Wno-trigraphs

ACLOCAL_AMFLAGS = -I m4

AM_CPPFLAGS = -D__packed__=

mmCellularRefApp_c_sources = mm-cellular-ref-app.c

requiredlibs = -lglib-2.0 -lgio-2.0 -lmm-glib -lgobject-2.0

bin_PROGRAMS = mmCellularRefApp
mmc_bin_CC = @CC@
pkgconfigdir = $(libdir)/pkgconfig
mmCellularRefApp_SOURCES = $(mmCellularRefApp_c_sources)
mmCellularRefApp_CFLAGS  = -Dstrlcpy=strncpy -Dstrlcat=strncat $(AM_CFLAGS)
mmCellularRefApp_LDFLAGS = -lpthread -shared -version-info 1:0:0
mmCellularRefApp_LDADD = $(requiredlibs)
mmCellularRefApp_CPPFLAGS = -Dstrlcpy=strncpy -Dstrlcat=strncat $(AM_CPPFLAGS)
