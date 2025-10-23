cmake_minimum_required(VERSION 3.25)

project(VAGC)

set(_libagc_src agc_engine_init.c agc_engine.c agc_utilities.c rfopen.c
SocketAPI.c DecodeDigitalDownlink.c)

set(_agc_src main.c agc_cli.c agc_simulator.c agc_help.c nbfgets.c
NormalizeSourceName.c checkdec.c)

add_library(libyaAGC ${_libagc_src})

add_executable(yaAGC ${_agc_src})
target_compile_definitions(yaAGC PRIVATE NVER="${NVER}" NOREADLINE="yes")
target_link_libraries(yaAGC PRIVATE libyaAGC m)

add_library(regex regex.c)



install(TARGETS yaAGC)
