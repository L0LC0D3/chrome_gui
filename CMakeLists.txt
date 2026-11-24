cmake_minimum_required(VERSION 3.20)
project(chrome_gui LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "chrome_gui draws with Win32 and GDI+ and builds on Windows only.")
endif()

option(CHROME_GUI_BUILD_DEMO "Build the demo application" ${PROJECT_IS_TOP_LEVEL})

file(GLOB_RECURSE CHROME_GUI_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/chrome/*.cpp")
file(GLOB_RECURSE CHROME_GUI_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/chrome/*.hpp")

add_library(chrome_gui STATIC ${CHROME_GUI_SOURCES} ${CHROME_GUI_HEADERS})
add_library(chrome::gui ALIAS chrome_gui)
# Headers are included as <chrome/controls/core/draw.hpp>.
target_include_directories(chrome_gui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(chrome_gui PUBLIC cxx_std_23)
target_compile_definitions(chrome_gui PRIVATE UNICODE _UNICODE)
target_link_libraries(chrome_gui PUBLIC dwmapi gdiplus winmm ole32 shell32 uxtheme comctl32)
if(MSVC)
    target_compile_options(chrome_gui PRIVATE /W3 /utf-8)
endif()

# The demo panels (every control on show), usable inside any dock host.
add_library(chrome_gui_demo_panels STATIC
    demo/controls_panel.cpp
    demo/controls_panel.hpp
    demo/data_controls_panel.cpp
    demo/data_controls_panel.hpp)
add_library(chrome::demo_panels ALIAS chrome_gui_demo_panels)
target_link_libraries(chrome_gui_demo_panels PUBLIC chrome_gui)

if(CHROME_GUI_BUILD_DEMO)
    add_executable(chrome_gui_demo WIN32 demo/main.cpp)
    target_link_libraries(chrome_gui_demo PRIVATE chrome_gui_demo_panels)
endif()
