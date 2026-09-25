#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Конвертер иконок ClimaticDisplay из формата GxEPD2/Img2Lcd (.h, 1 bpp)
в BMP-файлы для ESPHome.

Правила конвертации (совпадают с тем, как картинки рисовались в Arduino-прошивке
через Adafruit_GFX::drawBitmap):
  * бит = 1 -> чернила (рисуются цветом слоя), бит = 0 -> белый фон;
  * слои накладываются по порядку, последний слой - поверх;
  * в Arduino экран предварительно заливался белым, поэтому фон везде белый.

На выходе - 16-битные BMP (RGB565), которые ESPHome встраивает как
type: RGB565. Соответствие цветов:
  чёрный 0x0000, белый 0xFFFF, красный 0xF800, жёлтый 0xFFE0.

Запуск: python tools/convert_icons.py
"""

import re
import struct
import sys
from pathlib import Path

SRC = Path(__file__).resolve().parent.parent.parent / "ESP32-C3" / "img"
DST = Path(__file__).resolve().parent.parent / "img"

BLACK = 0x0000
WHITE = 0xFFFF
RED = 0xF800
YELLOW = 0xFFE0

# Слои для каждой итоговой картинки (имена .h без расширения)
# Формат: выходное_имя -> [(исходный .h, цвет слоя), ...]
COMPOSITIONS = {
    "arrow_up": [("Up", BLACK)],
    "arrow_down": [("Down", BLACK)],
    "thermo_bw": [("TermometrGen", BLACK)],
    "thermo_min": [("TermometrGen", BLACK), ("TermometrMinRED", RED)],
    "thermo_norm": [("TermometrGen", BLACK), ("TermometrNormRED", RED)],
    "thermo_max": [("TermometrGen", BLACK), ("TermometrMaxRED", RED)],
    "humidity_bw": [("HumidityBW", BLACK)],
    "humidity_norm": [("HumidityBW", BLACK), ("HumidityRY", YELLOW)],
    "humidity_max": [("HumidityBW", BLACK), ("HumidityRY", RED)],
    "pressure_bw": [("PressureNormBW", BLACK)],
    "pressure_min": [
        ("PressureMinBW", BLACK),
        ("PressureMinNormR", RED),
        ("PressureMinMaxY", YELLOW),
    ],
    "pressure_norm": [
        ("PressureNormBW", BLACK),
        ("PressureMinNormR", RED),
        ("PressureNormY", YELLOW),
    ],
    "pressure_max": [
        ("PressureMaxBW", BLACK),
        ("PressureMaxR", RED),
        ("PressureMinMaxY", YELLOW),
    ],
    "battery_full": [
        ("BatteryGenY", YELLOW),
        ("BatteryGenBW", BLACK),
        ("BatteryMaxBW", BLACK),
    ],
    "battery_half": [
        ("BatteryGenY", YELLOW),
        ("BatteryGenBW", BLACK),
        ("BatteryHalfR", RED),
    ],
    "battery_min": [
        ("BatteryGenY", YELLOW),
        ("BatteryGenBW", BLACK),
        ("BatteryMinR", RED),
    ],
}


def parse_h(path: Path):
    """Разбирает .h файл Img2Lcd, возвращает (ширина, высота, список байт)."""
    text = path.read_text(encoding="utf-8", errors="replace")
    # Заголовок: /* 0X81,0X01,0X<H_lo>,0X<H_hi>,0X<W_lo>,0X<W_hi>, */
    m = re.search(
        r"/\*\s*0X[0-9A-Fa-f]+,\s*0X[0-9A-Fa-f]+,\s*"
        r"0X([0-9A-Fa-f]+),\s*0X([0-9A-Fa-f]+),\s*"
        r"0X([0-9A-Fa-f]+),\s*0X([0-9A-Fa-f]+)",
        text,
    )
    if not m:
        raise ValueError(f"Не найден заголовок Img2Lcd в {path.name}")
    h_lo, h_hi, w_lo, w_hi = (int(g, 16) for g in m.groups())
    height = h_lo | (h_hi << 8)
    width = w_lo | (w_hi << 8)
    # Тело файла без комментария
    body = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    data = [int(x, 16) for x in re.findall(r"0X[0-9A-Fa-f]+", body)]
    expected = width * height // 8
    if len(data) != expected:
        raise ValueError(
            f"{path.name}: ожидалось {expected} байт данных, получено {len(data)}"
        )
    return width, height, data


def compose(width, height, layers):
    """Накладывает слои (1bpp, бит=1 -> цвет слоя) на белый фон."""
    pixels = [WHITE] * (width * height)
    for data, color in layers:
        for i, byte in enumerate(data):
            for bit in range(8):
                if (byte >> (7 - bit)) & 1:
                    pixels[i * 8 + bit] = color
    return pixels


def write_bmp_rgb565(path: Path, width, height, pixels):
    """Пишет 16-битный BMP (RGB565, битовые маски 5-6-5, снизу вверх)."""
    row_size = (width * 2 + 3) & ~3
    pixel_data_size = row_size * height
    file_size = 14 + 40 + 12 + pixel_data_size
    with open(path, "wb") as f:
        # BITMAPFILEHEADER
        f.write(b"BM")
        f.write(struct.pack("<IHHI", file_size, 0, 0, 14 + 40 + 12))
        # BITMAPINFOHEADER (biCompression = 3, BI_BITFIELDS)
        f.write(
            struct.pack(
                "<IiiHHIIiiII", 40, width, height, 1, 16, 3, pixel_data_size, 2835, 2835, 0, 0
            )
        )
        # Маски каналов RGB565
        f.write(struct.pack("<III", 0xF800, 0x07E0, 0x001F))
        # Пиксели: BMP хранит строки снизу вверх
        for y in range(height - 1, -1, -1):
            row = b"".join(
                struct.pack("<H", pixels[y * width + x]) for x in range(width)
            )
            f.write(row + b"\x00" * (row_size - len(row)))


def main():
    cache = {}

    def load(name):
        if name not in cache:
            cache[name] = parse_h(SRC / f"{name}.h")
        return cache[name]

    DST.mkdir(parents=True, exist_ok=True)
    for out_name, layers in COMPOSITIONS.items():
        # Все слои одной картинки должны иметь одинаковый размер
        w, h, _ = load(layers[0][0])
        resolved = [(load(n)[2], c) for n, c in layers]
        for (data, _), _ in zip(resolved, layers):
            if len(data) != w * h // 8:
                raise ValueError(f"Размер слоя не совпадает для {out_name}")
        pixels = compose(w, h, resolved)
        out_path = DST / f"{out_name}.bmp"
        write_bmp_rgb565(out_path, w, h, pixels)
        print(f"  {out_name}.bmp  {w}x{h}  <- {[n for n, _ in layers]}")

    print(f"Готово. BMP-файлы сохранены в {DST}")


if __name__ == "__main__":
    sys.exit(main())
