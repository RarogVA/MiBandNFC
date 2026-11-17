# Жесты / Quick actions на Xiaomi Smart Band 10 Pro (p67glt, 3.201.026)

Статический разбор `references/vela_ap.bin` (база `0x2c0c0000`, Thumb, указатели с `|1`).
Дата: 2026-11-17. Изменяемых файлов продукта нет; все адреса — только для этого образа.

## Итог (TL;DR)

1. **Таблица действий фиксированная.** На браслете UI строит список ровно из **5** действий,
   заданных массивом строковых ID по адресу `0x2c9f935c`: отклонить звонок, игнорировать
   звонки/сообщения, пульт камеры, погода, «нет». Список не зависит от установленных
   приложений (кроме проверки, установлены ли 5 штатных).
2. **Произвольный native app ID не поддерживается.** Обработчик `handle_quick_guesture`
   (`0x2c44775c`) — это цепочка `strcmp` по фиксированным строкам с жёстко зашитыми
   действиями; неизвестная строка просто игнорируется. Нигде в пути жеста нет поля
   «app ID»/«package name». Единственный запуск приложения — **Weather**, фиксированным
   сообщением.
3. **Настройка хранится локально**: `/data/persist.db`, ключи `persist.type_roll_wrist`,
   `persist.type_shake_wrist`, `persist.type_snap_finger`; значение — строка-ID действия.
   Запись делают только UI-страница настроек и factory reset. **SET-команды от телефона
   не найдено**; числовой маппинг 1..9 — это только статистика, не протокол.
4. **Наш app 0xD4 (`dev.mbapdu.tags`) выбрать нельзя** штатным путём. Даже если записать
   его имя/ID в `persist.type_*` в обход UI, жест ничего не запустит.
5. Следующий шаг — аппаратная проверка (раздел 5): список из 5 пунктов в UI + негативный
   тест с записью произвольной строки в `persist.type_roll_wrist` через диагностический
   модуль.

## 1. Подтверждено

### 1.1 Таблица действий (UI)

`0x2c9f935c` — массив из 5 указателей на строки-ID:

```
2c9f935c: 2c89bec4  "reject_incoming_calls.setting"
2c9f9360: 2c89bf04  "ignore_incoming_calls_or_msgs.setting"
2c9f9364: 2c89bf70  "remote_control_photography.settings"
2c9f9368: 2c89bdf8  "open_weather_app.setting"
2c9f936c: 2c89c5d0  "open_none_app.setting"
```

Страница `page_gesture_settings` (`0x2c4d0d8c`, лог-имя `guesture_radios_create`) строит
радио-список ровно по этому массиву:

```
2c4d0f80: ldr.w fp,[pc,#0x184]      ; fp = 0x2c9f935c
2c4d0f8e: ldr.w r0,[fp]             ; первый ID
...
2c4d1032: ldr   r1,[fp],#4          ; очередной ID действия
2c4d1122: cmp   r4,#5
2c4d1124: bne   0x2c4d107c          ; цикл ровно 5 итераций
...
2c4d1090: cmp   r4,#2
2c4d1096: ldr   r0,="remote_control_photography.settings"  ; индекс 2 = пульт камеры
```

На каждый пункт вешается колбэк `0x0c4d0795` (`questure_wrist/shake_app_name_set`):

```
2c4d1012: ldr r1,[pc,#0xdc]         ; 0x0c4d0795
2c4d1016: bl  0x2c557df0            ; регистрация cb на объекте
```

Колбэк `0x2c4d0794` берёт индекс нажатого пункта, выбирает строку из той же таблицы и
пишет её в настройку текущего жеста:

```
2c4d07c4: ldr   r3,[pc,#0x100]      ; 0x2c9f935c
2c4d07c8: ldr.w r1,[r3,r4,lsl#2]    ; action_id = table[index]
2c4d07cc: bl    0x2c447fb0          ; settings_set_string(key, action_id)
```

Тип жеста берётся из байта `0x200dafa4`: 2 = поворот запястья, 3 = встряхивание,
4 = «snap»; ключи — `persist.type_roll_wrist` / `..._shake_wrist` / `..._snap_finger`
(`0x2c4d07c6/0x2c4d07f2/0x2c4d082c`).

### 1.2 Событие жеста

`topic_quick_guesture_handler` `0x2c448024` (uORB-подписка на `quick_guesture`):

