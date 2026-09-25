#include "display.h"

#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome {
namespace gdey0266f51h {

static const char *const TAG = "gdey0266f51h";

// Нативные размеры панели (без учёта поворота)
static const int16_t WIDTH = 184;
static const int16_t HEIGHT = 360;

// Времена ожидания сигнала BUSY, мс (из GxEPD2)
static const uint32_t POWER_ON_TIME_MS = 200;
static const uint32_t POWER_OFF_TIME_MS = 100;
static const uint32_t FULL_REFRESH_TIME_MS = 25000;

float GDEY0266F51HDisplay::get_setup_priority() const { return setup_priority::PROCESSOR; }

void GDEY0266F51HDisplay::setup() {
  this->init_internal_(uint32_t(WIDTH) * uint32_t(HEIGHT) / 4u);
  this->dc_pin_->setup();
  this->dc_pin_->digital_write(false);
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();
    this->reset_pin_->digital_write(true);
  }
  if (this->busy_pin_ != nullptr) {
    this->busy_pin_->setup();
  }
  this->spi_setup();
}

void GDEY0266F51HDisplay::dump_config() {
  ESP_LOGCONFIG(TAG, "GDEY0266F51H (2.66\" 4-color e-paper)");
  ESP_LOGCONFIG(TAG, "  Dimensions: %dx%d (native)", WIDTH, HEIGHT);
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  BUSY Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

void GDEY0266F51HDisplay::update() {
  this->do_update_();
  this->display();
}

void GDEY0266F51HDisplay::display() {
  ESP_LOGD(TAG, "Display update: init...");
  this->initialize_();
  ESP_LOGD(TAG, "Display update: writing frame (%u bytes)...", uint32_t(WIDTH) * uint32_t(HEIGHT) / 4u);
  this->write_frame_();
  ESP_LOGD(TAG, "Display update: refresh...");
  this->refresh_();
  ESP_LOGD(TAG, "Display update: power off.");
  this->power_off_();
}

void HOT GDEY0266F51HDisplay::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x >= this->get_width_internal() || y >= this->get_height_internal() || x < 0 || y < 0)
    return;
  const uint8_t value = this->color_to_panel_(color);
  const uint32_t pos = (x >> 2) + uint32_t(y) * (WIDTH >> 2);
  switch (x & 3) {
    case 0:
      this->buffer_[pos] = (this->buffer_[pos] & 0x3F) | (value << 6);
      break;
    case 1:
      this->buffer_[pos] = (this->buffer_[pos] & 0xCF) | (value << 4);
      break;
    case 2:
      this->buffer_[pos] = (this->buffer_[pos] & 0xF3) | (value << 2);
      break;
    case 3:
      this->buffer_[pos] = (this->buffer_[pos] & 0xFC) | value;
      break;
  }
}

int GDEY0266F51HDisplay::get_width_internal() { return WIDTH; }

int GDEY0266F51HDisplay::get_height_internal() { return HEIGHT; }

uint8_t GDEY0266F51HDisplay::color_to_panel_(Color color) const {
  // Ближайший из четырёх цветов панели: 00=чёрный, 01=белый, 10=жёлтый, 11=красный.
  const uint8_t r = color.red, g = color.green, b = color.blue;
  const uint32_t d_black = uint32_t(r) * r + uint32_t(g) * g + uint32_t(b) * b;
  const uint32_t d_white =
      uint32_t(255 - r) * (255 - r) + uint32_t(255 - g) * (255 - g) + uint32_t(255 - b) * (255 - b);
  const uint32_t d_red = uint32_t(255 - r) * (255 - r) + uint32_t(g) * g + uint32_t(b) * b;
  const uint32_t d_yellow =
      uint32_t(255 - r) * (255 - r) + uint32_t(255 - g) * (255 - g) + uint32_t(b) * b;

  uint8_t value = 0x01;  // белый
  uint32_t d_min = d_white;
  if (d_black < d_min) {
    d_min = d_black;
    value = 0x00;
  }
  if (d_red < d_min) {
    d_min = d_red;
    value = 0x03;
  }
  if (d_yellow < d_min) {
    value = 0x02;
  }
  return value;
}

void GDEY0266F51HDisplay::command_(uint8_t value) {
  this->dc_pin_->digital_write(false);
  this->enable();
  this->write_byte(value);
  this->disable();
}

void GDEY0266F51HDisplay::data_(uint8_t value) {
  this->dc_pin_->digital_write(true);
  this->enable();
  this->write_byte(value);
  this->disable();
}

void GDEY0266F51HDisplay::reset_() {
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->digital_write(true);
    delay(20);
    this->reset_pin_->digital_write(false);
    delay(2);
    this->reset_pin_->digital_write(true);
    delay(20);  // как в демо Waveshare epd2in66g
  }
  this->wait_until_idle_(POWER_ON_TIME_MS);
}

