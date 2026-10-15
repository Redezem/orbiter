# A separate real-core client; does not change the OrbiterRuntime bootstrap app.
enable_language(OBJCXX)
find_package(SDL3 CONFIG REQUIRED)
find_library(CORE_METAL_FRAMEWORK Metal REQUIRED)
find_library(CORE_FOUNDATION_FRAMEWORK Foundation REQUIRED)
find_library(CORE_QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)
add_executable(orbiter-core-metal
    main.mm
    session_input.cpp
    imgui_platform.cpp
    dds/dds_decoder.cpp
    ${ORB_IMGUI_SRC}/backends/imgui_impl_metal.mm
)
target_compile_definitions(orbiter-core-metal PRIVATE ORBITER_NO_WIN32_SHELL)
target_compile_features(orbiter-core-metal PRIVATE cxx_std_17)
target_compile_options(orbiter-core-metal PRIVATE $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>)
target_include_directories(orbiter-core-metal PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../shim
    ${CMAKE_SOURCE_DIR}/Src/Orbiter
    ${CMAKE_SOURCE_DIR}/Orbitersdk/include
    ${CMAKE_SOURCE_DIR}/OVP
    ${CMAKE_SOURCE_DIR}/Src/Platform
    ${ORB_IMGUI_SRC}
    ${imgui_SOURCE_DIR}
    ${implot_SOURCE_DIR}
    ${TRACY_CLIENT_INCLUDE}
)
target_link_libraries(orbiter-core-metal PRIVATE
    -Wl,-force_load,$<TARGET_FILE:OrbiterCore>
    OrbiterCore OrbiterPlatformRuntime zlib SDL3::SDL3
    ${CORE_METAL_FRAMEWORK} ${CORE_FOUNDATION_FRAMEWORK} ${CORE_QUARTZCORE_FRAMEWORK}
)
target_link_options(orbiter-core-metal PRIVATE
    -Wl,-rpath,. -Wl,-rpath,@executable_path
    -Wl,-rpath,$<TARGET_FILE_DIR:SDL3::SDL3>
)

add_executable(orbiter-buffered-input-tests test_buffered_input.cpp session_input.cpp ../shim/shim_input.cpp)
target_compile_features(orbiter-buffered-input-tests PRIVATE cxx_std_17)
target_include_directories(orbiter-buffered-input-tests PRIVATE ../shim ${CMAKE_SOURCE_DIR}/Orbitersdk/include ${CMAKE_SOURCE_DIR}/Src/Platform)
target_link_libraries(orbiter-buffered-input-tests PRIVATE SDL3::SDL3)
target_link_options(orbiter-buffered-input-tests PRIVATE -Wl,-rpath,$<TARGET_FILE_DIR:SDL3::SDL3>)

# Adapter tests exercise the real DI shim but never initialise video or a window.
add_executable(orbiter-session-input-tests test_session_input.cpp session_input.cpp ../shim/shim_input.cpp)
target_compile_features(orbiter-session-input-tests PRIVATE cxx_std_17)
target_include_directories(orbiter-session-input-tests PRIVATE ../shim ${CMAKE_SOURCE_DIR}/Orbitersdk/include ${CMAKE_SOURCE_DIR}/Src/Platform)
target_link_libraries(orbiter-session-input-tests PRIVATE SDL3::SDL3)
target_link_options(orbiter-session-input-tests PRIVATE -Wl,-rpath,$<TARGET_FILE_DIR:SDL3::SDL3>)

# Independent backend path test: its own process/context, never a second engine context.
add_executable(orbiter-imgui-platform-tests test_imgui_platform.cpp imgui_platform.cpp session_input.cpp
    ${ORB_IMGUI_SRC}/imgui.cpp ${ORB_IMGUI_SRC}/imgui_draw.cpp
    ${ORB_IMGUI_SRC}/imgui_tables.cpp ${ORB_IMGUI_SRC}/imgui_widgets.cpp)
target_compile_features(orbiter-imgui-platform-tests PRIVATE cxx_std_17)
target_include_directories(orbiter-imgui-platform-tests PRIVATE ${ORB_IMGUI_SRC})
target_link_libraries(orbiter-imgui-platform-tests PRIVATE SDL3::SDL3)
target_link_options(orbiter-imgui-platform-tests PRIVATE -Wl,-rpath,$<TARGET_FILE_DIR:SDL3::SDL3>)
