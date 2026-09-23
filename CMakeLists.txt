cmake_minimum_required(VERSION 3.28)
project(darling C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(APPLE)
    enable_language(OBJC)
    set(CMAKE_OBJC_STANDARD 23)
    set(CMAKE_OBJC_STANDARD_REQUIRED ON)
endif()

# Release: dead-code stripping so unused symbols cost zero bytes in artifacts.
# (Duplicated per repo for standalone builds; harmless when built in-tree.)
if(APPLE)
    add_compile_options($<$<CONFIG:Release>:-ffunction-sections> $<$<CONFIG:Release>:-fdata-sections>)
    add_link_options($<$<CONFIG:Release>:-Wl,-dead_strip>)
elseif(UNIX)
    add_compile_options($<$<CONFIG:Release>:-ffunction-sections> $<$<CONFIG:Release>:-fdata-sections>)
    add_link_options($<$<CONFIG:Release>:-Wl,--gc-sections>)
endif()

# Check upstream dependencies (if not already provided by parent build)
if(NOT TARGET vexspoke OR NOT TARGET hotcwap OR NOT TARGET graphvex)
    option(DARLING_USE_UPSTREAM "Depend on vexspoke + hotcwap + graphvex via FetchContent" OFF)
    if(DARLING_USE_UPSTREAM)
        include(FetchContent)
        if(NOT TARGET vexspoke)
            FetchContent_Declare(
                vexspoke
                GIT_REPOSITORY https://github.com/vexgraph-dev/vexspoke.git
                GIT_TAG main
            )
        endif()
        if(NOT TARGET hotcwap)
            FetchContent_Declare(
                hotcwap
                GIT_REPOSITORY https://github.com/vexgraph-dev/hotcwap.git
                GIT_TAG main
            )
        endif()
        if(NOT TARGET graphvex)
            FetchContent_Declare(
                graphvex
                GIT_REPOSITORY https://github.com/vexgraph-dev/graphvex.git
                GIT_TAG main
            )
        endif()
        FetchContent_MakeAvailable(vexspoke hotcwap graphvex)
    endif()
endif()

if(NOT TARGET darling)
# ----------------------------------------------------------------------------
# Migrated sources — compile clean against the current R1/R2/R3 API (graphvex
# lang/*, the fused GraphicsComponent record, the Graphics row). The build list
# carries exactly what has migrated; every file below is verified warning-free
# under -Wall -Wextra -Werror.
#
# PARKED (await their own migration slice — each returns when its old-API
# dependencies move; the retired API is vulkan/vk.h, image/image.h, render/*):
#   c23/add.c, darling/anim/anim.c, darling/button/button.c,
#   darling/color/colordialog.c, darling/compositor.c, darling/cursor/cursor.c,
#   darling/dialog/{dialog,optiondialog,inputdialog}.c, darling/frame.c,
#   darling/field/{input,textarea,inputotp,searchfield}.c,
#   darling/label/{label,rich_label,typography}.c,
#   darling/overlay/filedialog.c, darling/plot/plot.c,
#   darling/panel/{expandable_list_container,markdown_panel,richtext_panel,
#   scroll_container}.c, darling/scene/canvas.c, event/dispatch.c,
#   render/{raster,surface}.c, text/rich_text.c, window/panel_bridge.c
# ----------------------------------------------------------------------------
add_library(darling STATIC
    darling/container.c
    darling/panel/panel.c
    darling/picture/picture.c
    darling/scene/scene.c
    darling/panel/layered_container.c
    darling/panel/section_container.c
    darling/panel/list_container.c
    darling/panel/grid_container.c
    darling/panel/scroll_panel.c
    darling/field/scrollbar.c
    darling/field/codefield.c
    darling/button/switch.c
    darling/field/checkbox.c
    darling/field/radiogroup.c
    darling/field/slider.c
    darling/field/knob.c
    darling/field/select.c
    darling/field/datepicker.c
    darling/field/colorpicker.c
    darling/field/colorswatch.c
    darling/color/color.c
    darling/label/kbd.c
    event/pointer.c
    event/keyevent.c
    event/focus.c
    event/action.c
    event/value.c
    event/tree.c
    event/gesture.c
    event/bridge.c
    text/text_select.c
    io/bake.c
    io/mmap.c
)

target_include_directories(darling PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(darling PUBLIC $<$<CONFIG:Debug>:DEBUG_BORROW_CHECK=1>)
target_compile_options(darling PRIVATE -Wall -Wextra -Werror -mcpu=native)

find_package(Vulkan QUIET)
if(Vulkan_FOUND)
    target_include_directories(darling PUBLIC ${Vulkan_INCLUDE_DIRS})
elseif(EXISTS "/opt/homebrew/include/vulkan/vulkan.h")
    target_include_directories(darling PUBLIC /opt/homebrew/include)
endif()

if(TARGET vexspoke)
    target_link_libraries(darling PUBLIC vexspoke)
endif()
if(TARGET hotcwap)
    target_link_libraries(darling PUBLIC hotcwap)
endif()
if(TARGET graphvex)
    target_link_libraries(darling PUBLIC graphvex)
endif()

# Cocoa UI shims (macOS only): panel layers, CoreText raster, font lookup.
# NOTE: objc/font_cocoa.m (System_* font bridge) moved down to graphvex with
# the font engine — graphvex links it, darling consumes via link (Rule 17).
if(APPLE)
    # objc/frame_cocoa.m is PARKED with darling/frame.c: both pull the retired
    # graphvex effect module (effect/visual_effect.h). It returns with frame.
    target_sources(darling PRIVATE
        objc/text_core.m
    )
    set_source_files_properties(
        objc/text_core.m
        PROPERTIES COMPILE_OPTIONS "-fobjc-arc"
    )
    target_link_libraries(darling PUBLIC
        "-framework Cocoa"
        "-framework AppKit"
        "-framework QuartzCore"
        "-framework CoreGraphics"
        "-framework IOSurface"
        "-framework Metal"
        "-framework MetalKit"
    )
else()
    target_sources(darling PRIVATE
        text/text_core_stub.c
        darling/frame_stub.c
    )
endif() # APPLE
endif() # NOT TARGET darling

if(NOT TARGET darling-ui)
    add_library(darling-ui ALIAS darling)
endif()
if(NOT TARGET darling::ui)
    add_library(darling::ui ALIAS darling)
endif()

# Live demo harness: two-thread decoupled painter (draw worker + Thread 0
# compositor). Umbrella probe — target lives in vexgraph/CMakeLists.txt
# (main/darlingtest.c) so standalone darling never sees umbrella paths.

# Headless baked-font installer CLI (no window, no GPU).
# PARKED: font/fontbake.c pulls the retired graphvex font-bake header
# (font/font_bake.h). It returns with the font-bake slice.

# Widget gallery + test suite: umbrella probe — target lives in
# vexgraph/CMakeLists.txt (main/darling_gallery.c).

set(DARLING_TEST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../tests/darling")
if(NOT EXISTS "${DARLING_TEST_DIR}")
    set(DARLING_TEST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tests")
endif()

# ----------------------------------------------------------------------------
# Migrated tests only. Each target below links the migrated darling library
# and passes headless. The parked suite returns one slice at a time, alongside
# the widget it exercises:
#
# PARKED: anim_test, anchor_lattice_test, anchor_oracle_test, button_test,
#   checkbox_test (kept below), component_test, compositor_batch_test,
#   corner_test, dialog_family_test, dispatcher_test,
#   expandable_list_container_test, frame_root_panel_test, frame_test,
#   input_test, inputotp_test, knob_test (kept below), label_highlight_test,
#   label_test, markdown_panel_test, panel_dirty_test, radiogroup_test (kept),
#   rich_label_test, scrollbar_test (kept below), search_code_test,
#   slider_test (kept below), switch_test (kept below), textarea_test,
#   text_select_test (kept below), color_test (kept below)
# ----------------------------------------------------------------------------

# Picture: Panel + a fitted image — background color, the image inside, and the
# eight modes (stretch / cover / contain / anchored source window) read back.
if(NOT TARGET picture_test AND EXISTS "${DARLING_TEST_DIR}/picture_test.c")
    add_executable(picture_test
        ${DARLING_TEST_DIR}/picture_test.c
    )
    target_link_libraries(picture_test PRIVATE darling graphvex vexspoke)
    target_compile_options(picture_test PRIVATE -Wall -Wextra -Werror)
endif()

if(NOT TARGET checkbox_test AND EXISTS "${DARLING_TEST_DIR}/checkbox_test.c")
    add_executable(checkbox_test
        ${DARLING_TEST_DIR}/checkbox_test.c
    )
    target_link_libraries(checkbox_test PRIVATE darling)
endif()

if(NOT TARGET switch_test AND EXISTS "${DARLING_TEST_DIR}/switch_test.c")
    add_executable(switch_test
        ${DARLING_TEST_DIR}/switch_test.c
    )
    target_link_libraries(switch_test PRIVATE darling)
endif()

if(NOT TARGET slider_test AND EXISTS "${DARLING_TEST_DIR}/slider_test.c")
    add_executable(slider_test
        ${DARLING_TEST_DIR}/slider_test.c
    )
    target_link_libraries(slider_test PRIVATE darling)
endif()

if(NOT TARGET knob_test AND EXISTS "${DARLING_TEST_DIR}/knob_test.c")
    add_executable(knob_test
        ${DARLING_TEST_DIR}/knob_test.c
    )
    target_link_libraries(knob_test PRIVATE darling)
endif()

if(NOT TARGET scrollbar_test AND EXISTS "${DARLING_TEST_DIR}/scrollbar_test.c")
    add_executable(scrollbar_test
        ${DARLING_TEST_DIR}/scrollbar_test.c
    )
    target_link_libraries(scrollbar_test PRIVATE darling)
endif()

if(NOT TARGET radiogroup_test AND EXISTS "${DARLING_TEST_DIR}/radiogroup_test.c")
    add_executable(radiogroup_test
        ${DARLING_TEST_DIR}/radiogroup_test.c
    )
    target_link_libraries(radiogroup_test PRIVATE darling)
endif()

if(NOT TARGET text_select_test AND EXISTS "${DARLING_TEST_DIR}/text_select_test.c")
    add_executable(text_select_test
        ${DARLING_TEST_DIR}/text_select_test.c
    )
    target_link_libraries(text_select_test PRIVATE darling)
endif()

if(NOT TARGET color_test AND EXISTS "${DARLING_TEST_DIR}/color_test.c")
    add_executable(color_test
        ${DARLING_TEST_DIR}/color_test.c
    )
    target_link_libraries(color_test PRIVATE darling)
endif()

set(DARLING_CODEFIELD_TEST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../_tests/darling")
if(NOT EXISTS "${DARLING_CODEFIELD_TEST_DIR}")
    set(DARLING_CODEFIELD_TEST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tests")
endif()
if(EXISTS "${DARLING_CODEFIELD_TEST_DIR}/codefield_test.c")
    add_executable(codefield_test "${DARLING_CODEFIELD_TEST_DIR}/codefield_test.c")
    target_link_libraries(codefield_test PRIVATE darling)
    target_compile_options(codefield_test PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()
if(APPLE AND EXISTS "${DARLING_CODEFIELD_TEST_DIR}/codefield_demo.m")
    add_executable(codefield_demo "${DARLING_CODEFIELD_TEST_DIR}/codefield_demo.m"
        "${DARLING_CODEFIELD_TEST_DIR}/codefield_demo.c")
    target_link_libraries(codefield_demo PRIVATE darling "-framework Cocoa")
    target_compile_options(codefield_demo PRIVATE -Wall -Wextra -Werror $<$<COMPILE_LANGUAGE:OBJC>:-fobjc-arc>)
endif()