void GDEY0266F51HDisplay::initialize_() {
  // Последовательность инициализации из GxEPD2 (Good Display demo для JD79667).
  this->reset_();
  this->command_(0x4D);
  this->data_(0x78);
  this->command_(0x00);  // PSR
  this->data_(0x0F);
  this->data_(0x29);
  this->command_(0x01);  // PWRR
  this->data_(0x07);
  this->data_(0x00);
  this->command_(0x03);  // POFS
  this->data_(0x10);
  this->data_(0x54);
  this->data_(0x44);
  this->command_(0x06);  // BTST (Booster Soft Start)
  this->data_(0x05);
  this->data_(0x00);
  this->data_(0x3F);
  this->data_(0x0A);
  this->data_(0x25);
  this->data_(0x12);
  this->data_(0x1A);
  this->command_(0x50);  // CDI
  this->data_(0x37);
  this->command_(0x60);  // TCON
  this->data_(0x02);
  this->data_(0x02);
  this->command_(0x61);  // TRES
  this->data_(WIDTH / 256);
  this->data_(WIDTH % 256);
  this->data_(HEIGHT / 256);
  this->data_(HEIGHT % 256);
  this->command_(0xE7);
  this->data_(0x1C);
  this->command_(0xE3);  // PWS
  this->data_(0x22);
  this->command_(0xB4);  // LVD
  this->data_(0xD0);
  this->command_(0xB5);
  this->data_(0x03);
  this->command_(0xE9);
  this->data_(0x01);
  this->command_(0x30);  // PLL
  this->data_(0x08);
  // Включаем питание панели
  this->command_(0x04);
  this->wait_until_idle_(POWER_ON_TIME_MS);
}

void GDEY0266F51HDisplay::write_frame_() {
  // Окно обновления (0x83). ВАЖНО: последний байт 0x01 (partial mode),
  // как в GxEPD2 (_setPartialRamArea с partial_mode = true). С 0x00
  // панель адресует данные иначе и экран получается «рябым».
  this->command_(0x83);
  this->data_(0x00);                        // X start
  this->data_(0x00);
  this->data_((WIDTH - 1) / 256);           // X end
  this->data_((WIDTH - 1) % 256);
  this->data_(0x00);                        // Y start
  this->data_(0x00);
  this->data_((HEIGHT - 1) / 256);          // Y end
  this->data_((HEIGHT - 1) % 256);
  this->data_(0x01);                        // partial mode (как в GxEPD2)
  // Пишем данные кадра (2 бита на пиксель)
  this->dc_pin_->digital_write(true);
  this->enable();
  this->write_array(this->buffer_, uint32_t(WIDTH) * uint32_t(HEIGHT) / 4u);
  this->disable();
}

void GDEY0266F51HDisplay::refresh_() {
  this->command_(0x50);  // VCOM and Data Interval Setting
  this->data_(0x37);     // белая рамка (по умолчанию)
  this->command_(0x12);  // Display Refresh
  this->data_(0x00);
  delay(1);
  this->wait_until_idle_(FULL_REFRESH_TIME_MS);
}

void GDEY0266F51HDisplay::power_off_() {
  this->command_(0x02);
  this->data_(0x00);
  this->wait_until_idle_(POWER_OFF_TIME_MS);
  // Глубокий сон до следующего аппаратного сброса
  this->command_(0x07);
  this->data_(0xA5);
}

void GDEY0266F51HDisplay::wait_until_idle_(uint32_t timeout_ms) {
  if (this->busy_pin_ == nullptr) {
    delay(timeout_ms);
    return;
  }
  const uint32_t start = millis();
  // Ждём, пока BUSY активируется (небольшое окно ~100 мс).
  // Если панель так и не выставила BUSY - либо сигнал не доходит,
  // либо полярность busy_pin задана неверно. В этом случае ждём
  // фиксированное время - обновление всё равно успеет завершиться.
  bool asserted = false;
  while (millis() - start < 100) {
    if (this->busy_pin_->digital_read()) {
      asserted = true;
      break;
    }
    delay(1);
  }
  if (!asserted) {
    ESP_LOGW(TAG, "BUSY не активируется (полярность сигнала?) - ждём фиксированные %u мс", timeout_ms);
    delay(timeout_ms);
    return;
  }
  // Ждём завершения операции (снятия BUSY)
  while (this->busy_pin_->digital_read()) {
    if (millis() - start > timeout_ms) {
      ESP_LOGW(TAG, "Timeout while displaying image! (%u мс)", timeout_ms);
      return;
    }
    App.feed_wdt();
    delay(10);
  }
  ESP_LOGD(TAG, "BUSY занятость: %u мс", millis() - start);
}

}  // namespace gdey0266f51h
}  // namespace esphome
