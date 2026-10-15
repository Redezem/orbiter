# A separate real-core client; does not change the OrbiterRuntime bootstrap app.
enable_language(OBJCXX)
find_package(SDL3 CONFIG REQUIRED)
find_library(CORE_METAL_FRAMEWORK Metal REQUIRED)
find_library(CORE_FOUNDATION_FRAMEWORK Foundation REQUIRED)
find_library(CORE_QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)
add_executable(orbiter-core-metal
    main.mm
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
