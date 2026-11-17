# Три циферблата SmartRound

Состояние на 17 ноября 2026 года. Все три собраны для
**miwear.watch.p67glt, прошивка 3.201.026**, содержат одинаковые функции и
различаются только темами. Аппаратная приёмка **не выполнена**.

| | **SmartRound #1** | **SmartRound #2** | **SmartRound #3** |
| --- | --- | --- | --- |
| ID | `999999978` | `999999977` | `999999975` |
| Темы | Dark Turquoise, Light Sapphire, Deep Ocean, Light Turquoise | Copper, Light Emerald, Sunset Coral, Forest Sage | оригинальные Roundabout, стили 1–4 без перекраски |
| Файл | `build/nfc-tags-classic.face` | `build/nfc-tags-custom.face` | `build/nfc-tags-original.face` |
| Размер | 3 915 421 байт | 3 899 737 байт | 3 421 329 байт |
| SHA-256 | `0ba855d3c19a5a3d126091dca930c1b1721da1a8f1fda0a1bfa09daad79ab0f3` | `8409d92705c8331549b57f0d7a08c2782ccf4316f03268a2fd52e50217fd4a81` | `2398eba6909e7ce5f3dcdfe6a541a7d4a9b4d7ae4fc4215c099a02f130c7c597` |
| Превью | [classic-preview.png](../tags/roundabout/classic-preview.png) | [custom-preview.png](../tags/roundabout/custom-preview.png) | [original-preview.png](../tags/roundabout/original-preview.png) |
| Отчёт | [classic_validation.json](../tags/roundabout/classic_validation.json) | [custom_validation.json](../tags/roundabout/custom_validation.json) | [original_validation.json](../tags/roundabout/original_validation.json) |

## Как они называются на браслете

В галерее циферблатов (Notify/Mi Fitness) отображается имя FACE:

- **SmartRound #1**;
- **SmartRound #2**;
- **SmartRound #3**.

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

- **SmartRound #1** — основные цвета: тёмная бирюза, светлый сапфир,
  глубокий океан, светлая бирюза.
- **SmartRound #2** — авторские: медный, светлый изумруд, закатный коралл
  и лесной шалфей.
- **SmartRound #3** — исходные четыре стиля Roundabout без перекраски;
  добавлены только наши функции (installer-слой, лаунчер, Mi Home).

В #1 и #2 перекрашены безель, минутная стрелка, дорожка
солнца, нижние полосы шагов/ккал, AOD и превью; остальные ресурсы
сохраняются. Стиль 2 — полный клон светлого стиля 4 (формат держит только
один светлый базовый стиль).

## Сборка

Сначала базовый установочный FACE (не устанавливать, он нужен как основа):

```sh
build/shortcut-research-venv/bin/python tags/bootstrap/build_choice_face.py \
  --circle-app findPhone --face-id 999999976 \
  --face-name "SmartRound Base" \
  --core tags/catalog/build/catalog_core_v1.elf \
  --ui tags/launcher/build/nfc_tags_catalog_ui_v4.elf \
  --output build/nfc-tags-installer-base.face \
  --report build/nfc-tags-installer-base.json
```

«Оригинал» собирается напрямую из той же базы, без перекраски:

```sh
build/shortcut-research-venv/bin/python tags/bootstrap/build_choice_face.py \
  --circle-app findPhone --face-id 999999975 \
  --face-name "SmartRound #3" \
  --core tags/catalog/build/catalog_core_v1.elf \
  --ui tags/launcher/build/nfc_tags_catalog_ui_v4.elf \
  --output build/nfc-tags-original.face \
  --report tags/roundabout/original_validation.json
```

«Классика» и «Авторский» — из базы:

```sh
BASE=build/nfc-tags-installer-base.face
SHA=$(shasum -a 256 "$BASE" | cut -d' ' -f1)
build/shortcut-research-venv/bin/python tags/roundabout/combined_face.py \
  --base-face "$BASE" --base-sha "$SHA" --palette main \
  --face-id 999999978 --face-name "SmartRound #1" \
  --output build/nfc-tags-classic.face \
  --report tags/roundabout/classic_validation.json \
  --preview tags/roundabout/classic-preview.png
build/shortcut-research-venv/bin/python tags/roundabout/combined_face.py \
  --base-face "$BASE" --base-sha "$SHA" --palette custom \
  --face-id 999999977 --face-name "SmartRound #2" \
  --output build/nfc-tags-custom.face \
  --report tags/roundabout/custom_validation.json \
  --preview tags/roundabout/custom-preview.png
```

## Установка

1. Передать нужные `.face` на браслет через **Notify for Xiaomi**.
2. В галерее появятся **SmartRound #1**, **SmartRound #2**
   и/или **SmartRound #3**.
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
