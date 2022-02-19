#
# Copyright (c) 2022 Rieon Ke <i@ry.ke>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy of
# this software and associated documentation files (the "Software"), to deal in
# the Software without restriction, including without limitation the rights to
# use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
# the Software, and to permit persons to whom the Software is furnished to do so,
# subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
# FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
# COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
# IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
# CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#

cmake_minimum_required(VERSION 3.0)
project(beco C)

set(CMAKE_C_STANDARD 90)

option(ENABLE_TEST "Build test" ON)
option(ENABLE_EXAMPLES "Build examples" ON)

include_directories(3rd ${CMAKE_SOURCE_DIR})

add_library(beco STATIC beco.c 3rd/yyjson.c)
add_library(beco-mock STATIC mock.c)
target_link_options(beco-mock PUBLIC beco)

include(GNUInstallDirs)
install(TARGETS beco beco-mock
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(FILES beco.h mock.h DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/beco")

if (ENABLE_TEST)
    add_subdirectory(test)
endif ()

if (ENABLE_EXAMPLES)
    add_subdirectory(examples)
endif ()