include(FetchContent)

set(FTXUI_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_DOCS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/ftxui.git
    GIT_TAG        v6.1.9
)
FetchContent_MakeAvailable(ftxui)

# Everything but main(), so tests can build the UI without a terminal.
add_library(ledbasic_tui_core STATIC
    app.cpp
    editor.cpp
    ../host/DeviceClient.cpp
)
target_include_directories(ledbasic_tui_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../host
)
file(TO_CMAKE_PATH "${LEDBASIC_ROOT}/examples/bas" LEDBASIC_EXAMPLES_PATH)
target_compile_definitions(ledbasic_tui_core PUBLIC
    "LEDBASIC_EXAMPLES_DIR=\"${LEDBASIC_EXAMPLES_PATH}\""
)
target_link_libraries(ledbasic_tui_core PUBLIC
    ledbasic_host
    ftxui::screen
    ftxui::dom
    ftxui::component
)
if(WIN32)
    target_link_libraries(ledbasic_tui_core PUBLIC user32 winhttp)
endif()

add_executable(ledbasic-tui main.cpp)
target_link_libraries(ledbasic-tui PRIVATE ledbasic_tui_core)

add_executable(ledbasic_tui_tests
    tests/test_main.cpp
    tests/test_editor.cpp
    tests/test_app.cpp
    tests/test_file_menu.cpp
)
target_link_libraries(ledbasic_tui_tests PRIVATE ledbasic_tui_core)
