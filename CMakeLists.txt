cmake_minimum_required(VERSION 3.15...4.0 FATAL_ERROR)

set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)
set(LICENSECC_SHORT_LICENSE "BSD Software License")

cmake_policy(SET CMP0048 NEW)
# Use old FindBoost.cmake behavior instead of requiring BoostConfig.cmake
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 OLD)
endif()
# Enable modern MSVC runtime library selection
cmake_policy(SET CMP0091 NEW)
project (lccgen
	VERSION 2.1.0
	DESCRIPTION "License generator for licensecc" 
	LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/" "${CMAKE_ROOT}/Modules/")

include(utilities) #import module utilities.cmake

#Not sure about this. My system doesn't support binfmt misc
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CROSSCOMPILING AND NOT DEFINED CMAKE_CROSSCOMPILING_EMULATOR)
	set(CMAKE_CROSSCOMPILING_EMULATOR "wine")	
endif()

set(OPENSSL_USE_STATIC_LIBS ON)
find_package(OpenSSL COMPONENTS Crypto QUIET)
if(OPENSSL_FOUND)
	add_definitions(-DHAS_OPENSSL)
	include_directories(${OPENSSL_INCLUDE_DIR})
	message(STATUS "Found openssl version ${OPENSSL_VERSION} - generator")
endif()

if(UNIX) #this is true for all the linux systems but not for cross compiling "linux->windows"
	#openssl required for linux
	if(NOT OPENSSL_FOUND)
		message(SEND_ERROR "Openssl not found - generator - required in Linux")
	endif()
	# For static builds on Unix, use static linking
	if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--whole-archive -static-libgcc -static-libstdc++ -static")
		unset(CMAKE_DL_LIBS)
	endif()
	
    find_package(Threads)
	if(THREADS_HAVE_PTHREAD_ARG)
  		list(APPEND EXTERNAL_LIBS "-pthread")
	else()
		if(CMAKE_THREAD_LIBS_INIT)
  			 list(APPEND EXTERNAL_LIBS "${CMAKE_THREAD_LIBS_INIT}")
		endif()
	endif()
	
	#Zlib required when openssl version < 1.0.2k (centos 7)
	if(OPENSSL_VERSION VERSION_LESS_EQUAL 1.0.2)
		set(ZLIB_USE_STATIC_LIBS ON) 
		find_package(ZLIB REQUIRED)
		list(APPEND EXTERNAL_LIBS ${ZLIB_LIBRARIES})
		message(STATUS "Found zlib version ${ZLIB_VERSION_STRING} - generator")
	endif()

	find_program( MEMORYCHECK_COMMAND valgrind )
	set( MEMORYCHECK_COMMAND_OPTIONS "--trace-children=yes --leak-check=full" ) 
	set(pkg_config_dest "lib/cmake/${PROJECT_NAME}")
else() #windows and linux crosscompiling for windows target
	if(NOT OPENSSL_FOUND)
		message(STATUS "Openssl not found - generator - configuring libary without it.")
	endif()
	if(MSVC)
		include(target_arch)
		target_architecture( TARGET_ARCHITECTURE )
		message(STATUS "architecture detected: ${TARGET_ARCHITECTURE}")
		
		# Use modern CMake approach for runtime library selection
		# Set static runtime library for all targets
		set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
		message(STATUS "Using static runtime library (CMAKE_MSVC_RUNTIME_LIBRARY)")
		set(Boost_USE_STATIC_RUNTIME ON)
		add_definitions("/D _CRT_SECURE_NO_WARNINGS")
		list(APPEND EXTERNAL_LIBS "bcrypt" "crypt32" "ws2_32" "iphlpapi")
	else()
	    if(MINGW)
	        list(APPEND EXTERNAL_LIBS "-lcrypt32 -lbcrypt -lws2_32 -liphlpapi")
	        set(CMAKE_EXE_LINKER_FLAGS "-static")
	    endif()
	endif()
	set(pkg_config_dest "cmake/${PROJECT_NAME}")
endif()

#bug in cmake win32 - can't find boost compiled with mingw 
if(WIN32 AND "x${CMAKE_CXX_COMPILER_ARCHITECTURE_ID}" STREQUAL "x")
    message(WARNING "WIN32 compiler does not specify CMAKE_CXX_COMPILER_ARCHITECTURE_ID -- filling in manually")
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(CMAKE_CXX_COMPILER_ARCHITECTURE_ID "x64")
    else()
        set(CMAKE_CXX_COMPILER_ARCHITECTURE_ID "x86")
    endif()
    message(STATUS "Compiler architecture: ${CMAKE_CXX_COMPILER_ARCHITECTURE_ID}")
endif()

set(Boost_USE_STATIC_LIBS ON)
find_package(Boost REQUIRED COMPONENTS date_time filesystem program_options system unit_test_framework)

#set below in case of dynamic linking in debug.
#set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS $<$<CONFIG:Debug>:BOOST_TEST_DYN_LINK>)
add_definitions(-DBOOST_ALL_NO_LIB) #Disable Boost Microsoft magic, all dependencies are handled by cmake
add_definitions(-DBOOST_LIB_DIAGNOSTIC) #Check it is really disabled
include_directories(${Boost_INCLUDE_DIRS}) 

configure_file (
	"src/build_properties.h.in" 
	"${CMAKE_BINARY_DIR}/include/build_properties.h"
)

message(STATUS "CXX compiler              : " ${CMAKE_CXX_COMPILER})
message(STATUS "CXX compiler flags        : " ${CMAKE_CXX_FLAGS})
message(STATUS "CXX compiler flags debug  : " ${CMAKE_CXX_FLAGS_DEBUG})
message(STATUS "CXX compiler flags release: " ${CMAKE_CXX_FLAGS_RELEASE})
if(CMAKE_BUILD_TYPE)
  message(STATUS "Build type                : " ${CMAKE_BUILD_TYPE})
endif()
message(STATUS "Install prefix            : " ${CMAKE_INSTALL_PREFIX})
#include (build directory)/include to find build_properties.h
message(STATUS "External libs             : " ${EXTERNAL_LIBS})
include_directories(${CMAKE_BINARY_DIR}/include)
add_subdirectory("src/base_lib")
add_subdirectory("src/license_generator")

include(CTest)
if(BUILD_TESTING)
    set(BUILDNAME "${BUILDNAME}" CACHE STRING "Name of build on the dashboard")
	mark_as_advanced(BUILDNAME)
	add_subdirectory("test")
endif()

set(CPACK_GENERATOR "DEB;TBZ2;RPM")
set(CPACK_PACKAGE_NAME "lcc-generator")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "open license manager Team")
set(CPACK_PACKAGING_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${PROJECT_VERSION}.${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_OUTPUT_FILE_PREFIX dist)
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
include(CPack)

