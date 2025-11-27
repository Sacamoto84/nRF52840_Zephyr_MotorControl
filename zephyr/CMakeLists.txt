#cmake_minimum_required(VERSION 3.13.1)
#include($ENV{ZEPHYR_BASE}/cmake/app/boilerplate.cmake NO_POLICY_SCOPE)
#project(N5280_Zephyr)

#FILE(GLOB app_sources ../src/*.c*)
#target_sources(app PRIVATE ${app_sources})

cmake_minimum_required(VERSION 3.13.1)

include($ENV{ZEPHYR_BASE}/cmake/app/boilerplate.cmake NO_POLICY_SCOPE)
project(N5280_Zephyr)

# Собрать все исходные файлы C и C++
FILE(GLOB app_sources ../src/*.c*)
target_sources(app PRIVATE ${app_sources})

# Настройки для C++
set_property(TARGET app PROPERTY CXX_STANDARD 17)
set_property(TARGET app PROPERTY CXX_STANDARD_REQUIRED ON)

# Флаги компиляции для C++ (применяются только к .cpp файлам)
target_compile_options(app PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-threadsafe-statics>
)