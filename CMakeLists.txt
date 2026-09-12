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
add_library(darling STATIC
        c23/add.c
    darling/scene/canvas.c
    darling/compositor.c
    darling/container.c
    darling/anim/anim.c
    darling/cursor/cursor.c
    darling/label/label.c
    darling/panel/panel.c
    darling/picture/picture.c
    darling/label/rich_label.c
    darling/scene/scene.c
    darling/panel/layered_container.c
    darling/panel/section_container.c
    darling/panel/expandable_list_container.c
    darling/panel/list_container.c
    darling/panel/grid_container.c
    darling/panel/markdown_panel.c
    darling/panel/richtext_panel.c
    darling/panel/scroll_container.c
    darling/field/scrollbar.c
    darling/button/button.c
    darling/button/switch.c
    darling/field/checkbox.c
    darling/field/radiogroup.c
    darling/field/slider.c
    darling/field/knob.c
    darling/field/input.c
    darling/field/textarea.c
    darling/field/inputotp.c
    darling/field/searchfield.c
    darling/field/codefield.c
    darling/field/select.c
    darling/field/datepicker.c
    darling/field/colorpicker.c
    darling/field/colorswatch.c
    darling/overlay/filedialog.c
    darling/label/typography.c
    darling/label/kbd.c
    darling/plot/plot.c
    event/pointer.c
    event/keyevent.c
    event/focus.c
    event/action.c
    event/value.c
    event/tree.c
    event/gesture.c
    event/dispatch.c
    event/bridge.c
    text/rich_text.c
    text/text_select.c
    render/raster.c
    render/surface.c
    io/bake.c
    io/mmap.c
    window/panel_bridge.c
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
    target_sources(darling PRIVATE
        objc/panel_cocoa.m
        objc/text_core.m
    )
    set_source_files_properties(
        objc/panel_cocoa.m
        objc/text_core.m
        PROPERTIES COMPILE_OPTIONS "-fobjc-arc"
    )
    target_link_libraries(darling PUBLIC
        "-framework Cocoa"
        "-framework AppKit"
        "-framework QuartzCore"
        "-framework CoreGraphics"
        "-framework IOSurface"
    )
else()
    target_sources(darling PRIVATE text/text_core_stub.c)
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

# Headless baked-font installer CLI (no window, no GPU): the one harness
# this agent can execute end-to-end here.
if(NOT TARGET fontbake)
    add_executable(fontbake
        font/fontbake.c
    )
    target_link_libraries(fontbake PRIVATE graphvex vexspoke)
endif()

# Widget gallery + test suite: umbrella probe — target lives in
# vexgraph/CMakeLists.txt (main/darling_gallery.c).

set(DARLING_TEST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../_tests/darling")
if(NOT EXISTS "${DARLING_TEST_DIR}")
    set(DARLING_TEST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tests")
endif()

# Phase 1 substrate artifact (layout-panels §8): headless rounded-rect diff.
if(NOT TARGET corner_test AND EXISTS "${DARLING_TEST_DIR}/corner_test.c")
    add_executable(corner_test
        ${DARLING_TEST_DIR}/corner_test.c
    )
    target_link_libraries(corner_test PRIVATE darling)
endif()

# Anim artifact (anim §1): headless easing + keyframe player suite.
if(NOT TARGET anim_test AND EXISTS "${DARLING_TEST_DIR}/anim_test.c")
    add_executable(anim_test
        ${DARLING_TEST_DIR}/anim_test.c
    )
    target_link_libraries(anim_test PRIVATE darling)
endif()

# Label & Cursor artifact: headless typography, styling, mnemonic, and cursor suite.
if(NOT TARGET label_test AND EXISTS "${DARLING_TEST_DIR}/label_test.c")
    add_executable(label_test
        ${DARLING_TEST_DIR}/label_test.c
    )
    target_link_libraries(label_test PRIVATE darling)
endif()

# LabelHighlight artifact: selection-overlay split (text-only stable raster, per-frame highlight) suite.
if(NOT TARGET label_highlight_test AND EXISTS "${DARLING_TEST_DIR}/label_highlight_test.c")
    add_executable(label_highlight_test
        ${DARLING_TEST_DIR}/label_highlight_test.c
    )
    target_link_libraries(label_highlight_test PRIVATE darling)
endif()

# RichLabel artifact: headless selection, synthetic-quad hit-test, and fixed-anchor drag suite.
if(NOT TARGET rich_label_test AND EXISTS "${DARLING_TEST_DIR}/rich_label_test.c")
    add_executable(rich_label_test
        ${DARLING_TEST_DIR}/rich_label_test.c
    )
    target_link_libraries(rich_label_test PRIVATE darling)
endif()

# MarkdownPanel artifact: document selection over stacked rows + dispatch pointer seam.
if(NOT TARGET markdown_panel_test AND EXISTS "${DARLING_TEST_DIR}/markdown_panel_test.c")
    add_executable(markdown_panel_test
        ${DARLING_TEST_DIR}/markdown_panel_test.c
    )
    target_link_libraries(markdown_panel_test PRIVATE darling)
endif()

# TextSelect artifact: shared fixed-anchor selection part state machine suite.
if(NOT TARGET text_select_test AND EXISTS "${DARLING_TEST_DIR}/text_select_test.c")
    add_executable(text_select_test
        ${DARLING_TEST_DIR}/text_select_test.c
    )
    target_link_libraries(text_select_test PRIVATE darling)
endif()

# Dispatcher artifact: darling event dispatch routing, focus gate, bridge seam.
if(NOT TARGET dispatcher_test AND EXISTS "${DARLING_TEST_DIR}/dispatcher_test.c")
    add_executable(dispatcher_test
        ${DARLING_TEST_DIR}/dispatcher_test.c
    )
    target_link_libraries(dispatcher_test PRIVATE darling)
endif()

# Button artifact (click §1): headless click, hover, down/up bounds suite.
if(NOT TARGET button_test AND EXISTS "${DARLING_TEST_DIR}/button_test.c")
    add_executable(button_test
        ${DARLING_TEST_DIR}/button_test.c
    )
    target_link_libraries(button_test PRIVATE darling)
endif()

# Checkbox artifact (click §2): headless toggle, indeterminate, inside/outside bounds suite.
if(NOT TARGET checkbox_test AND EXISTS "${DARLING_TEST_DIR}/checkbox_test.c")
    add_executable(checkbox_test
        ${DARLING_TEST_DIR}/checkbox_test.c
    )
    target_link_libraries(checkbox_test PRIVATE darling)
endif()

# Switch artifact (click §3): headless toggle, state flip, inside/outside bounds suite.
if(NOT TARGET switch_test AND EXISTS "${DARLING_TEST_DIR}/switch_test.c")
    add_executable(switch_test
        ${DARLING_TEST_DIR}/switch_test.c
    )
    target_link_libraries(switch_test PRIVATE darling)
endif()

# Slider artifact (drag §1): headless drag, step snap, range clamping suite.
if(NOT TARGET slider_test AND EXISTS "${DARLING_TEST_DIR}/slider_test.c")
    add_executable(slider_test
        ${DARLING_TEST_DIR}/slider_test.c
    )
    target_link_libraries(slider_test PRIVATE darling)
endif()

# Knob artifact (drag §2): headless rotary scrub, normalized mapping, callback suite.
if(NOT TARGET knob_test AND EXISTS "${DARLING_TEST_DIR}/knob_test.c")
    add_executable(knob_test
        ${DARLING_TEST_DIR}/knob_test.c
    )
    target_link_libraries(knob_test PRIVATE darling)
endif()

# ScrollBar artifact (drag §3): headless drag, clickAt fraction mapping suite.
if(NOT TARGET scrollbar_test AND EXISTS "${DARLING_TEST_DIR}/scrollbar_test.c")
    add_executable(scrollbar_test
        ${DARLING_TEST_DIR}/scrollbar_test.c
    )
    target_link_libraries(scrollbar_test PRIVATE darling)
endif()

# ExpandableListContainer artifact (tree/hierarchy §1): headless add, expand/collapse, depth, checklist, remove suite.
if(NOT TARGET expandable_list_container_test AND EXISTS "${DARLING_TEST_DIR}/expandable_list_container_test.c")
    add_executable(expandable_list_container_test
        ${DARLING_TEST_DIR}/expandable_list_container_test.c
    )
    target_link_libraries(expandable_list_container_test PRIVATE darling)
endif()

# RadioGroup artifact (click §4): headless options, mutual exclusion, pointer click suite.
if(NOT TARGET radiogroup_test AND EXISTS "${DARLING_TEST_DIR}/radiogroup_test.c")
    add_executable(radiogroup_test
        ${DARLING_TEST_DIR}/radiogroup_test.c
    )
    target_link_libraries(radiogroup_test PRIVATE darling)
endif()

# Input artifact (text §1): headless text insertion, deletion, caret motion, key event suite.
if(NOT TARGET darling_input_test AND EXISTS "${DARLING_TEST_DIR}/input_test.c")
    add_executable(darling_input_test
        ${DARLING_TEST_DIR}/input_test.c
    )
    target_link_libraries(darling_input_test PRIVATE darling)
endif()

# Textarea artifact (text §2): headless multiline buffer, caret, pointer, key event suite.
if(NOT TARGET textarea_test AND EXISTS "${DARLING_TEST_DIR}/textarea_test.c")
    add_executable(textarea_test
        ${DARLING_TEST_DIR}/textarea_test.c
    )
    target_link_libraries(textarea_test PRIVATE darling)
endif()

# InputOTP artifact: headless passcode digit entry, auto-advance, and completion suite.
if(NOT TARGET inputotp_test AND EXISTS "${DARLING_TEST_DIR}/inputotp_test.c")
    add_executable(inputotp_test
        ${DARLING_TEST_DIR}/inputotp_test.c
    )
    target_link_libraries(inputotp_test PRIVATE darling)
endif()

# SearchField & CodeField artifact: headless search and code editor suites.
if(NOT TARGET search_code_test AND EXISTS "${DARLING_TEST_DIR}/search_code_test.c")
    add_executable(search_code_test
        ${DARLING_TEST_DIR}/search_code_test.c
    )
    target_link_libraries(search_code_test PRIVATE darling)
endif()

# Panel dirty artifact: headless recursive tree dirtiness and clearing suite.
if(NOT TARGET panel_dirty_test AND EXISTS "${DARLING_TEST_DIR}/panel_dirty_test.c")
    add_executable(panel_dirty_test
        ${DARLING_TEST_DIR}/panel_dirty_test.c
    )
    target_link_libraries(panel_dirty_test PRIVATE darling)
endif()

# Compositor batch-ring contract test: headless settled/idle/depth/capacity
# defaults + shutdown-with-no-flight. Links darling; needs no GPU.
if(NOT TARGET compositor_batch_test AND EXISTS "${DARLING_TEST_DIR}/compositor_batch_test.c")
    add_executable(compositor_batch_test
        ${DARLING_TEST_DIR}/compositor_batch_test.c
    )
    target_link_libraries(compositor_batch_test PRIVATE darling)
endif()

# Manifest emitter artifact: build-time tool printing the module manifest
# JSON (name/version from argv) with fully stated type rows. Links nothing;
# sizeof/macros/parent table only. Run manually, never in the build graph.
if(NOT TARGET emit_darling_manifest)
    add_executable(emit_darling_manifest
        tools/emit_manifest.c
    )
    target_include_directories(emit_darling_manifest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_options(emit_darling_manifest PRIVATE -Wall -Wextra -Werror)
endif()



