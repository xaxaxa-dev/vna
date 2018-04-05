AM_CPPFLAGS = -O2 --std=c++0x -fPIC -fwrapv -fno-delete-null-pointer-checks -funsigned-char -fno-strict-aliasing -Wno-pmf-conversions
AM_LDFLAGS = -no-undefined -lpthread
lib_LTLIBRARIES = libxavna.la
libxavna_la_SOURCES = xavna.C xavna_cpp.C calibration.C

if WINDOWS
libxavna_la_SOURCES += platform_abstraction_windows.C
AM_CPPFLAGS += -DEIGEN_DONT_VECTORIZE -DEIGEN_DISABLE_UNALIGNED_ARRAY_ASSERT
else
libxavna_la_SOURCES += platform_abstraction.C
endif