* `0x1c2` → SHAKE (`0x2c448238`), `0x1c3` → WRIST (`0x2c4481ee`);
* проверки: DND/sleep (`0x2c448038`/`0x2c44803e`, при срабатывании — выход),
  `persist.switch_guestre_state`, screen state;
* строка действия читается **из настроек**, а не из события:

```
2c44822c: ldr  r0,="persist.type_roll_wrist"
2c448230: add  r1,r7,#0x10
2c448232: bl   0x2c447fbc            ; settings_get_string(key, buf, default)
2c448236: b    0x2c4480c2

2c448268: ldr  r2,="open_wechat_payment.setting"   ; default для shake
2c44826e: ldr  r0,="persist.type_shake_wrist"
2c448270: bl   0x2c36f25c            ; settings_get_string
```

Далее действие кладётся в глобальный буфер `0x200d60d4` и, если экран уже включён,
немедленно вызывается `handle_quick_guesture`; иначе — отложенно через
`topic_screen_status_handler` `0x2c4479ec` (при state == 2):

```
2c447a36: ldr r0,=0x200d60d4
2c447a38: bl  0x2c44775c            ; handle_quick_guesture(action)
```

### 1.3 Диспетчеризация: только фиксированные строки

`handle_quick_guesture` `0x2c44775c`. Сначала — частный случай глобальной прошивки:
**WeChat Pay подменяется на Weather**.

```
2c44775c: ldr r1,="open_wechat_payment.setting"
2c447760: bl  0x2c558468             ; strcmp(action, "...wechat...")
2c447764: cbnz r0, 0x2c447778
2c44776c: movs r1,#0xff
2c447770: ldr r3,="open_weather_app.setting"
2c447772: ldr r2,="%s"
2c447774: bl  0x2c1b8e6c             ; snprintf(action, 255, "%s", "open_weather_app.setting")
```

Затем фиксированная цепочка `strcmp` и жёстко зашитые обработчики:

| Строка-ID | Адрес сравнения | Обработчик |
|---|---|---|
| `reject_incoming_calls.setting` | `0x2c4477a8` | `0x2c4adc28` (отклонить звонок) |
| `ignore_incoming_calls_or_msgs.setting` | `0x2c4477b4` | `0x2c4adcf8`, сообщение `0x2202` |
| `remote_control_photography.settings` | `0x2c4477c2` | сообщение `0x2203` (пульт камеры на телефоне) |
| `mi_iot_linkage.setting` | `0x2c4477d0` | сообщение `0x2208` |
| `open_wechat_payment.setting` | `0x2c4477de` | уже переписано на weather |
| `open_alipay_payment.setting` | `0x2c4477ea` | фактически ничего (ветка в эпилог) |
| `open_voice_assistant.setting` | `0x2c4477f6` | фактически ничего (ветка в эпилог) |
| `open_weather_app.setting` | `0x2c447802` | запуск Weather (см. 1.4) |
| `open_none_app.setting` | — | отдельного сравнения нет, уходит в общий no-op |

Любая другая строка доходит до эпилога `0x2c447860` (`pop {r4,r5,r7,pc}`) — **no-op**.
Никакого `app_id`/`pkgname` в этом коде нет.

### 1.4 Единственный запуск приложения — Weather

```
2c44780c: bl    0x2c44a2f8          ; get_watchface_on(); без watchface — выход
...
2c44782c: movs  r1,#0
2c44782e: movs  r2,#0x2c
2c447832: bl    memset              ; 44-байтное сообщение
2c447836: movs  r1,#0x21
2c447838: movs  r2,#0x0a
2c44783a: ldr   r3,[pc]=0x200b7360  ; сервис/UI-канал
2c447840: strh  r1,[r7,#0xa]        ; поле 0x21
2c447842: strb  r2,[r7,#0xc]        ; поле 0x0a
2c447844: blx   r3 ([r3,#8])
```

То есть запуск Weather — это фиксированная команда `0x21/0x0a` в системный сервис,
**без app ID из настройки**. Другие приложения так не запускаются: у wechat/alipay/voice
ветки пустые (на глобальной прошивке), remote camera шлёт команду телефону, mi_iot —
команду 0x2208.

### 1.5 Где хранится

* KV-слой открывает `/data/persist.db` (`0x2c8763fc`, открытие в `0x2c36fa44`).
* `settings_set_string` = `0x2c36f210`; обёртка `0x2c447fb0` (`settings_set_string(key,value)`).
* `settings_get_string` = `0x2c36f25c` (есть fallback-default), `settings_get_bool` =
  `0x2c36f340`; обёртка `0x2c447fbc` с default по ключу:

