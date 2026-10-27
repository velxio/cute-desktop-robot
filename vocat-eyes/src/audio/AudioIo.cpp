#include "AudioIo.h"

#include <Arduino.h>

#include "../../Config.h"

#ifdef EMULATOR
bool AudioIo::begin() {
  ok_ = true;
  return true;
}
size_t AudioIo::read4(int16_t *, size_t) {
  delay(20);
  return 0;
}
size_t AudioIo::writeMono(const int16_t *, size_t n) {
  delay(20);
  return n;
}

#else
#include <driver/i2s_tdm.h>

namespace {
i2s_chan_handle_t txChan = nullptr, rxChan = nullptr;
int16_t frame4[4 * 320];
}  // namespace

bool AudioIo::begin() {
  i2s_chan_config_t chan = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  chan.auto_clear = true;  // silence, not the last buffer again, when nothing is written
  chan.dma_desc_num = 6;
  chan.dma_frame_num = 320;
  if (i2s_new_channel(&chan, &txChan, &rxChan) != ESP_OK) return false;

  // Both directions use the same TDM frame (4 slots of 16 bit, BCLK = 64 x fs) so that the
  // shared BCLK/WS pins carry one clock. The ES8311 reads the first 16 bits of each half
  // frame (slots 0 and 1 carry the sample), the ES7210 fills the four slots with its ADCs.
  i2s_tdm_config_t tdm = {};
  i2s_tdm_clk_config_t tdmClk = I2S_TDM_CLK_DEFAULT_CONFIG(AUDIO_RATE);
  i2s_tdm_slot_config_t tdmSlot = I2S_TDM_PHILIPS_SLOT_DEFAULT_CONFIG(
      I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO,
      (i2s_tdm_slot_mask_t)(I2S_TDM_SLOT0 | I2S_TDM_SLOT1 | I2S_TDM_SLOT2 | I2S_TDM_SLOT3));
  tdm.clk_cfg = tdmClk;
  tdm.slot_cfg = tdmSlot;
  tdm.gpio_cfg.mclk = (gpio_num_t)PIN_I2S_MCLK;
  tdm.gpio_cfg.bclk = (gpio_num_t)PIN_I2S_BCLK;
  tdm.gpio_cfg.ws = (gpio_num_t)PIN_I2S_WS;
  tdm.gpio_cfg.dout = (gpio_num_t)PIN_I2S_DOUT;
  tdm.gpio_cfg.din = I2S_GPIO_UNUSED;
  i2s_tdm_config_t tdmRx = tdm;
  tdmRx.gpio_cfg.dout = I2S_GPIO_UNUSED;
  tdmRx.gpio_cfg.din = (gpio_num_t)PIN_I2S_DIN;

  if (i2s_channel_init_tdm_mode(txChan, &tdm) != ESP_OK) return false;
  if (i2s_channel_init_tdm_mode(rxChan, &tdmRx) != ESP_OK) return false;
  if (i2s_channel_enable(txChan) != ESP_OK) return false;
  if (i2s_channel_enable(rxChan) != ESP_OK) return false;
  ok_ = true;
  return true;
}

size_t AudioIo::read4(int16_t *out, size_t frames) {
  if (!ok_) return 0;
  size_t got = 0;
  i2s_channel_read(rxChan, out, frames * 4 * sizeof(int16_t), &got, portMAX_DELAY);
  return got / (4 * sizeof(int16_t));
}

size_t AudioIo::writeMono(const int16_t *mono, size_t n) {
  if (!ok_) return 0;
  size_t done = 0;
  while (done < n) {
    size_t k = n - done;
    if (k > 320) k = 320;
    for (size_t i = 0; i < k; i++) {
      frame4[4 * i] = frame4[4 * i + 1] = mono[done + i];
      frame4[4 * i + 2] = frame4[4 * i + 3] = 0;
    }
    size_t wrote = 0;
    i2s_channel_write(txChan, frame4, k * 4 * sizeof(int16_t), &wrote, portMAX_DELAY);
    done += k;
  }
  return done;
}
#endif
