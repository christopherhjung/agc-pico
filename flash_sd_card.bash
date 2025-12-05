cmake --build build --target my_sd_card -j 10 \
  && picotool load -v -x -f build/src/pico/my_sd_card.uf2 \
  && pio device monitor