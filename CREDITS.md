# 🙏 Благодарности и лицензии сторонних компонентов

> **Навигация:** [← На главную](ReadMe.md)

Проект не появился бы без чужих открытых наработок. Ниже — что и откуда взято,
чтобы было понятно, кому и за что спасибо.

---

## Сторонние проекты

| Проект | Автор | Лицензия | Где используется |
|---|---|---|---|
| [**GxEPD2**](https://github.com/ZinggJM/GxEPD2) | Jean-Marc Zingg (ZinggJM) | GPL-3.0 | Arduino-версия: вывод картинки на e-paper. Драйвер `gdey0266f51h` для ESPHome — **порт** класса `GxEPD2_266c_GDEY0266F51H` (последовательности инициализации и работа с 4-цветными панелями) |
| [**GyverBME280**](https://github.com/GyverLibs/GyverBME280) | AlexGyver (GyverLibs) | MIT | Arduino-версия: датчик BME280 |
| [**ESPHome**](https://esphome.io/) | ESPHome / Nabu Casa | GPL-3.0 | Домашняя версия: вся прошивка и внешние компоненты |
| [**GNU FreeFont**](https://www.gnu.org/software/freefont/) (FreeSans Bold) | GNU Project | GPL-3.0+ с Font Exception | Шрифты на экране: `ESP32-C3/fonts/`, `ForESPHome/fonts/FreeSansBold.ttf`. Растровые версии сделаны на [truetype2gfx](https://rop.nl/truetype2gfx/), иконки — в программе Img2Lcd |
| Демо-примеры Waveshare / Good Display для JD79667 | Waveshare / Good Display | — | Использованы как справка по регистрам панели GDEY0266F51H |
| [**Altium Designer**](https://www.altium.com/) | Altium | — | Схема и разводка платы (`Altium/`). Компоненты — из библиотеки автора (`AltiumDesignerLibrary`) |
| [**FreeCAD**](https://www.freecad.org/) | FreeCAD Community | LGPL-2.1 | 3D-модели платы и корпуса (`CAD/`) |
| [**LightBurn**](https://lightburnsoftware.com/) | LightBurn Software | — | Проект лазерной резки корпуса (`CAD/PlateLaser.lbrn2`) |

Даташиты на компоненты лежат в `DOC/`: [DS1307Z](DOC/ds1307.pdf), [TPL5110](DOC/tpl5110.pdf).

---

## Лицензия этого проекта

Всё, что написано автором (прошивка `ESP32-C3/`, конфигурация и компоненты
`ForESPHome/`, скрипты, CAD-модели, документация), распространяется по
лицензии **MIT** — см. [LICENSE](LICENSE). Коротко: можно использовать,
менять, продавать и встраивать куда угодно, главное — сохранить упоминание
авторства и не предъявлять претензий, если что-то сломалось.

**Исключение** — производные от GPL-компонентов:

- `ForESPHome/components/gdey0266f51h/` — производная работа от GxEPD2
  (GPL-3.0), поэтому распространяется под **GPL-3.0**
  (текст: <https://www.gnu.org/licenses/gpl-3.0.txt>). Если вы используете или
  распространяете этот компонент, действуют условия GPL-3.0;
- шрифты `ESP32-C3/fonts/*.h` и `ForESPHome/fonts/FreeSansBold.ttf` — GNU FreeFont
  под GPL-3.0+ с Font Exception: встраивание шрифта в изображение или документ
  не делает их производной работой, но при распространении самого файла шрифта
  действует GPL.

Отдельно: Arduino-версия прошивки собирается с библиотекой GxEPD2 (GPL-3.0),
поэтому **собранный бинарник** — уже под GPL-3.0, даже несмотря на MIT у
исходного скетча. Для домашнего хобби-проекта это никак не мешает.

---

## Навигация

[← На главную](ReadMe.md)
