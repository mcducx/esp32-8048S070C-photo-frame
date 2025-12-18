# esp32-8048S070C-photo-frame
=======
# ESP32 Portrait Photo Frame

Простая цифровая фоторамка на ESP32 с дисплеем 800×480, работающая в портретном режиме (480×800) с автоматическим слайд-шоу.

## ✨ Основные особенности

- **Портретный режим**: Дисплей повернут на 90° (480×800 пикселей)
- **Автоматическое слайд-шоу**: Смена изображений каждые 10 секунд
- **Центрирование изображений**: Все фото всегда по центру экрана
- **Минималистичный интерфейс**: Нет лишних элементов, только изображения
- **Поддержка SD карты**: Чтение JPEG файлов с карты памяти

## 📋 Требования

### Аппаратное обеспечение
- Плата ESP32-8048S070C
- Дисплей 800×480 с RGB интерфейсом
- Сенсорный экран GT911 (опционально, не используется в текущей версии)
- SD карта (формат FAT32)

### Программное обеспечение
- PlatformIO
- Arduino framework
- Библиотеки (указываются в `platformio.ini`)

## ⚙️ Настройка

1. **Подготовка SD карты**:
   - Отформатируйте в FAT32
   - Воспользуйтесь моим конвертером https://github.com/mcducx/converter-photo-esp32-8048S070C-photo-frame
   - Добавьте JPEG файлы в корень
   - Оптимальный размер изображений: 480×800 пикселей

2. **Настройка параметров**:
   - Интервал слайд-шоу: измените `SLIDESHOW_INTERVAL` в `main.cpp`
   - Ориентация дисплея: `gfx.setRotation(1)` в `setup_display()`

## 🚀 Сборка и загрузка

1. Клонируйте репозиторий
2. Откройте проект в PlatformIO
3. Подключите ESP32 к компьютеру
4. Выполните:
```bash
pio run --target upload
```
5. Вставьте SD карту с изображениями
6. Перезагрузите устройство

## 📁 Структура проекта

```
src/
├── main.cpp          # Основная логика слайд-шоу
├── display.cpp       # Драйвер дисплея и LVGL
├── display.h         # Заголовочный файл дисплея
└── config.h          # Конфигурация пинов
platformio.ini        # Конфигурация PlatformIO
```

## 🛠️ Используемые библиотеки

- **LVGL** (v9.2.2): Графическая библиотека
- **Arduino_GFX**: Драйвер дисплея
- **TJpg_Decoder**: Декодер JPEG
- **SD**: Работа с SD картой
- **TAMC_GT911**: Драйвер тачскрина

## 📄 Лицензия

MIT License

## 🙏 Благодарности

Проект основан на библиотеках Arduino_GFX и LVGL

# Resolving the Build Error

Add board to PlatformIO https://github.com/rzeldent/platformio-espressif32-sunton

When you first compile the project, you may encounter the following error:  
`.pio/libdeps/esp32-8048S070C/lvgl/src/lv_conf_internal.h:60:18: fatal error: ../../lv_conf.h: No such file or directory`

#### How to Fix

1. Navigate to the folder `.pio/libdeps/esp32-8048S070C/lvgl`.
2. Locate the file named `lv_conf_template.h`.
3. Copy this file to the parent directory: `.pio/libdeps/esp32s3box`.
4. Rename the copied file to `lv_conf.h`.

After completing these steps, you should have the file located at:  
`.pio/libdeps/esp32-8048S070C/lv_conf.h`  

