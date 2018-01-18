#----------------------------------------------------------------------------
#  Makefile for KNLmeansCL
#  Author: sl1pkn07
#----------------------------------------------------------------------------

include config.mak

SRCS = KNLMeansCL/NLMKernel.cpp \
       KNLMeansCL/NLMVapoursynth.cpp \
       KNLMeansCL/shared/common.cpp \
       KNLMeansCL/shared/ocl_utils.cpp \
       KNLMeansCL/shared/startchar.cpp

OBJS = KNLMeansCL/NLMKernel.o \
       KNLMeansCL/NLMVapoursynth.o \
       KNLMeansCL/shared/common.o \
       KNLMeansCL/shared/ocl_utils.o \
       KNLMeansCL/shared/startchar.o

.PHONY: all install clean distclean

all:
	$(LD) -o $(LIBNAME) $(LDFLAGS) $(CXXFLAGS) $(LIBS) $(SRCS)
	$(STRIP) $(LIBNAME)

install: all
	install -d $(DESTDIR)$(libdir)
	install -m 755 $(LIBNAME) $(DESTDIR)$(libdir)

clean:
	$(RM) *.dll *.so *.dylib $(OBJS)

distclean: clean
	$(RM) config.*
