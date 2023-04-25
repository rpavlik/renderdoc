set(sources
    xr_common.cpp
    xr_common.h
    # xr_next_chains.cpp
    xr_core.cpp
    xr_core.h
    # xr_counters.cpp
    # xr_debug.h
    # xr_debug.cpp
    # xr_postvs.cpp
    # xr_shader_feedback.cpp
    # xr_overlay.cpp
    # xr_msaa_buffer_conv.cpp
    # xr_outputwindow.cpp
    # xr_rendermesh.cpp
    # xr_rendertexture.cpp
    # xr_rendertext.h
    # xr_rendertext.cpp
    # xr_shader_cache.h
    # xr_shader_cache.cpp
    # xr_dispatchtables.cpp
    # xr_dispatchtables.h
    # xr_dispatch_defs.h
    # xr_hookset_defs.h
    # xr_image_states.cpp
    # xr_info.cpp
    # xr_info.h
    # xr_initstate.cpp
    # xr_manager.cpp
    # xr_manager.h
    # xr_memory.cpp
    # xr_pixelhistory.cpp
    # xr_replay.cpp
    # xr_replay.h
    # xr_resources.cpp
    # xr_resources.h
    # xr_shaderdebug.cpp
    # xr_state.cpp
    # xr_state.h
    # xr_serialise.cpp
    # xr_stringise.cpp
    xr_layer.cpp
    # imagestate_tests.cpp
    # imgrefs_tests.cpp
    official/loader_interfaces.h
    official/openxr.h
    official/openxr_platform_defines.h
    official/openxr_platform.h
    official/openxr_reflection.h
    official/openxr_reflection_parent_structs.h
    official/openxr_reflection_structs.h
    official/platform_utils.hpp
    official/xr_generated_dispatch_table.c
    official/xr_generated_dispatch_table.h)
    # wrappers/vk_cmd_funcs.cpp
    # wrappers/vk_descriptor_funcs.cpp
    # wrappers/vk_device_funcs.cpp
    # wrappers/vk_draw_funcs.cpp
    # wrappers/vk_dynamic_funcs.cpp
    # wrappers/vk_get_funcs.cpp
    # wrappers/vk_misc_funcs.cpp
    # wrappers/vk_queue_funcs.cpp
    # wrappers/vk_resource_funcs.cpp
    # wrappers/vk_shader_funcs.cpp
    # wrappers/vk_sync_funcs.cpp
    # wrappers/vk_wsi_funcs.cpp)

set(definitions ${RDOC_DEFINITIONS})

set(OPENXR_LAYER_FOLDER_DEFAULT /etc/openxr/implicit_layer.d)

if(NOT ANDROID AND NOT CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT AND NOT DEFINED OPENXR_LAYER_FOLDER)
    message(WARNING "*** CMAKE_INSTALL_PREFIX has been customised to ${CMAKE_INSTALL_PREFIX}, but OPENXR_LAYER_FOLDER is not customised and defaults to ${OPENXR_LAYER_FOLDER_DEFAULT}. This may not do what you expect, e.g. installing to a non-root location ***")
endif()

# This must be specified separately because it needs to go in /etc regardless of what the install
# prefix is, since the loader only looks in a set location (and /usr/share is reserved for distribution
# packages). For people who want to 'make install' to another folder, perhaps for preparing a package,
# they can set this variable to make sure it stays local
set(OPENXR_LAYER_FOLDER ${OPENXR_LAYER_FOLDER_DEFAULT} CACHE PATH "Path to install the OpenXR layer file")

if(ANDROID)
    # list(APPEND sources xr_posix.cpp xr_android.cpp xr_layer_android.cpp)
    list(APPEND definitions PRIVATE -DXR_USE_PLATFORM_ANDROID)
# elseif(APPLE)
#     list(APPEND sources xr_posix.cpp xr_apple.cpp xr_apple_helpers.mm)

#     add_definitions(-DXR_USE_PLATFORM_MACOS_MVK -DXR_USE_PLATFORM_METAL_EXT)
elseif(UNIX)
    list(APPEND sources xr_posix.cpp xr_linux.cpp)

    if(ENABLE_XLIB)
        add_definitions(-DXR_USE_PLATFORM_XLIB)
    endif()

    if(ENABLE_XCB)
        add_definitions(-DXR_USE_PLATFORM_XCB)
    endif()

    if(ENABLE_WAYLAND)
        add_definitions(-DXR_USE_PLATFORM_WAYLAND)
    endif()

    # add_definitions(-DXR_USE_PLATFORM_WIN32)

    set(OPENXR_LAYER_MODULE_PATH "${CMAKE_INSTALL_PREFIX}/lib${LIB_SUFFIX}/${LIB_SUBFOLDER_TRAIL_SLASH}librenderdoc.so")
    set(OPENXR_ENABLE_VAR "ENABLE_OPENXR_RENDERDOC_CAPTURE")

    set(json_in ${CMAKE_CURRENT_SOURCE_DIR}/renderdoc.json)
    set(json_out ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/renderdoc_capture${OPENXR_JSON_SUFFIX}.json)

    configure_file(${json_in} ${json_out})

    install (FILES ${json_out} DESTINATION ${OPENXR_LAYER_FOLDER})
endif()

add_library(rdoc_openxr OBJECT ${sources})
target_compile_definitions(rdoc_openxr ${definitions})
target_include_directories(rdoc_openxr ${RDOC_INCLUDES})
