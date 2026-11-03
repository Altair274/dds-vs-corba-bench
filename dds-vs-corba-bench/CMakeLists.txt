cmake_minimum_required(VERSION 3.16)
project(dds_vs_corba_bench LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(BUILD_DDS   "Build the Cyclone DDS examples and benchmark" ON)
option(BUILD_CORBA "Build the omniORB benchmark"                   ON)

set(COMMON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/common)

# ---------------------------------------------------------------- DDS
if(BUILD_DDS)
  find_package(CycloneDDS REQUIRED)

  idlc_generate(TARGET radio_status_types FILES ${CMAKE_CURRENT_SOURCE_DIR}/idl/dds/RadioStatus.idl)
  idlc_generate(TARGET bench_dds_types    FILES ${CMAKE_CURRENT_SOURCE_DIR}/idl/dds/Bench.idl)

  add_executable(status_pub src/hello/status_pub.c)
  add_executable(status_sub src/hello/status_sub.c)
  target_link_libraries(status_pub PRIVATE radio_status_types CycloneDDS::ddsc)
  target_link_libraries(status_sub PRIVATE radio_status_types CycloneDDS::ddsc)

  add_executable(dds_pingpong src/dds/dds_pingpong.c)
  target_include_directories(dds_pingpong PRIVATE ${COMMON_DIR})
  target_link_libraries(dds_pingpong PRIVATE bench_dds_types CycloneDDS::ddsc)
endif()

# ---------------------------------------------------------------- CORBA
if(BUILD_CORBA)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(OMNIORB REQUIRED IMPORTED_TARGET omniORB4)
  find_program(OMNIIDL omniidl REQUIRED)

  set(CORBA_GEN ${CMAKE_CURRENT_BINARY_DIR}/corba_gen)
  set(CORBA_IDL ${CMAKE_CURRENT_SOURCE_DIR}/idl/corba/Bench.idl)
  file(MAKE_DIRECTORY ${CORBA_GEN})
  add_custom_command(
    OUTPUT  ${CORBA_GEN}/Bench.hh ${CORBA_GEN}/BenchSK.cc
    COMMAND ${OMNIIDL} -bcxx -C ${CORBA_GEN} ${CORBA_IDL}
    DEPENDS ${CORBA_IDL}
    COMMENT "omniidl: generating C++ stubs and skeletons from Bench.idl")

  add_library(bench_corba_stubs STATIC ${CORBA_GEN}/BenchSK.cc)
  target_include_directories(bench_corba_stubs PUBLIC ${CORBA_GEN})
  target_link_libraries(bench_corba_stubs PUBLIC PkgConfig::OMNIORB)

  add_executable(corba_server src/corba/corba_server.cpp)
  add_executable(corba_client src/corba/corba_client.cpp)
  target_include_directories(corba_client PRIVATE ${COMMON_DIR})
  target_link_libraries(corba_server PRIVATE bench_corba_stubs)
  target_link_libraries(corba_client PRIVATE bench_corba_stubs)
endif()
