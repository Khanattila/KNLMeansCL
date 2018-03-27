
#----------------------------------------------------------------------------
#    This file is part of KNLMeansCL,
#    Copyright(C) 2014-2018 Edoardo Brunetti,
#    Copyright(C) 2016      sl1pkn07,
#    Copyright(C) 2018      ldepandis.
#    
#    KNLMeansCL is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.
#
#    KNLMeansCL is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with KNLMeansCL. If not, see <http://www.gnu.org/licenses/>.
#----------------------------------------------------------------------------

include config.mak

SRCS = KNLMeansCL/NLMKernel.cpp \
       KNLMeansCL/NLMAvisynth.cpp \
       KNLMeansCL/NLMVapoursynth.cpp \
       KNLMeansCL/shared/common.cpp \
       KNLMeansCL/shared/ocl_utils.cpp \
       KNLMeansCL/shared/startchar.cpp

OBJS = KNLMeansCL/NLMKernel.o \
       KNLMeansCL/NLMAvisynth.o \
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
