PROJECT(ChimeraTK-DeviceAccess-DoocsBackend)
cmake_minimum_required(VERSION 3.16)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/Modules)

set(${PROJECT_NAME}_MAJOR_VERSION 01)
set(${PROJECT_NAME}_MINOR_VERSION 05)
set(${PROJECT_NAME}_PATCH_VERSION 00)
include(cmake/set_version_numbers.cmake)

include(cmake/set_default_build_to_release.cmake)
include(cmake/set_default_flags.cmake)
include(cmake/enable_code_coverage_report.cmake)

# linker flags
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--export-dynamic")

FIND_PACKAGE(ChimeraTK-DeviceAccess 03.08 REQUIRED)
# we need the zmq client which is part of server component
FIND_PACKAGE(DOOCS 22.11 REQUIRED COMPONENTS server)
FIND_PACKAGE(doocs-server-test-helper 01.06 REQUIRED)
FIND_PACKAGE(Boost REQUIRED COMPONENTS thread system filesystem unit_test_framework)

include_directories(${CMAKE_SOURCE_DIR}/include)
FIND_PACKAGE(PkgConfig REQUIRED)

set(LIBXML++_VERSION "libxml++-2.6")
PKG_CHECK_MODULES(LibXML++ REQUIRED IMPORTED_TARGET ${LIBXML++_VERSION})
PKG_CHECK_MODULES(glib REQUIRED IMPORTED_TARGET glib-2.0)

set(${PROJECT_NAME}_INCLUDE_DIRS ${${PROJECT_NAME}_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR}/include/)

SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# C++ library
aux_source_directory(${CMAKE_SOURCE_DIR}/src library_sources)
add_library(${PROJECT_NAME} SHARED ${library_sources} )
set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${${PROJECT_NAME}_FULL_LIBRARY_VERSION} SOVERSION ${${PROJECT_NAME}_SOVERSION})
target_link_libraries(${PROJECT_NAME}
    PUBLIC ChimeraTK::ChimeraTK-DeviceAccess 
    PRIVATE DOOCS::server
    PRIVATE PkgConfig::LibXML++ PkgConfig::glib)
# --no-as-needed: force linking against this library. This is required for proper registering.
target_link_options(${PROJECT_NAME} PUBLIC "-Wl,--no-as-needed")

option(BUILD_TESTS "Build test programs" ON)
if(BUILD_TESTS)

  # Create the executables for automated unit testing.
  # Currently we assume that they are all tests contained in one file, so
  # each file gives a new executable. This section has to be adapted if this should change.
  # loop over all sources files in execultables_src directory
  ENABLE_TESTING()
  aux_source_directory(${CMAKE_SOURCE_DIR}/tests/executables_src testExecutables)
  foreach( testExecutableSrcFile ${testExecutables})
    # NAME_WE means the base name without path and (longest) extension
    get_filename_component(excutableName ${testExecutableSrcFile} NAME_WE)
    # each test includes a DOOCS server
    add_executable(${excutableName} ${testExecutableSrcFile} ${CMAKE_SOURCE_DIR}/tests/DoocsDummyServer/doocsDummy_rpc_server.cc ${CMAKE_SOURCE_DIR}/tests/DoocsDummyServer/eq_dummy.cc)
    target_include_directories(${excutableName} PRIVATE "${CMAKE_SOURCE_DIR}/tests/DoocsDummyServer")
    target_link_libraries(${excutableName} 
        PRIVATE  ChimeraTK::doocs-server-test-helper ChimeraTK::ChimeraTK-DeviceAccess DOOCS::server ${PROJECT_NAME})
    add_test(${excutableName} ${excutableName})
    # copy config file
    FILE( COPY ${CMAKE_SOURCE_DIR}/tests/doocsDummy_rpc_server.conf DESTINATION ${PROJECT_BINARY_DIR})
    FILE( RENAME ${PROJECT_BINARY_DIR}/doocsDummy_rpc_server.conf ${PROJECT_BINARY_DIR}/${excutableName}.conf)
  endforeach( testExecutableSrcFile )

  FILE( COPY ${CMAKE_SOURCE_DIR}/tests/dummies.dmap DESTINATION ${PROJECT_BINARY_DIR})

endif()

# Enable documentation
include(cmake/enable_doxygen_documentation.cmake)

#Install the library and the executables
install(TARGETS ${PROJECT_NAME}
    EXPORT ${PROJECT_NAME}Targets
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
  )

# we support our cmake EXPORTS as imported targets
set(PROVIDES_EXPORTED_TARGETS 1)
# we need the public dependencies so create_cmake_config_files can find them as implicit dependencies
list(APPEND ${PROJECT_NAME}_PUBLIC_DEPENDENCIES "ChimeraTK-DeviceAccess")
include(${CMAKE_SOURCE_DIR}/cmake/create_cmake_config_files.cmake)
