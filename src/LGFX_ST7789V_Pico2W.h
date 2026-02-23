#pragma once

#include <LovyanGFX.hpp>

class LGFX : public lgfx::LGFX_Device {

  lgfx::Panel_ST7789  _panel_instance;
  lgfx::Bus_SPI       _bus_instance;

public:

  LGFX(void) {

    // -------------------------
    // SPI Bus konfigurace
    // -------------------------
    {
      auto cfg = _bus_instance.config();

      cfg.spi_host = 1;          // SPI1 (piny 14,15 patří na SPI1 u RP2040/RP2350)
      cfg.spi_mode = 0;
      cfg.freq_write = 40000000; // 40 MHz – bezpečná hodnota pro ST7789V
      cfg.freq_read  = 16000000;

      cfg.pin_sclk = 14;  // SCK
      cfg.pin_mosi = 15;  // MOSI
      cfg.pin_miso = -1;  // MISO nezapojen
      cfg.pin_dc   = 12;  // DC (Data/Command)

      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }

    // -------------------------
    // Panel (displej) konfigurace
    // -------------------------
    {
      auto cfg = _panel_instance.config();

      cfg.pin_cs   = 11;   // CS
      cfg.pin_rst  = 13;   // RST
      cfg.pin_busy = -1;   // BUSY nezapojen

      cfg.panel_width  = 240;  // šířka displeje
      cfg.panel_height = 320;  // výška displeje

      cfg.offset_x        = 0;
      cfg.offset_y        = 0;
      cfg.offset_rotation = 0;  // 0–3 normální, 4–7 otočeno o 180°

      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits  = 1;
      cfg.readable         = false; // MISO nezapojen → nelze číst

      cfg.invert           = true;  // ST7789V typicky potřebuje inverzi barev
      cfg.rgb_order        = false;
      cfg.dlen_16bit       = false;
      cfg.bus_shared       = false;

      _panel_instance.config(cfg);
    }

    setPanel(&_panel_instance);
  }
};

// Globální instance – použij v main.cpp
static LGFX tft;