```
2c447fe4: r2="ignore_incoming_calls_or_msgs.setting"  ; default для roll
2c447ffe: r2="open_wechat_payment.setting"            ; default для shake
2c447ff2: r2="mi_iot_linkage.setting"                 ; default для snap
```

* Ключи: `persist.type_roll_wrist`, `persist.type_shake_wrist`,
  `persist.type_snap_finger`; значения — строки из закрытого набора (см. 1.1 и 1.3).
* Сопутствующие: `persist.switch_guestre_state` (вкл/выкл алгоритма,
  `set_quick_guesture_switch_state` `0x2c447e28`), `persist.guesture_first_setting`,
  `persist.guesture_roll_first_setting`, `persist.guesture_shake_first_setting`,
  `persist.guesture_snap_first_setting`, `persist.guesture_wrist_tch_end`,
  `persist.guesture_shake_tch_end`, `close_guesture_page_open_app`, `is_it_guesture_teach`.
* Factory reset возвращает roll → ignore, shake → wechat (`0x2c5aef0c`/`0x2c5aef14`).

### 1.6 Фича в прошивке присутствует

`page_gesture_settings` (`0x2c8b955c`), страница/колбэки `0x2c4d1374`, `0x2c4d0d8c`,
ресурсы `/resource/app/settings/guesture/guesture.bin` (`0x2c8b9460`), `gwrist/`, `gshake/`,
`gsnap/`, библиотека `MiGesture-1.0.0 (2025-04-24)` (`0x2c91bae4`), uORB-топики
`quick_guesture`/`system_quick_guesture`. То есть на этой модели/прошивке жест
реализован; ограничения: DND/сон, включённый `persist.switch_guestre_state`, для
Weather — включённый циферблат.

## 2. Телефон / Mi Fitness

* Строк `System_SystemID_*` в образе **нет вообще** (полный поиск по строкам), как и
  «gesture»-команд протокола. Именование команд в этой прошивке другое, например
  `Notification_NotificationID_GET_SETTING` (`0x2c8bab5c`), `Nfc_NfcID_GET_DEFAULT_CARD`.
* SET-обработчика, который писал бы `persist.type_*` по данным из BLE, не найдено: все
  найденные записи — UI-колбэк (`0x2c4d0794`), factory reset (`0x2c5aef0c`,
  `0x2c4df0f6`); дефолт подставляется при чтении, если ключа нет.
* Числовая нумерация действий 1..9 существует, но это **статистика**, не протокол:
  функция `stats_quick_guesture_settings` `0x2c4483c0` преобразует строку в число и
  отправляет событие в `append_str_custom_value_event_to_buffer` (`0x2c7e4320`) с логом
  `[%s] %s: a1 %s a2 %s a3 %s a4 %s`. Маппинг: 1 alipay, 2 wechat, 3 voice, 4 weather,
  5 remote camera, 6 ignore, 7 reject, 8 none, 9 mi_iot.
* **Вывод:** настройка жест-действия — локальная для браслета. Телефон может читать её
  через общий settings-канал (не доказано), но SET-команды в образе нет. Утверждение
  «Mi Fitness задаёт действие жеста» для 3.201.026 не подтверждается.

## 3. Можно ли выбрать наш app 0xD4

**Нет, штатным путём — нельзя.** Причины (подтверждено кодом):

1. UI-список — ровно 5 фиксированных ID (§1.1), без «выбрать приложение из списка».
2. Обработчик жеста — фиксированные `strcmp` + зашитые действия (§1.3); произвольная
   строка/ID игнорируется.
3. Запуск приложения есть только для Weather и не параметризован app ID (§1.4).
4. Проверка установленных приложений (`select_guesture_app_is_uninstalled`,
   цикл `0x2c4cf0e2`) перебирает установленные приложения, но лишь выставляет флаги для
   **пяти известных**: remote camera, wechat, alipay, weather, aivs. Новые приложения
   в список не добавляются.

Что можно сделать только модификацией прошивки: заменить ветку `open_weather_app` на
запуск нашего пакета либо добавить свою строку в диспетчер. Штатной «свободной» ячейки,
которая уже запускает приложение по ID, нет: «none» — no-op, «mi_iot»/remote — команды
телефону, wechat/alipay/voice на глобальной сборке не обрабатываются.

