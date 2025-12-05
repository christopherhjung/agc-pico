/*****************************************************************************
* |	This version:   V1.0
* | Date        :   2021-04-20
* | Info        :   
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documnetation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to  whom the Software is
# furished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS OR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#
******************************************************************************/

#ifndef _PICO_AUDIO_PIO_H
#define _PICO_AUDIO_PIO_H

#include "audio_pio.h"
#include "audio_pio.pio.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "pico/stdlib.h"

#define PICO_AUDIO_FREQ 44100
#define PICO_AUDIO_COUNT 2
#define PICO_AUDIO_DATA_PIN 26
#define PICO_AUDIO_CLOCK_PIN_BASE 27
#define PICO_AUDIO_PIO 0
#define PICO_AUDIO_SM 0

#define AUDIO_PIO __CONCAT(pio, PICO_AUDIO_PIO)
#define GPIO_FUNC_PIOx __CONCAT(GPIO_FUNC_PIO, PICO_AUDIO_PIO)

// Audio format definition
typedef struct audio_format
{
  uint32_t sample_freq;
  uint16_t channel_count;
  uint8_t  audio_data;
  uint8_t  audio_clock;
  PIO      pio;
  uint8_t  sm;
  int      dma_chan;
  dma_channel_config dma_cfg;
} audio_format_t;

static audio_format_t audio_format = {
  .sample_freq   = PICO_AUDIO_FREQ,
  .channel_count = PICO_AUDIO_COUNT,
  .audio_data    = PICO_AUDIO_DATA_PIN,
  .audio_clock   = PICO_AUDIO_CLOCK_PIN_BASE,
  .pio           = AUDIO_PIO,
  .sm            = PICO_AUDIO_SM,
};

void     init_audio();
void     deinit_audio();
void     stream_audio(uint32_t* samples, uint32_t sample_count);

void     audio_out(uint32_t sample);



#endif //_PICO_AUDIO_PIO_H
