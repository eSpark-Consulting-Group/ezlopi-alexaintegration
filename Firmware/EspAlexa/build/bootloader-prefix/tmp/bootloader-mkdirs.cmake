# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "C:/Users/eSpark/esp/esp-idf/components/bootloader/subproject"
  "C:/Users/eSpark/EspAlexa/build/bootloader"
  "C:/Users/eSpark/EspAlexa/build/bootloader-prefix"
  "C:/Users/eSpark/EspAlexa/build/bootloader-prefix/tmp"
  "C:/Users/eSpark/EspAlexa/build/bootloader-prefix/src/bootloader-stamp"
  "C:/Users/eSpark/EspAlexa/build/bootloader-prefix/src"
  "C:/Users/eSpark/EspAlexa/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/Users/eSpark/EspAlexa/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
