cmake --build src/pico/cmake-build-debug --target blink -j 10 \
  && picotool load -v -x -f src/pico/cmake-build-debug/blink.uf2 \
  && pio device monitor