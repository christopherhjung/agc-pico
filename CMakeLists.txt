cmake_minimum_required(VERSION 3.25)

project(VAGC)

set(VAGC_src main.c agc_cli.c agc_simulator.c
 checkdec.c agc_engine_init.c agc_engine.c rfopen.c
  ringbuffer_api.c ringbuffer.c dsky.c)

add_executable(yaAGC ${VAGC_src})
target_compile_definitions(yaAGC PRIVATE NVER="${NVER}" NOREADLINE="yes")
target_link_libraries(yaAGC PRIVATE m)


install(TARGETS yaAGC)
