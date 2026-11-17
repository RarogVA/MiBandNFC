# Три циферблата: «Классика», «Авторские», «Оригинал»

Состояние на 17 ноября 2026 года. Все три собраны для
**miwear.watch.p67glt, прошивка 3.201.026**, содержат одинаковые функции и
различаются только темами. Аппаратная приёмка **не выполнена**.

| | **NFC-метки — Классика** | **NFC-метки — Авторские** | **NFC-метки — Оригинал** |
| --- | --- | --- | --- |
| ID | `999999978` | `999999977` | `999999975` |
| Темы | Dark Turquoise, Light Sapphire, Deep Ocean, Light Turquoise | Copper, Light Emerald, Sunset Coral, Forest Sage | оригинальные Roundabout, стили 1–4 без перекраски |
| Файл | `build/nfc-tags-classic.face` | `build/nfc-tags-custom.face` | `build/nfc-tags-original.face` |
| Размер | 3 916 939 байт | 3 901 321 байт | 3 422 055 байт |
| SHA-256 | `ee8ea72a26fa825d44a5c5118c7d44c2a7317b9c8696aec6346b5eb2014de4d8` | `5f48b56276cf2aac5989faf3cc8e0e45f368e37e70181a68520a6a777307fb4e` | `6d5cd1adaf048e66e03d2706b38fdd8b6b83f15adef7a4d86f4137c58c7fd1b8` |
| Превью | [classic-preview.png](../tags/roundabout/classic-preview.png) | [custom-preview.png](../tags/roundabout/custom-preview.png) | [original-preview.png](../tags/roundabout/original-preview.png) |
| Отчёт | [classic_validation.json](../tags/roundabout/classic_validation.json) | [custom_validation.json](../tags/roundabout/custom_validation.json) | [original_validation.json](../tags/roundabout/original_validation.json) |

## Как они называются на браслете

В галерее циферблатов (Notify/Mi Fitness) отображается имя FACE:

- **NFC-метки — Классика**;
- **NFC-метки — Авторские**;
- **NFC-метки — Оригинал**.

Внутри каждого — четыре стиля. Браслет показывает их как **Стиль 1–4**;
названия цветов (Dark Turquoise, Copper и т.п.) — наши подписи в
документации и превью, а не отдельные пункты в галерее.
В общем списке приложений остаются **NFC-метки**, **Mi Home**, **Модули**.

## Что общего

- Первый тап по нижнему кругу: загрузка модулей, `READY`, запись
  `/data/tag_catalog/auto_load` и перезагрузка браслета.
- После перезагрузки нижний круг работает как выбранное штатное приложение
  (на превью — Heart Rate), а модули восстанавливаются автоматически.
- Все три круга остаются редактируемыми Slot. У 43 штатных вариантов
  **нижнего** круга есть прозрачный Lua-слой; верхние два круга и AOD —
  без него.
- Меню приложений: **NFC-метки**, **Mi Home**, **Модули**.
- Mi Home: пять слотов, BLE-опрос Pixel и локальный miIO. Пиксельное
  приложение должно быть открыто и подключено.
- Иконки приложений и нижнего круга — штатные ресурсы прошивки.

## Темы

- **Оригинал** — исходные четыре стиля Roundabout без перекраски; добавлены
  только наши функции (installer-слой, лаунчер, Mi Home).
- **Классика** — основные цвета: тёмная бирюза, светлый сапфир, глубокий
  океан, светлая бирюза.
- **Авторские** — медный, светлый изумруд, закатный коралл и лесной шалфей.

В «Классике» и «Авторских» перекрашены безель, минутная стрелка, дорожка
солнца, нижние полосы шагов/ккал, AOD и превью; остальные ресурсы
сохраняются. Стиль 2 — полный клон светлого стиля 4 (формат держит только
один светлый базовый стиль).

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

«Оригинал» собирается напрямую из той же базы, без перекраски:

```sh
build/shortcut-research-venv/bin/python tags/bootstrap/build_choice_face.py \
  --circle-app findPhone --face-id 999999975 \
  --face-name "NFC-метки — Оригинал" \
  --core tags/catalog/build/catalog_core_v1.elf \
  --ui tags/launcher/build/nfc_tags_catalog_ui_v4.elf \
  --output build/nfc-tags-original.face \
  --report tags/roundabout/original_validation.json
```

«Классика» и «Авторские» — из базы:

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

## Установка

1. Передать нужные `.face` на браслет через **Notify for Xiaomi**.
2. В галерее появятся **NFC-метки — Классика**, **NFC-метки — Авторские**
   и/или **NFC-метки — Оригинал**.
3. Выбрать циферблат, нажать нижний круг один раз и дождаться перезагрузки.
4. После перезагрузки проверить меню и выбранное приложение нижнего круга.

Все три FACE можно установить одновременно: ID и имена различаются.

## Ограничения

- Аппаратная проверка установки, редактора, клона стиля 2, AOD, нижних
  полос, первого тапа и автозагрузки ещё не выполнена.
- Стиль 2 в «Классике»/«Авторских» — статический клон стиля 4; правки
  стиля 4 после сборки в клон не попадут.
- «Полупрозрачность» — предварительное смешение в indexed8, не runtime-альфа.
- Превью — рендеры, не фотографии устройства.
- Если сменить нижний круг на вариант из верхних двух (без Lua), installer
  и автозагрузка для этой настройки не сработают; используйте нижний круг.
