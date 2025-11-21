cmake_minimum_required(VERSION 3.25)

project(VAGC-cli)

add_library(cjson STATIC ../../thirdparty/cJSON/cJSON.c)
target_include_directories(cjson PUBLIC ../../thirdparty/)


set(agc_native_src
  main.c
  dsky_output_handler.c
  agc_cli.c
  timer.c
  us_time.c
  ../core/agc_simulator.c
  ../core/agc_engine_init.c
  ../core/agc_engine.c
  ../core/agc_io_handler.c
  ../core/ringbuffer.c
  ../core/dsky.c
  ../core/dsky_dump.c
  ../core/profile.c
)

add_executable(agc_native ${agc_native_src})
target_compile_definitions(agc_native PRIVATE NVER="${NVER}" NOREADLINE="yes" -DGYRO_TIMING_SIMULATED=)
target_link_libraries(agc_native PRIVATE m cjson)
target_include_directories(agc_native PRIVATE ..)
target_include_directories(agc_native PRIVATE ${CJSON_INCLUDE_DIRS} ../../src)
