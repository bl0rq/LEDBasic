cmake_minimum_required(VERSION 3.16)
project(ledbasic_tools CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MinGW g++ dynamically links libstdc++-6.dll / libgcc / winpthread.
# VS Code's Start-Process PATH does not include MSYS2, so those DLLs
# are missing unless we static-link the runtime into the .exe.
if(MINGW)
    add_link_options(-static-libgcc -static-libstdc++ -static)
endif()

set(LEDBASIC_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")

find_package(Threads REQUIRED)

add_library(ledbasic_host STATIC
    "${LEDBASIC_ROOT}/src/BasicInterpreter.cpp"
    host/HostSession.cpp
)
target_include_directories(ledbasic_host PUBLIC
    "${LEDBASIC_ROOT}/test/native"
    "${LEDBASIC_ROOT}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/host"
)
target_link_libraries(ledbasic_host PUBLIC Threads::Threads)

add_executable(ledbasic cli/main.cpp)
target_link_libraries(ledbasic PRIVATE ledbasic_host)

option(LEDBASIC_BUILD_TUI "Build the FTXUI editor/simulator" ON)
if(LEDBASIC_BUILD_TUI)
    add_subdirectory(tui)
endif()
