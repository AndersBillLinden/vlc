neondir = $(pluginsdir)/arm_neon

libchroma_yuv_neon_plugin_la_SOURCES = \
	isa/arm/neon/deinterleave_chroma.S \
	isa/arm/neon/i420_yuyv.S \
	isa/arm/neon/i422_yuyv.S \
	isa/arm/neon/yuyv_i422.S \
	isa/arm/neon/chroma_yuv.c isa/arm/neon/chroma_neon.h
libchroma_yuv_neon_plugin_la_CFLAGS = $(AM_CFLAGS)
libchroma_yuv_neon_plugin_LIBTOOLFLAGS = --tag=CC

libvolume_neon_plugin_la_SOURCES = isa/arm/neon/volume.c isa/arm/neon/amplify.S
libvolume_neon_plugin_la_CFLAGS = $(AM_CFLAGS)
libvolume_neon_plugin_LIBTOOLFLAGS = --tag=CC

libyuv_rgb_neon_plugin_la_SOURCES = \
	isa/arm/neon/i420_rgb.S \
	isa/arm/neon/i420_rv16.S \
	isa/arm/neon/nv21_rgb.S \
	isa/arm/neon/nv12_rgb.S \
	isa/arm/neon/yuv_rgb.c
libyuv_rgb_neon_plugin_la_CFLAGS = $(AM_CFLAGS)
libyuv_rgb_neon_plugin_LIBTOOLFLAGS = --tag=CC

if HAVE_NEON
neon_LTLIBRARIES = \
	libchroma_yuv_neon_plugin.la \
	libvolume_neon_plugin.la \
	libyuv_rgb_neon_plugin.la
endif

noinst_HEADERS += isa/arm/asm.S
