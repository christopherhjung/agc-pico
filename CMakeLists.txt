cmake_minimum_required(VERSION 3.25)

project(VAGC)

set(VAGC_src
  src/cli/main.c
  src/cli/agc_cli.c
  src/core/agc_simulator.c
  src/core/agc_engine_init.c
  src/core/agc_engine.c
  src/core/rfopen.c
  src/core/ringbuffer_api.c
  src/core/ringbuffer.c
  src/core/dsky.c
)

add_executable(yaAGC ${VAGC_src})
target_compile_definitions(yaAGC PRIVATE NVER="${NVER}" NOREADLINE="yes")
target_link_libraries(yaAGC PRIVATE m)
target_include_directories(yaAGC PRIVATE src)

install(TARGETS yaAGC)
