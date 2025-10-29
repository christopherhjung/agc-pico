cmake_minimum_required(VERSION 3.25)

project(VAGC)

set(VAGC_src
  src/cli/main.c
  src/cli/dsky_output_handler.c
  src/cli/agc_cli.c
  src/core/agc_simulator.c
  src/core/agc_engine_init.c
  src/core/agc_engine.c
  src/core/agc_io_handler.c
  src/core/ringbuffer.c
  src/core/dsky.c
)

add_executable(agc ${VAGC_src})
target_compile_definitions(agc PRIVATE NVER="${NVER}" NOREADLINE="yes" )
target_link_libraries(agc PRIVATE m)
target_include_directories(agc PRIVATE src)

install(TARGETS agc)
