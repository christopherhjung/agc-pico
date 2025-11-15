cmake_minimum_required(VERSION 3.25)

project(VAGC-cli)

add_library(cjson STATIC ../../thirdparty/cJSON/cJSON.c)
target_include_directories(cjson PUBLIC ../../thirdparty/)


set(VAGC_src
  main.c
  dsky_output_handler.c
  agc_cli.c
  ../core/agc_simulator.c
  ../core/agc_engine_init.c
  ../core/agc_engine.c
  ../core/agc_io_handler.c
  ../core/ringbuffer.c
  ../core/dsky.c
  ../core/dsky_dump.c
  ../core/profile.c
)

add_executable(agc ${VAGC_src})
target_compile_definitions(agc PRIVATE NVER="${NVER}" NOREADLINE="yes" -DGYRO_TIMING_SIMULATED=)
target_link_libraries(agc PRIVATE m cjson)
target_include_directories(agc PRIVATE ${CJSON_INCLUDE_DIRS} ../../src)
install(TARGETS agc)
