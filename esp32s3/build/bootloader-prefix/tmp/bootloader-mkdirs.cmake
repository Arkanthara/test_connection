# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "C:/Espressif/frameworks/esp-idf-v4.4-2/components/bootloader/subproject"
  "C:/Users/midonnet/Documents/test_connection/esp32s3/build/bootloader"
  "C:/Users/midonnet/Documents/test_connection/esp32s3/build/bootloader-prefix"
  "C:/Users/midonnet/Documents/test_connection/esp32s3/build/bootloader-prefix/tmp"
  "C:/Users/midonnet/Documents/test_connection/esp32s3/build/bootloader-prefix/src/bootloader-stamp"
  "C:/Users/midonnet/Documents/test_connection/esp32s3/build/bootloader-prefix/src"
  "C:/Users/midonnet/Documents/test_connection/esp32s3/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/Users/midonnet/Documents/test_connection/esp32s3/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
