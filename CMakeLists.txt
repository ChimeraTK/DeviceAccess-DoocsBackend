PROJECT(ChimeraTK-DeviceAccess-DoocsBackend)
cmake_minimum_required(VERSION 2.8)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/Modules)

set(${PROJECT_NAME}_MAJOR_VERSION 01)
set(${PROJECT_NAME}_MINOR_VERSION 03)
set(${PROJECT_NAME}_PATCH_VERSION 00)
include(cmake/set_version_numbers.cmake)

include(cmake/set_default_build_to_release.cmake)
include(cmake/set_default_flags.cmake)

# linker flags
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--export-dynamic")

include(cmake/add_dependency.cmake)
add_dependency(ChimeraTK-DeviceAccess 02.04 REQUIRED)
add_dependency(DOOCS 21.4.0 COMPONENTS server REQUIRED)
add_dependency(doocs-server-test-helper 01.05 REQUIRED)

FIND_PACKAGE(Boost COMPONENTS thread system filesystem unit_test_framework REQUIRED)
include_directories(${CMAKE_SOURCE_DIR}/include)
FIND_PACKAGE(PkgConfig REQUIRED)

set(LIBXML++_VERSION "libxml++-2.6")
PKG_CHECK_MODULES(LibXML++ REQUIRED ${LIBXML++_VERSION})
PKG_CHECK_MODULES(glib REQUIRED glib-2.0)
include_directories(SYSTEM "${LibXML++_INCLUDEDIR}/${LIBXML++_VERSION}" ${glib_INCLUDE_DIRS} ${LibXML++_INCLUDE_DIRS})

set(${PROJECT_NAME}_INCLUDE_DIRS ${${PROJECT_NAME}_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR}/include/)

SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib:${DOOCS_DIR}")

aux_source_directory(${CMAKE_SOURCE_DIR}/src library_sources)

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
    set_target_properties(${excutableName} PROPERTIES COMPILE_FLAGS "${ChimeraTK-DeviceAccess_CXX_FLAGS} ${DOOCS_CXX_FLAGS}")
    set_target_properties(${excutableName} PROPERTIES LINK_FLAGS "${DOOCS_LINKER_FLAGS} ${doocs-server-test-helper_LINKER_FLAGS} ${ChimeraTK-DeviceAccess_LINKER_FLAGS}")
    target_include_directories(${excutableName} PRIVATE "${CMAKE_SOURCE_DIR}/tests/DoocsDummyServer")
    target_link_libraries(${excutableName} ${doocs-server-test-helper_LIBRARIES} ${ChimeraTK-DeviceAccess_LIBRARIES} ${DOOCS_LIBRARIES} ${PROJECT_NAME})
    add_test(${excutableName} ${excutableName})
    # copy config file
    FILE( COPY ${CMAKE_SOURCE_DIR}/tests/doocsDummy_rpc_server.conf DESTINATION ${PROJECT_BINARY_DIR})
    FILE( RENAME ${PROJECT_BINARY_DIR}/doocsDummy_rpc_server.conf ${PROJECT_BINARY_DIR}/${excutableName}.conf)
  endforeach( testExecutableSrcFile )

  FILE( COPY ${CMAKE_SOURCE_DIR}/tests/dummies.dmap DESTINATION ${PROJECT_BINARY_DIR})

endif()

#The make coverage command is only available in debug mode
IF(CMAKE_BUILD_TYPE STREQUAL "Debug")
  configure_file(cmake/Makefile.coverage.in
    ${PROJECT_BINARY_DIR}/Makefile.coverage @ONLY)
  add_custom_target(coverage
    make -f Makefile.coverage
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generating test coverage documentation" VERBATIM
    )
ENDIF(CMAKE_BUILD_TYPE STREQUAL "Debug")

# C++ library
add_library(${PROJECT_NAME} SHARED ${library_sources} )
set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${${PROJECT_NAME}_FULL_LIBRARY_VERSION} SOVERSION ${${PROJECT_NAME}_SOVERSION})
set_target_properties(${PROJECT_NAME} PROPERTIES COMPILE_FLAGS "${ChimeraTK-DeviceAccess_CXX_FLAGS} ${DOOCS_CXX_FLAGS}")
set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_FLAGS "${DOOCS_LINKER_FLAGS} ${ChimeraTK-DeviceAccess_LINKER_FLAGS}")
target_link_libraries(${PROJECT_NAME}
                      PUBLIC
                        ${ChimeraTK-DeviceAccess_LIBRARIES}
                        ${DOOCS_LIBRARIES}
                        ${LibXML++_LIBRARIES}
                        ${glib_LIBRARIES})

# Enable documentation
include(cmake/enable_doxygen_documentation.cmake)

#Install the library and the executables
install(TARGETS ${PROJECT_NAME}
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION lib
  )

#Create the config files by filling the correct variables into the template (*.cmake.in).
#All variables @VARIABLE@ are replaced with the current values, but they have to be set first....
#Only the reference header is needed. The other variables are for dependencies of this library,
#but there are none.
set(${PROJECT_NAME}_INCLUDE_DIRS "${CMAKE_INSTALL_PREFIX}/include ${Boost_INCLUDE_DIRS}")
set(${PROJECT_NAME}_LIBRARIES "${PROJECT_NAME} ${ChimeraTK-DeviceAccess_LIBRARIES} ${DOOCS_LIBRARIES}")
set(${PROJECT_NAME}_LIBRARY_DIRS "${CMAKE_INSTALL_PREFIX}/lib ${ChimeraTK-DeviceAccess_LIBRARY_DIRS} ${DOOCS_LIBRARY_DIRS}")
set(${PROJECT_NAME}_CPPFLAGS "${ChimeraTK-DeviceAccess_CXX_FLAGS} ${DOOCS_CXX_FLAGS}")
set(${PROJECT_NAME}_LINKER_FLAGS "-Wl,-rpath=${CMAKE_INSTALL_PREFIX}/lib,-rpath=${DOOCS_DIR},--enable-new-dtags ${DOOCS_LINKER_FLAGS}")
include(${CMAKE_SOURCE_DIR}/cmake/create_cmake_config_files.cmake)
