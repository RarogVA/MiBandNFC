# Два циферблата: «Классика» и «Авторские»

Состояние на 17 ноября 2026 года. Оба циферблата собраны для
**miwear.watch.p67glt, прошивка 3.201.026**, содержат одинаковые функции и
различаются только набором тем. Аппаратная приёмка **не выполнена**.

| | **NFC-метки — Классика** | **NFC-метки — Авторские** |
| --- | --- | --- |
| ID | `999999978` | `999999977` |
| Темы | Dark Turquoise, Light Sapphire, Deep Ocean, Light Turquoise | Copper, Light Emerald, Sunset Coral, Forest Sage |
| Файл | `build/nfc-tags-classic.face` | `build/nfc-tags-custom.face` |
| Размер | 3 916 939 байт | 3 901 321 байт |
| SHA-256 | `ee8ea72a26fa825d44a5c5118c7d44c2a7317b9c8696aec6346b5eb2014de4d8` | `5f48b56276cf2aac5989faf3cc8e0e45f368e37e70181a68520a6a777307fb4e` |
| Превью | [classic-preview.png](../tags/roundabout/classic-preview.png) | [custom-preview.png](../tags/roundabout/custom-preview.png) |
| Отчёт | [classic_validation.json](../tags/roundabout/classic_validation.json) | [custom_validation.json](../tags/roundabout/custom_validation.json) |

Оба циферблата — четыре обычных стиля и четыре AOD. Имена в галерее
Mi Fitness/Notify — нормальные, без технического «Roundabout RU NFC 6».

## Что общего

- Первый тап по нижнему кругу: загрузка модулей, `READY`, запись
  `/data/tag_catalog/auto_load` и перезагрузка браслета.
- После перезагрузки нижний круг работает как выбранное штатное приложение
  (то, что выбрано в Slot; на превью — Heart Rate), а модули восстанавливаются автоматически.
- Все три круга остаются редактируемыми Slot. У 43 штатных вариантов
  **нижнего** круга есть прозрачный Lua-слой; верхние два круга и AOD —
  без него.
- Меню приложений: **NFC-метки**, **Mi Home**, **Модули**.
- Mi Home: пять слотов, BLE-опрос Pixel и локальный miIO. Пиксельное
  приложение должно быть открыто и подключено.
- Иконки приложений и нижнего круга — штатные ресурсы прошивки.

## Темы

**Классика** использует основные цвета Roundabout: тёмная бирюза, светлый
сапфир, глубокий океан и светлая бирюза. **Авторские** — медный, светлый
изумруд, закатный коралл и лесной шалфей. В обеих сборках перекрашены
безeль, минутная стрелка, дорожка солнца, нижние полосы шагов/ккал, AOD и
превью; остальные ресурсы сохраняются.

Стиль 2 — полный клон светлого стиля 4 (формат держит только один светлый
базовый стиль). Это уже проверенная схема NFC 5 Four.

## Сборка

Сначала базовый установочный FACE (не устанавливать, он нужен как основа):

```sh
build/shortcut-research-venv/bin/python tags/bootstrap/build_choice_face.py \
  --circle-app findPhone --face-id 999999976 \
  --face-name "NFC-метки — Установщик" \
  --core tags/catalog/build/catalog_core_v1.elf \
  --ui tags/launcher/build/nfc_tags_catalog_ui_v4.elf \
  --output build/nfc-tags-installer-base.face \
  --report build/nfc-tags-installer-base.json
```

Затем два финальных циферблата:

```sh
BASE=build/nfc-tags-installer-base.face
SHA=$(shasum -a 256 "$BASE" | cut -d' ' -f1)
build/shortcut-research-venv/bin/python tags/roundabout/combined_face.py \
  --base-face "$BASE" --base-sha "$SHA" --palette main \
  --face-id 999999978 --face-name "NFC-метки — Классика" \
  --output build/nfc-tags-classic.face \
  --report tags/roundabout/classic_validation.json \
  --preview tags/roundabout/classic-preview.png
build/shortcut-research-venv/bin/python tags/roundabout/combined_face.py \
  --base-face "$BASE" --base-sha "$SHA" --palette custom \
  --face-id 999999977 --face-name "NFC-метки — Авторские" \
  --output build/nfc-tags-custom.face \
  --report tags/roundabout/custom_validation.json \
  --preview tags/roundabout/custom-preview.png
```

Скрипт проверяет SHA-256 базы, сохраняет все исходные ресурсы, изменяет
только разрешённые image/descriptor/preview-ресурсы и клонирует стиль 2.

## Установка

1. Передать нужный `.face` на браслет через **Notify for Xiaomi**.
2. В галерее появятся **NFC-метки — Классика** и/или
   **NFC-метки — Авторские**.
3. Выбрать циферблат, нажать нижний круг один раз и дождаться перезагрузки.
4. После перезагрузки проверить меню и штатное приложение нижнего круга.

Оба FACE можно установить одновременно: ID и имена различаются.

## Ограничения

- Аппаратная проверка установки, редактора, клона стиля 2, AOD, нижних
  полос, первого тапа и автозагрузки ещё не выполнена.
- Стиль 2 — статический клон стиля 4; правки стиля 4 после сборки в клон не
  попадут.
- «Полупрозрачность» — предварительное смешение в indexed8, не runtime-альфа.
- Превью — рендеры, не фотографии устройства.
- Если сменить нижний круг на вариант из верхних двух (без Lua), installer
  и автозагрузка для этой настройки не сработают; используйте нижний круг.
