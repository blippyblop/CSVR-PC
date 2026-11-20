cmake_minimum_required(VERSION 3.5)
project(cs16-client)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")
set(EXTRAS_DIR "${CMAKE_CURRENT_BINARY_DIR}/extras")

include(LibraryNaming)
# include(ExternalProject)

option(CSVR_ENABLE_BOT_LTO "Enable upstream bot link-time optimization" OFF)
if(NOT ANDROID)
	option(MAINUI_USE_CUSTOM_FONT_RENDER "Opt into outline font rendering instead of game bitmap fonts" OFF)
endif()
option(BUILD_CLIENT "Build client library." ON)
option(BUILD_MAINUI "Build menu library." ON)
option(BUILD_SERVER "Build server library." ON)

set(GAME_DIR "cstrike" CACHE STRING "Game directory name.")
set(CLIENT_INSTALL_DIR "cl_dlls" CACHE STRING "Client library location.")
set(SERVER_INSTALL_DIR "dlls" CACHE STRING "Server library location.")

# if(MSVC AND CMAKE_BUILD_TYPE STREQUAL "Debug")
# 	add_compile_options(/fsanitize=address)
# 	add_link_options(/fsanitize=address)
# endif()

if("${CMAKE_CXX_COMPILER}" MATCHES "em\\+\\+")
	set(EMSCRIPTEN TRUE)
else()
	set(EMSCRIPTEN FALSE)
endif()

if(VITA)
	message(STATUS "Building for PS Vita")
	add_compile_options(-fno-use-cxa-atexit)
endif()

if(BUILD_CLIENT)
	add_subdirectory(cl_dll)
endif()

# if(CMAKE_BUILD_TYPE STREQUAL "Debug")
# 	set(MESON_BUILD_TYPE debug)
# elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
# 	set(MESON_BUILD_TYPE debugoptimized)
# else()
# 	set(MESON_BUILD_TYPE release)
# endif()

if(BUILD_SERVER)
	# ExternalProject_Add(
	# 	yapb
	# 	SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/yapb
	# 	CONFIGURE_COMMAND meson setup --buildtype ${MESON_BUILD_TYPE} build
	# 	BUILD_COMMAND meson compile -C build
	# 	BUILD_ALWAYS 1
	# 	BUILD_IN_SOURCE 1
	# 	INSTALL_COMMAND meson install -C build --destdir ${CMAKE_INSTALL_PREFIX}
	# )
	if (NOT EMSCRIPTEN)
		add_subdirectory(3rdparty/yapb)
		if(NOT CSVR_ENABLE_BOT_LTO AND NOT MSVC)
			get_target_property(_bot_options yapb COMPILE_OPTIONS)
			list(FILTER _bot_options EXCLUDE REGEX "^-flto")
			set_property(TARGET yapb PROPERTY COMPILE_OPTIONS "${_bot_options}")
		endif()
		set_target_postfix(yapb)
	endif()

	set(XASH_COMPAT ON CACHE BOOL "" FORCE)
	add_subdirectory(3rdparty/ReGameDLL_CS)
endif()

if(BUILD_MAINUI)
#	set(CS16CLIENT ON)
	# set(XASH_SDK ../../)
	add_subdirectory(3rdparty/mainui_cpp)
endif()

include(GenerateExtras)

if(WIN32)
	set(CPACK_GENERATOR "ZIP")
	set(CPACK_PACKAGE_FILE_NAME "CS16Client-${CMAKE_SYSTEM_NAME}-${CMAKE_CXX_COMPILER_ARCHITECTURE_ID}")
elseif(VITA)
	set(CPACK_GENERATOR "ZIP")
	set(CPACK_PACKAGE_FILE_NAME "CS16Client-PSVita")
else()
	set(CPACK_GENERATOR "TGZ")
	set(CPACK_PACKAGE_FILE_NAME "CS16Client-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
endif()

set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)

include(CPack)
option(BUILD_DESKTOP_XR_TESTS "Build deterministic desktop OpenXR frame tests" OFF)
if(BUILD_DESKTOP_XR_TESTS AND NOT ANDROID)
	include(CTest)
	set(_vr_dir "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/xash3d-fwgs/engine/vr")
	add_executable(csvr-xr-renderer-test tests/desktop/renderer_test.c ${_vr_dir}/VrRenderer.c ${_vr_dir}/VrMath.c)
	target_include_directories(csvr-xr-renderer-test PRIVATE ${_vr_dir} 3rdparty/xash3d-fwgs/3rdparty/openxr/include 3rdparty/xash3d-fwgs/public)
	if(UNIX)
		target_link_libraries(csvr-xr-renderer-test PRIVATE m)
	endif()
	add_test(NAME desktop-xr-renderer COMMAND csvr-xr-renderer-test)
	add_executable(csvr-xr-input-test tests/desktop/input_test.c ${_vr_dir}/VrInput.c ${_vr_dir}/VrMath.c 3rdparty/xash3d-fwgs/public/crtlib.c)
	target_include_directories(csvr-xr-input-test PRIVATE ${_vr_dir} 3rdparty/xash3d-fwgs/3rdparty/openxr/include 3rdparty/xash3d-fwgs/common 3rdparty/xash3d-fwgs/public)
	target_compile_options(csvr-xr-input-test PRIVATE -ffunction-sections -fdata-sections)
	target_link_options(csvr-xr-input-test PRIVATE -Wl,--gc-sections)
	if(UNIX)
		target_link_libraries(csvr-xr-input-test PRIVATE m)
		target_compile_definitions(csvr-xr-input-test PRIVATE HAVE_STRCASECMP=1)
	elseif(WIN32)
		target_compile_definitions(csvr-xr-input-test PRIVATE HAVE_STRICMP=1)
	endif()
	add_test(NAME desktop-xr-input COMMAND csvr-xr-input-test)
endif()

if(BUILD_DESKTOP_XR_TESTS AND UNIX AND NOT ANDROID)
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(XR_TEST_SDL REQUIRED IMPORTED_TARGET sdl2)
	set(OpenGL_GL_PREFERENCE GLVND)
	find_package(OpenGL REQUIRED)
	add_executable(csvr-xr-framebuffer-test tests/desktop/framebuffer_test.c ${_vr_dir}/VrFramebufferDesktop.c ${_vr_dir}/VrMath.c)
	target_include_directories(csvr-xr-framebuffer-test PRIVATE ${_vr_dir} 3rdparty/xash3d-fwgs/3rdparty/openxr/include 3rdparty/xash3d-fwgs/public)
	target_link_libraries(csvr-xr-framebuffer-test PRIVATE PkgConfig::XR_TEST_SDL OpenGL::GL m)
	add_test(NAME desktop-xr-framebuffer COMMAND csvr-xr-framebuffer-test)
	set_tests_properties(desktop-xr-framebuffer PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=offscreen;LIBGL_ALWAYS_SOFTWARE=1")
endif()
