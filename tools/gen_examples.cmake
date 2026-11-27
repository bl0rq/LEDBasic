# Generates include/BasicExamples/<Name>.h from examples/bas/<Name>.bas.
#
# The .bas files are the source of truth. The headers are checked in so the
# library builds anywhere (PlatformIO, Arduino, any C++11 compiler) without
# this script. The host CMake build includes this file at configure time and
# re-runs it when a .bas file changes. To refresh by hand:
#
#   cmake -P tools/gen_examples.cmake
#
# A header is only rewritten when its contents change.

get_filename_component(_gen_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(_gen_src "${_gen_root}/examples/bas")
set(_gen_dst "${_gen_root}/include/BasicExamples")

file(GLOB _gen_bas_files "${_gen_src}/*.bas")
list(SORT _gen_bas_files)

foreach(_gen_bas IN LISTS _gen_bas_files)
    get_filename_component(_gen_name "${_gen_bas}" NAME_WE)
    file(READ "${_gen_bas}" _gen_body)
    string(REPLACE "\r\n" "\n" _gen_body "${_gen_body}")
    string(FIND "${_gen_body}" ")BAS\"" _gen_clash)
    if(NOT _gen_clash EQUAL -1)
        message(FATAL_ERROR "${_gen_bas} contains )BAS\" and cannot be embedded in a raw string")
    endif()

    set(_gen_text "// Generated from examples/bas/${_gen_name}.bas by tools/gen_examples.cmake. Do not edit.\n")
    string(APPEND _gen_text "#pragma once\n\n")
    string(APPEND _gen_text "namespace ${_gen_name} {\n")
    string(APPEND _gen_text "constexpr char program[] = R\"BAS(${_gen_body})BAS\";\n")
    string(APPEND _gen_text "}\n")

    set(_gen_out "${_gen_dst}/${_gen_name}.h")
    set(_gen_old "")
    if(EXISTS "${_gen_out}")
        file(READ "${_gen_out}" _gen_old)
    endif()
    if(NOT _gen_old STREQUAL _gen_text)
        file(WRITE "${_gen_out}" "${_gen_text}")
        message(STATUS "gen_examples: wrote include/BasicExamples/${_gen_name}.h")
    endif()
endforeach()
