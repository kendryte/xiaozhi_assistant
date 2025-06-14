cmake_minimum_required(VERSION 3.12.4)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

if(NOT ESP_PLATFORM)
  if(NOT (CMAKE_C_COMPILER_ID STREQUAL "MSVC"))
    project(lvgl LANGUAGES C CXX ASM HOMEPAGE_URL https://github.com/lvgl/lvgl)
  else()
    project(lvgl LANGUAGES C CXX HOMEPAGE_URL https://github.com/lvgl/lvgl)
  endif()
endif()

set(LVGL_ROOT_DIR ${CMAKE_CURRENT_LIST_DIR})

set(FREETYPE_INCLUDE_DIRS "/home/wangyan01/k230_linux_sdk_0423/k230_linux_sdk/output/k230_canmv_01studio_defconfig/build/freetype-2.13.2/include/")
include_directories(${FREETYPE_INCLUDE_DIRS})
include_directories(${LVGL_ROOT_DIR})

if(ESP_PLATFORM)
  include(${CMAKE_CURRENT_LIST_DIR}/env_support/cmake/esp.cmake)
elseif(ZEPHYR_BASE)
  include(${CMAKE_CURRENT_LIST_DIR}/env_support/cmake/zephyr.cmake)
elseif(MICROPY_DIR)
  include(${CMAKE_CURRENT_LIST_DIR}/env_support/cmake/micropython.cmake)
else()
  include(${CMAKE_CURRENT_LIST_DIR}/env_support/cmake/custom.cmake)
endif()

#[[
    unfortunately CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS does not work for global data. 
    for global data we still need decl specs.
    Check out the docs to learn more about the limitations of CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS
    https://cmake.org/cmake/help/latest/prop_tgt/WINDOWS_EXPORT_ALL_SYMBOLS.html#prop_tgt:WINDOWS_EXPORT_ALL_SYMBOLS

    For all compiled sources within the library (i.e. basically all lvgl files) we need to use dllexport.
    For all compiled sources from outside the library (i.e. files which include lvgl headers) we need to use dllimport.
    We can do this by using CMakes INTERFACE and PRIVATE keyword.
  ]]
if (MSVC)
  target_compile_definitions(lvgl
    INTERFACE LV_ATTRIBUTE_EXTERN_DATA=__declspec\(dllimport\)
    PRIVATE LV_ATTRIBUTE_EXTERN_DATA=__declspec\(dllexport\)
  )
endif()
