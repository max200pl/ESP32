/**
 * ESP32-WROOM-32D WiFi Bridge для STM32 Black Pill
 *
 * Архитектура: WebSocket клиент
 * - Подключается к Node.js серверу через WebSocket
 * - Регистрируется как "esp32" при подключении
 * - Получает команды от сервера и передаёт на STM32 через UART
 * - Получает телеметрию от STM32 и отправляет на сервер через WS
 *
 * Подключение:
 * ESP32 RX (GPIO3/RXD0) → STM32 TX (PA9)
 * ESP32 TX (GPIO1/TXD0) → STM32 RX (PA10)
 * ESP32 GND → STM32 GND
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

// ============================================================================
// НАСТРОЙКИ WiFi
// ============================================================================

const char *WIFI_SSID     = "TOTOLINK_A702R";
const char *WIFI_PASSWORD = "87654321";

// ============================================================================
// НАСТРОЙКИ СЕРВЕРА
// ============================================================================

const char *SERVER_HOST = "192.168.0.2";
const uint16_t SERVER_PORT = 3000;
const char *SERVER_PATH = "/ws";

// ============================================================================
// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
// ============================================================================

WebSocketsClient webSocket;
bool wsConnected = false;

// ============================================================================
// ПРОТОТИПЫ ФУНКЦИЙ
// ============================================================================

void connectToWiFi();
void webSocketEvent(WStype_t type, uint8_t *payload, size_t length);
void handleCommand(uint8_t *payload, size_t length);

// ============================================================================
// SETUP
// ============================================================================

void setup()
{
    // UART для связи с STM32 (НЕ использовать Serial.print в loop — идёт на STM32!)
    Serial.begin(115200);
    delay(100);

    // Встроенный LED
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    // Подключение к WiFi
    connectToWiFi();

    // Подключение к WebSocket серверу
    webSocket.begin(SERVER_HOST, SERVER_PORT, SERVER_PATH);
    webSocket.onEvent(webSocketEvent);
    webSocket.setReconnectInterval(3000);
}

// ============================================================================
// LOOP
// ============================================================================

void loop()
{
    // Обработка WebSocket (неблокирующий)
    webSocket.loop();

    // Переподключение WiFi если потеряно
    if (WiFi.status() != WL_CONNECTED)
    {
        wsConnected = false;
        connectToWiFi();
        return;
    }

    // Индикация работы (мигание LED)
    static unsigned long lastBlink = 0;
    if (millis() - lastBlink > 2000)
    {
        digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
        lastBlink = millis();
    }

    // Чтение телеметрии от STM32 и отправка на сервер
    if (Serial.available() > 0)
    {
        String data = Serial.readStringUntil('\n');
        data.trim();

        if (data.length() > 0 && wsConnected)
        {
            // Оборачиваем в JSON с типом "telemetry"
            StaticJsonDocument<256> doc;
            DeserializationError err = deserializeJson(doc, data);

            String msg;
            if (!err)
            {
                // Данные уже JSON — добавляем type поле
                doc["type"] = "telemetry";
                serializeJson(doc, msg);
            }
            else
            {
                // Не JSON — отправляем как raw строку
                msg = "{\"type\":\"telemetry\",\"raw\":\"" + data + "\"}";
            }

            webSocket.sendTXT(msg);
        }
    }
}

// ============================================================================
// WEBSOCKET ОБРАБОТЧИК СОБЫТИЙ
// ============================================================================

void webSocketEvent(WStype_t type, uint8_t *payload, size_t length)
{
    switch (type)
    {
        case WStype_CONNECTED:
            wsConnected = true;
            digitalWrite(LED_BUILTIN, HIGH);
            // Регистрируемся как ESP32
            webSocket.sendTXT("{\"type\":\"register\",\"device\":\"esp32\"}");
            break;

        case WStype_DISCONNECTED:
            wsConnected = false;
            digitalWrite(LED_BUILTIN, LOW);
            break;

        case WStype_TEXT:
            handleCommand(payload, length);
            break;

        case WStype_ERROR:
            wsConnected = false;
            break;

        default:
            break;
    }
}

// ============================================================================
// ОБРАБОТКА КОМАНД ОТ СЕРВЕРА
// ============================================================================

/**
 * Преобразует JSON команду от сервера в строку для STM32
 * Формат UART для STM32: "C:F:70\n"
 *
 * Входной JSON (от браузера через сервер):
 * {"type":"command","action":"forward","speed":70}
 * {"type":"command","action":"stop"}
 * {"type":"command","action":"motor","id":0,"dir":"forward","speed":80}
 */
void handleCommand(uint8_t *payload, size_t length)
{
    StaticJsonDocument<200> doc;
    DeserializationError err = deserializeJson(doc, payload, length);
    if (err) return;

    // Проверяем что это команда
    const char *msgType = doc["type"] | "";
    if (strcmp(msgType, "command") != 0) return;

    const char *action = doc["action"] | "";
    int spd = doc["speed"] | 70;

    String cmd = "";

    if      (strcmp(action, "forward")  == 0) cmd = "C:F:" + String(spd);
    else if (strcmp(action, "backward") == 0) cmd = "C:B:" + String(spd);
    else if (strcmp(action, "left")     == 0) cmd = "C:L:" + String(spd);
    else if (strcmp(action, "right")    == 0) cmd = "C:R:" + String(spd);
    else if (strcmp(action, "stop")     == 0) cmd = "C:S";
    else if (strcmp(action, "motor")    == 0)
    {
        int id       = doc["id"] | 0;
        const char *dir = doc["dir"] | "stop";
        char d = (strcmp(dir, "forward")  == 0) ? 'F' :
                 (strcmp(dir, "backward") == 0) ? 'B' : 'S';
        cmd = "C:M:" + String(id) + ":" + String(d) + ":" + String(spd);
    }

    if (cmd.length() > 0)
    {
        Serial.println(cmd);  // Отправка на STM32 через UART
    }
}

// ============================================================================
// ПОДКЛЮЧЕНИЕ К WiFi
// ============================================================================

void connectToWiFi()
{
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20)
    {
        delay(500);
        attempts++;
        digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        digitalWrite(LED_BUILTIN, HIGH);
    }
    else
    {
        digitalWrite(LED_BUILTIN, LOW);
    }
}
