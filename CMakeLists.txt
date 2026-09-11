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

# Phase 1 substrate artifact (layout-panels §8): headless rounded-rect diff.
if(NOT TARGET corner_test)
    add_executable(corner_test
        render/corner_test.c
    )
    target_link_libraries(corner_test PRIVATE darling)
endif()

# Anim artifact (anim §1): headless easing + keyframe player suite.
if(NOT TARGET anim_test)
    add_executable(anim_test
        darling/anim/anim_test.c
    )
    target_link_libraries(anim_test PRIVATE darling)
endif()

# Label & Cursor artifact: headless typography, styling, mnemonic, and cursor suite.
if(NOT TARGET label_test)
    add_executable(label_test
        darling/label/label_test.c
    )
    target_link_libraries(label_test PRIVATE darling)
endif()

# RichLabel artifact: headless selection, synthetic-quad hit-test, and fixed-anchor drag suite.
if(NOT TARGET rich_label_test)
    add_executable(rich_label_test
        darling/label/rich_label_test.c
    )
    target_link_libraries(rich_label_test PRIVATE darling)
endif()

# MarkdownPanel artifact: document selection over stacked rows + dispatch pointer seam.
if(NOT TARGET markdown_panel_test)
    add_executable(markdown_panel_test
        darling/panel/markdown_panel_test.c
    )
    target_link_libraries(markdown_panel_test PRIVATE darling)
endif()

# Button artifact (click §1): headless click, hover, down/up bounds suite.
if(NOT TARGET button_test)
    add_executable(button_test
        darling/button/button_test.c
    )
    target_link_libraries(button_test PRIVATE darling)
endif()

# Checkbox artifact (click §2): headless toggle, indeterminate, inside/outside bounds suite.
if(NOT TARGET checkbox_test)
    add_executable(checkbox_test
        darling/field/checkbox_test.c
    )
    target_link_libraries(checkbox_test PRIVATE darling)
endif()

# Switch artifact (click §3): headless toggle, state flip, inside/outside bounds suite.
if(NOT TARGET switch_test)
    add_executable(switch_test
        darling/button/switch_test.c
    )
    target_link_libraries(switch_test PRIVATE darling)
endif()

# Slider artifact (drag §1): headless drag, step snap, range clamping suite.
if(NOT TARGET slider_test)
    add_executable(slider_test
        darling/field/slider_test.c
    )
    target_link_libraries(slider_test PRIVATE darling)
endif()

# Knob artifact (drag §2): headless rotary scrub, normalized mapping, callback suite.
if(NOT TARGET knob_test)
    add_executable(knob_test
        darling/field/knob_test.c
    )
    target_link_libraries(knob_test PRIVATE darling)
endif()

# ScrollBar artifact (drag §3): headless drag, clickAt fraction mapping suite.
if(NOT TARGET scrollbar_test)
    add_executable(scrollbar_test
        darling/field/scrollbar_test.c
    )
    target_link_libraries(scrollbar_test PRIVATE darling)
endif()

# ExpandableListContainer artifact (tree/hierarchy §1): headless add, expand/collapse, depth, checklist, remove suite.
if(NOT TARGET expandable_list_container_test)
    add_executable(expandable_list_container_test
        darling/panel/expandable_list_container_test.c
    )
    target_link_libraries(expandable_list_container_test PRIVATE darling)
endif()

# RadioGroup artifact (click §4): headless options, mutual exclusion, pointer click suite.
if(NOT TARGET radiogroup_test)
    add_executable(radiogroup_test
        darling/field/radiogroup_test.c
    )
    target_link_libraries(radiogroup_test PRIVATE darling)
endif()

# Input artifact (text §1): headless text insertion, deletion, caret motion, key event suite.
if(NOT TARGET darling_input_test)
    add_executable(darling_input_test
        darling/field/input_test.c
    )
    target_link_libraries(darling_input_test PRIVATE darling)
endif()

# Textarea artifact (text §2): headless multiline buffer, caret, pointer, key event suite.
if(NOT TARGET textarea_test)
    add_executable(textarea_test
        darling/field/textarea_test.c
    )
    target_link_libraries(textarea_test PRIVATE darling)
endif()

# Panel dirty artifact: headless recursive tree dirtiness and clearing suite.
if(NOT TARGET panel_dirty_test)
    add_executable(panel_dirty_test
        darling/panel/panel_dirty_test.c
    )
    target_link_libraries(panel_dirty_test PRIVATE darling)
endif()



