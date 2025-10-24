cmake_minimum_required(VERSION 3.25)

project(VAGC)

set(VAGC_src
  src/main.c
  src/agc_cli.c
  src/agc_simulator.c
  src/agc_engine_init.c
  src/agc_engine.c
  src/rfopen.c
  src/ringbuffer_api.c
  src/ringbuffer.c
  src/dsky.c
)

add_executable(yaAGC ${VAGC_src})
target_compile_definitions(yaAGC PRIVATE NVER="${NVER}" NOREADLINE="yes")
target_link_libraries(yaAGC PRIVATE m)


install(TARGETS yaAGC)
