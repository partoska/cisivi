# Cisivi Library - Small, minimal CSV reader and writer.
# Copyright (C) 2026 Fabrika Charvat s.r.o. All rights reserved.
# Developed by Partoska Laboratory team, <https://lab.partoska.com>
#
# MIT License
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#
# You can contact the author(s) via email at ask <at> partoska.com.
#

# libcisivi: static by default, shared with -DBUILD_SHARED_LIBS=ON.

add_library(cisivi inc/cstypes.h inc/cisivi.h src/plat.h src/plat.c src/cisivi.c)
target_include_directories(cisivi PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
target_compile_options(cisivi PRIVATE ${PL_WARNINGS} ${PL_SANITIZERS_STRICT})
set_target_properties(cisivi PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    PUBLIC_HEADER "inc/cstypes.h;inc/cisivi.h"
    # A DLL exports nothing unless told; the API is functions only, so
    # exporting every symbol needs no dllexport in the headers.
    WINDOWS_EXPORT_ALL_SYMBOLS ON)

# On MSVC cisivi.lib and cisivi.pdb would clash with the cisivi.exe of the
# command line tool in the shared output directory.
set(PL_LIB_NAME cisivi)
if(MSVC)
    set(PL_LIB_NAME libcisivi)
endif()
set_target_properties(cisivi PROPERTIES OUTPUT_NAME ${PL_LIB_NAME})

# The prefix in cisivi.pc is relative to the installed file, so it stays right
# under cmake --install --prefix and in unpacked archives.
# The base only has to be absolute (/ is not on Windows).
file(RELATIVE_PATH PL_PC_PREFIX
    "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR}/pkgconfig" "${CMAKE_BINARY_DIR}")
string(REGEX REPLACE "/$" "" PL_PC_PREFIX "${PL_PC_PREFIX}")
configure_file(cisivi.pc.in "${CMAKE_CURRENT_BINARY_DIR}/cisivi.pc" @ONLY)

install(TARGETS cisivi
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/cisivi.pc"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
