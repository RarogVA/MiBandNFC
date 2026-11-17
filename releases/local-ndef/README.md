# Roundabout RU NFC Local

Кандидат для проверки локального NDEF Type 4 на браслете без Pixel в момент чтения.

- Файл циферблата: `roundabout-ru-nfc-local-p67glt.face`, Face ID `999999969`.
- Модель/прошивка: `miwear.watch.p67glt`, `3.201.026`.
- Аппаратно **не проверялся**; host-тесты проходят.
- Чеклист: [ACCEPTANCE.md](ACCEPTANCE.md).
- Issue: [#9](https://github.com/RarogVA/MiBandNFC/issues/9).

Содержимое: `main_local.lua` (Lua-загрузчик), `catalog-core-v2-local.elf`,
`catalog-ui-v5.elf`, артефакты интерфейса и сам `.face`. SHA-256 — в
`SHA256SUMS` и `manifest.json`.
