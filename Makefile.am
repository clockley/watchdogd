ACLOCAL_AMFLAGS = -I m4
SUBDIRS = src conf contrib man src/threadpool
dist_doc_DATA = README LICENSE INSTALL MAN.TXT

DISTCHECK_CONFIGURE_FLAGS = \
                   --with-systemdsystemunitdir=$$dc_install_base/$(systemdsystemunitdir)

