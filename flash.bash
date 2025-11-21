cmake --build src/pico/cmake-build-debug --target agc_pico -j 10 \
  && picotool load -v -x -f src/pico/cmake-build-debug/agc_pico.uf2 \
  && pio device monitor