cmake --build ./Pico_Audio/build --target audio_firmware -j 10 \
  && picotool load -v -x -f ./Pico_Audio/build/audio_firmware.uf2 \
  && pio device monitor


  #Pico-Audio-V2/c/cmake-build-debug/audio_firmware.uf2 \