## 4. Риски и неизвестные

* **Две генерации UI в образе.** Кроме `page_gesture_settings` (`0x2c4d0d8c`, 5 фиксированных
  action-ID) есть старая `page_guesture` (`0x2c4cef48`) с проверкой имён установленных
  приложений (remote camera/wechat/alipay/weather/aivs). Диспетчер у обеих веток общий —
  `handle_quick_guesture` с закрытым набором строк, произвольного app ID нет. Какая
  страница реально открывается на устройстве, решает аппаратная проверка (раздел 5, шаг 2).
* **Неизвестно:** использует ли Mi Fitness отдельный generic-settings канал для этих
  ключей (строк-команд нет, но канал мог быть без строковых литералов). Проверяется
  только на устройстве.
* **Неизвестно:** полный формат сообщения `0x21/0x0a` (какой сервис и что означает
  `0x0a`); для нашей задачи не критично, т.к. ID фиксирован.
* **Инференс:** `0x2c558070` — это «получить индекс радио-пункта» (венер в другую часть
  прошивки/ядро), точная семантика по коду не доказана; на вывод о фиксированной
  таблице не влияет.
* **Риск:** ручная запись произвольной строки в `persist.type_*` может привести к
  пустому/некорректному тексту в списке настроек и к no-op жесту. Нужен откат на
  `open_none_app.setting`/дефолт.
* **Риск:** ресурсный бандл `/resource/app/settings/guesture/guesture.bin` в образе
  отсутствует (отдельный раздел), поэтому визуальный список проверяется только на
  устройстве.

## 5. Следующий эксперимент на устройстве

1. Установить наш app 0xD4 (`dev.mbapdu.tags`), убедиться, что он виден в лаунчере.
2. Открыть Settings → Быстрые действия → Жесты. Зафиксировать (фото) список действий
   для «поворота запястья» и «встряхивания». Ожидание: ровно 5 пунктов —
   «Отклонять звонки», «Игнорировать звонки/уведомления», «Пульт камеры», «Погода»,
   «Нет». Если появится «Открыть приложение» с выбором приложений — статический вывод
   неверен для этого региона, собрать логи и фото.
3. Выбрать «Погода», включить циферблат, выполнить поворот запястья → проверить, что
   открывается именно Weather (единственный подтверждённый путь запуска).
4. **Негативный тест:** диагностическим модулем (по образцу `band/`, с проверкой
   фингерпринтов) вызвать `settings_set_string` (`0x2c36f211`) для
   `persist.type_roll_wrist` со значением `dev.mbapdu.tags` (или `0xD4`); затем
   `settings_get_string` (`0x2c36f25d`) — убедиться, что строка сохранилась; выполнить
   жест с включённым циферблатом. Ожидание: приложение не запускается, в логе нет
   обработчика. После теста вернуть `open_none_app.setting` (или factory default
   `ignore_incoming_calls_or_msgs.setting`).
5. Дополнительно: посмотреть, показывает ли Mi Fitness эти настройки и может ли менять;
   если да — попробовать выставить wechat/alipay/voice и убедиться, что на глобальной
   прошивке они не срабатывают (ветки пустые/подмена на weather).

## Приложение: карта адресов

| Адрес | Что |
|---|---|
| `0x2c9f935c` | таблица 5 action-ID (UI) |
| `0x2c9efd60` | таблица строк «1»..«9» (статистика) |
| `0x2c89c33c/354/370` | `persist.type_roll_wrist/shake_wrist/snap_finger` |
| `0x2c4d0794` | колбэк выбора действия (пишет настройку) |
| `0x2c4d0d8c` | `guesture_radios_create` (UI, 5 пунктов) |
| `0x2c4d1374` | on_create страницы жеста |
| `0x2c44775c` | `handle_quick_guesture` (диспетчер) |
| `0x2c448024` | `topic_quick_guesture_handler` (событие) |
| `0x2c4479ec` | отложенный запуск после включения экрана |
| `0x2c447e28` | `set_quick_guesture_switch_state` |
| `0x2c4483c0` | `stats_quick_guesture_settings` (1..9, статистика) |
| `0x2c447fb0`/`0x2c447fbc` | set/get строки действия |
| `0x2c36f210`/`0x2c36f25c`/`0x2c36f340` | settings set/get string/bool |
| `0x2c36fa44` | открытие `/data/persist.db` |
