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

#include "audio_pio.h"

#include <stdlib.h>

#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "pico/stdlib.h"


/******************************************************************************
function: 16 bit unsigned audio data processing
parameter:
    audio :  16-bit audio array
    len   :  The length of the array 
return:  The address of a 32-bit array
******************************************************************************/
int32_t* data_treating(const int16_t* audio, uint32_t len)
{
  int32_t* samples = (int32_t*)calloc(len, sizeof(int32_t));
  for(uint32_t i = 0; i < len; i++)
  {
    int16_t shift_audio = audio[i] - 0x7fff;
    samples[i] = audio_format.channel_count == 1
        ? shift_audio
        : (shift_audio << 16) + shift_audio;
  }
  return samples;
}

/******************************************************************************
function: audio out
parameter:
    samples :  32-bit audio array
******************************************************************************/
void audio_out(uint32_t samples)
{
  pio_sm_put_blocking(audio_format.pio, audio_format.sm, samples);
}

/******************************************************************************
function: pio init
parameter:
******************************************************************************/
void init_audio()
{
  gpio_set_function(audio_format.audio_data, GPIO_FUNC_PIOx);
  gpio_set_function(audio_format.audio_clock, GPIO_FUNC_PIOx);
  gpio_set_function(audio_format.audio_clock + 1, GPIO_FUNC_PIOx);

  pio_sm_claim(audio_format.pio, audio_format.sm);

  uint offset = pio_add_program(audio_format.pio, &audio_pio_program);

  audio_pio_program_init(
    audio_format.pio,
    audio_format.sm,
    offset,
    audio_format.audio_data,
    audio_format.audio_clock);

  uint32_t system_clock_frequency = clock_get_hz(clk_sys);
  uint32_t divider = system_clock_frequency * 4 / (audio_format.sample_freq); // avoid arithmetic overflow
  pio_sm_set_clkdiv_int_frac(
    audio_format.pio, audio_format.sm, divider >> 8u, divider & 0xffu);

  pio_sm_set_enabled(audio_format.pio, audio_format.sm, true);


  // DMA-Kanal anfordern (ersetze PIO0 und SM0 durch deine PIO-Instanz und State-Machine aus audio_pio.c)
  audio_format.dma_chan = dma_claim_unused_channel(true);
  audio_format.dma_cfg = dma_channel_get_default_config(audio_format.dma_chan);
  channel_config_set_transfer_data_size(&audio_format.dma_cfg, DMA_SIZE_32);  // 32-Bit für Stereo-Samples
  channel_config_set_dreq(&audio_format.dma_cfg, pio_get_dreq(audio_format.pio, audio_format.sm, true));  // DREQ für PIO TX (true = TX)
  channel_config_set_read_increment(&audio_format.dma_cfg, true);  // Inkrementiere Quelladresse (Buffer)
  channel_config_set_write_increment(&audio_format.dma_cfg, false);  // Schreibe immer in dieselbe FIFO-Adresse
}

void deinit_audio()
{
  dma_channel_abort(audio_format.dma_chan);
  dma_channel_unclaim(audio_format.dma_chan);
}

void stream_audio(uint32_t* samples, uint32_t sample_count)
{
  // Warte auf DMA-Abschluss
  dma_channel_wait_for_finish_blocking(audio_format.dma_chan);

  // Ersetze die manuelle Schleife durch DMA-Übertragung
  // Konfiguriere und starte DMA
  dma_channel_configure(
    audio_format.dma_chan,
    &audio_format.dma_cfg,
    &audio_format.pio->txf[0],  // Ziel: PIO TX-FIFO (ersetze pio0 und SM 0 bei Bedarf)
    samples,        // Quelle: Dein Buffer
    sample_count,   // Anzahl der Transfers (Samples)
    true
  );          // Starte sofort

}
