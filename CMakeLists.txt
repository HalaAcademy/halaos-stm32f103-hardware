# SPDX-FileCopyrightText: 2026 HALA Academy
# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.20)
project(HalaOS NONE)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
option(HALAOS_QUALIFICATION "Enable hardware qualification instrumentation" OFF)

file(READ "${CMAKE_SOURCE_DIR}/config/source_files.json" HALAOS_SOURCE_JSON)
string(JSON HALAOS_SOURCE_COUNT LENGTH "${HALAOS_SOURCE_JSON}" sources)
math(EXPR HALAOS_SOURCE_LAST "${HALAOS_SOURCE_COUNT} - 1")
set(HALAOS_SOURCE_DEPENDENCIES)
foreach(INDEX RANGE 0 ${HALAOS_SOURCE_LAST})
  string(JSON SOURCE_FILE GET "${HALAOS_SOURCE_JSON}" sources ${INDEX})
  list(APPEND HALAOS_SOURCE_DEPENDENCIES "${CMAKE_SOURCE_DIR}/${SOURCE_FILE}")
endforeach()

if(HALAOS_QUALIFICATION)
  set(HALAOS_PROFILE qualification)
  set(HALAOS_BASENAME halaos-bluepill-qualification)
else()
  set(HALAOS_PROFILE hardware)
  set(HALAOS_BASENAME halaos-bluepill)
endif()

set(HALAOS_ELF "${CMAKE_BINARY_DIR}/${HALAOS_BASENAME}.elf")
set(HALAOS_BIN "${CMAKE_BINARY_DIR}/${HALAOS_BASENAME}.bin")
set(HALAOS_MAP "${CMAKE_BINARY_DIR}/${HALAOS_BASENAME}.map")

add_custom_command(
  OUTPUT "${HALAOS_ELF}" "${HALAOS_BIN}" "${HALAOS_MAP}"
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/build.py"
          --profile "${HALAOS_PROFILE}" --out "${HALAOS_ELF}"
  DEPENDS
    "${CMAKE_SOURCE_DIR}/tools/build.py"
    "${CMAKE_SOURCE_DIR}/config/source_files.json"
    "${CMAKE_SOURCE_DIR}/config/memory/stm32f103c8.ld"
    ${HALAOS_SOURCE_DEPENDENCIES}
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  VERBATIM
)

add_custom_target(halaos-bluepill ALL DEPENDS "${HALAOS_ELF}" "${HALAOS_BIN}" "${HALAOS_MAP}")
