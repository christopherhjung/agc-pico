cmake --build src/pico/cmake-build-debug --target sd_card -j 10 \
  && picotool load -v -x -f src/pico/cmake-build-debug/sd_card.uf2 \
  && pio device monitor