# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/Users/sayon/Documents/codes/FOTA")
  file(MAKE_DIRECTORY "/Users/sayon/Documents/codes/FOTA")
endif()
file(MAKE_DIRECTORY
  "/Users/sayon/Documents/codes/FOTA/build/FOTA"
  "/Users/sayon/Documents/codes/FOTA/build/_sysbuild/sysbuild/images/FOTA-prefix"
  "/Users/sayon/Documents/codes/FOTA/build/_sysbuild/sysbuild/images/FOTA-prefix/tmp"
  "/Users/sayon/Documents/codes/FOTA/build/_sysbuild/sysbuild/images/FOTA-prefix/src/FOTA-stamp"
  "/Users/sayon/Documents/codes/FOTA/build/_sysbuild/sysbuild/images/FOTA-prefix/src"
  "/Users/sayon/Documents/codes/FOTA/build/_sysbuild/sysbuild/images/FOTA-prefix/src/FOTA-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/Users/sayon/Documents/codes/FOTA/build/_sysbuild/sysbuild/images/FOTA-prefix/src/FOTA-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/Users/sayon/Documents/codes/FOTA/build/_sysbuild/sysbuild/images/FOTA-prefix/src/FOTA-stamp${cfgdir}") # cfgdir has leading slash
endif()
