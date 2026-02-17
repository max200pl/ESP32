# ESP32 WiFi Bridge

WiFi мост между STM32 Black Pill и Node.js сервером.

## 🔧 Оборудование

- **ESP32-WROOM-32D**
- **STM32F411CEU6 Black Pill**
- **3 провода** (TX, RX, GND)

## 🔌 Подключение

```
ESP32-WROOM-32D    →    STM32 Black Pill
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
GPIO3 (RX)         ←    PA9  (TX)
GPIO1 (TX)         →    PA10 (RX)
GND                →    GND
```

## ⚙️ Настройка

Перед прошивкой отредактируйте `src/main.cpp`:

```cpp
const char *WIFI_SSID = "YOUR_WIFI_SSID";        // ← Ваша WiFi сеть
const char *WIFI_PASSWORD = "YOUR_WIFI_PASSWORD"; // ← Пароль
const char *SERVER_URL = "http://192.168.1.100:3000/data"; // ← IP компьютера
```

## 🚀 Сборка и загрузка

### Через VSCode (PlatformIO):

1. Открыть проект в VSCode
2. Подключить ESP32 к USB
3. Нажать **"Upload"** в нижней панели

### Через командную строку:

```bash
# Сборка
platformio run

# Загрузка
platformio run --target upload

# Монитор порта
platformio device monitor
```

## 📊 Формат данных

ESP32 получает от STM32 и отправляет на сервер JSON:

### Кнопка:
```json
{"button": 0, "state": "pressed"}
```

### Мотор:
```json
{"motor": 0, "direction": "forward", "speed": 75}
```

## 🐛 Отладка

### Serial Monitor:

```bash
platformio device monitor -b 115200
```

**Ожидаемый вывод:**
```
=================================
ESP32 WiFi Bridge для STM32
=================================

[WiFi] Подключение к YourWiFi...
[WiFi] ✓ Подключено!
[WiFi] IP адрес: 192.168.1.150
[WiFi] Сигнал (RSSI): -45 dBm
[WiFi] Сервер: http://192.168.1.100:3000/data

[STM32] Получено: {"button":0,"state":"pressed"}
[HTTP] Отправка на сервер: {"button":0,"state":"pressed"}
[HTTP] ✓ Код ответа: 200
```

## 🔍 Возможные проблемы

### ESP32 не подключается к WiFi

- Проверить SSID и пароль
- Убедиться что WiFi 2.4 GHz (ESP32 не работает с 5 GHz)
- Приблизить ESP32 к роутеру

### Не получает данные от STM32

- Проверить провода: TX → RX, RX → TX
- Проверить общий GND
- Убедиться что baud rate = 115200

### Сервер не получает данные

- Проверить IP адрес компьютера
- Убедиться что ESP32 и компьютер в одной сети
- Проверить что сервер запущен (`node server.js`)
- Проверить firewall (разрешить порт 3000)

## 📚 Библиотеки

- **WiFi** - встроена в ESP32 Arduino Core
- **HTTPClient** - встроена в ESP32 Arduino Core
- **ArduinoJson** v6.21.3 - парсинг JSON

## 🎯 Следующие шаги

- [ ] Двусторонняя связь (Server → ESP32 → STM32)
- [ ] WebSocket для реал-тайм связи
- [ ] OTA обновления прошивки
- [ ] Шифрование данных (HTTPS)
