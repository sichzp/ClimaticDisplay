#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/display/display_buffer.h"
#include "esphome/components/spi/spi.h"

namespace esphome {
namespace gdey0266f51h {

/**
 * @brief Драйвер 4-цветного e-paper дисплея Good Display GDEY0266F51H.
 *
 * 2.66", 184x360, контроллер JD79667. Код портирован из библиотеки GxEPD2
 * (класс GxEPD2_266c_GDEY0266F51H, https://github.com/ZinggJM/GxEPD2).
 * Как производная работа от GxEPD2 распространяется под GPL-3.0
 * (см. CREDITS.md в корне репозитория).
 *
 * Кодировка пикселей в буфере - 2 бита на пиксель, 4 пикселя на байт,
 * старшие биты - левый пиксель группы:
 *   00 - чёрный, 01 - белый, 10 - жёлтый, 11 - красный.
 *
 * Сигнал BUSY у панели активен НИЗКИМ уровнем, поэтому в конфигурации
 * busy_pin должен быть задан с inverted: true. Если полярность указана
 * неверно, драйвер заметит, что BUSY не активируется, и дождётся
 * фиксированного времени обновления (экран обновится в любом случае).
 */
class GDEY0266F51HDisplay : public display::DisplayBuffer,
                            public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                                  spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_2MHZ> {
 public:
  void set_dc_pin(GPIOPin *dc_pin) { this->dc_pin_ = dc_pin; }
  void set_reset_pin(GPIOPin *reset_pin) { this->reset_pin_ = reset_pin; }
  void set_busy_pin(GPIOPin *busy_pin) { this->busy_pin_ = busy_pin; }

  float get_setup_priority() const override;

  display::DisplayType get_display_type() override {
    return display::DisplayType::DISPLAY_TYPE_COLOR;
  }

  void setup() override;
  void update() override;
  void dump_config() override;

  /// Вывести содержимое буфера на панель (полное обновление экрана).
  void display();

 protected:
  void draw_absolute_pixel_internal(int x, int y, Color color) override;
  int get_width_internal() override;
  int get_height_internal() override;

  uint8_t color_to_panel_(Color color) const;

  void command_(uint8_t value);
  void data_(uint8_t value);
  void reset_();
  void initialize_();
  void write_frame_();
  void refresh_();
  void power_off_();
  void wait_until_idle_(uint32_t timeout_ms);

  GPIOPin *dc_pin_{nullptr};
  GPIOPin *reset_pin_{nullptr};
  GPIOPin *busy_pin_{nullptr};
};

}  // namespace gdey0266f51h
}  // namespace esphome
