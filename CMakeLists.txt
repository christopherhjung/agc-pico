cmake_minimum_required(VERSION 3.25)

project(VAGC)

set(_libagc_src agc_engine_init.c agc_engine.c agc_utilities.c rfopen.c
Backtrace.c SocketAPI.c DecodeDigitalDownlink.c)

set(_agc_src main.c agc_cli.c agc_simulator.c agc_debugger.c
agc_gdbmi.c agc_disassembler.c agc_help.c nbfgets.c agc_symtab.c
NormalizeSourceName.c checkdec.c Backtrace.c)

add_library(libyaAGC ${_libagc_src})

add_executable(yaAGC ${_agc_src})
target_compile_definitions(yaAGC PRIVATE NVER="${NVER}" NOREADLINE="yes" GDBMI)
target_link_libraries(yaAGC PRIVATE libyaAGC m)
if(Threads_FOUND)
  target_link_libraries(yaAGC PRIVATE Threads::Threads)
endif()

add_library(regex regex.c)
if(WIN32)
  target_sources(yaAGC PRIVATE random.c)
  target_link_libraries(yaAGC PRIVATE regex ${WINSOCK_LIBRARIES})
endif()

if(APPLE OR WIN32)
  target_compile_definitions(regex PRIVATE STDC_HEADERS)
  target_compile_definitions(yaAGC PRIVATE STDC_HEADERS)
endif()



install(TARGETS yaAGC)
