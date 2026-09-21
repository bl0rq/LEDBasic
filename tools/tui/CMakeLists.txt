include(FetchContent)

set(FTXUI_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_DOCS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/ftxui.git
    GIT_TAG        v6.1.9
)
FetchContent_MakeAvailable(ftxui)

add_executable(ledbasic-tui
    main.cpp
    editor.cpp
)
target_include_directories(ledbasic-tui PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
file(TO_CMAKE_PATH "${LEDBASIC_ROOT}/examples/bas" LEDBASIC_EXAMPLES_PATH)
target_compile_definitions(ledbasic-tui PRIVATE
    "LEDBASIC_EXAMPLES_DIR=\"${LEDBASIC_EXAMPLES_PATH}\""
)
target_link_libraries(ledbasic-tui PRIVATE
    ledbasic_host
    ftxui::screen
    ftxui::dom
    ftxui::component
)
if(WIN32)
    target_link_libraries(ledbasic-tui PRIVATE user32)
endif()
