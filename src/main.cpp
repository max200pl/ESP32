/**
 * ESP32-WROOM-32D WiFi Bridge для STM32 Black Pill
 *
 * Функции:
 * - Получает данные от STM32 через UART (115200 baud)
 * - Отправляет данные на Node.js сервер через WiFi
 * - Поддерживает JSON формат
 *
 * Подключение:
 * ESP32 RX (GPIO3/RXD0) → STM32 TX (PA9)
 * ESP32 TX (GPIO1/TXD0) → STM32 RX (PA10)
 * ESP32 GND → STM32 GND
 */

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ============================================================================
// НАСТРОЙКИ WiFi
// ============================================================================

const char *WIFI_SSID = "TOTOLINK_A702R";
const char *WIFI_PASSWORD = "87654321";

// ============================================================================
// НАСТРОЙКИ СЕРВЕРА
// ============================================================================

const char *SERVER_URL   = "http://192.168.0.3:3000/data";
const char *COMMAND_URL  = "http://192.168.0.3:3000/command";

// ============================================================================
// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
// ============================================================================

unsigned long lastConnectionAttempt = 0;
const unsigned long CONNECTION_RETRY_DELAY = 5000;

unsigned long lastCmdPoll = 0;
const unsigned long CMD_POLL_INTERVAL = 200; // Poll commands every 200ms

// ============================================================================
// ПРОТОТИПЫ ФУНКЦИЙ
// ============================================================================

void connectToWiFi();
void sendToServer(String jsonData);
void pollCommands();
String addMetadata(String jsonData);
void sendTestData();

// ============================================================================
// SETUP - Инициализация
// ============================================================================

void setup()
{
    // Настройка UART для связи с STM32
    // ESP32 использует Serial (UART0) на GPIO1 (TX) и GPIO3 (RX)
    Serial.begin(115200);
    delay(100);

    Serial.println();
    Serial.println("=================================");
    Serial.println("ESP32 WiFi Bridge для STM32");
    Serial.println("=================================");
    Serial.println();

    // Настройка встроенного LED (GPIO2 на ESP32-WROOM-32D)
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    // Подключение к WiFi
    connectToWiFi();
}

// ============================================================================
// LOOP - Главный цикл
// ============================================================================

void loop()
{
    // Проверка подключения к WiFi
    if (WiFi.status() != WL_CONNECTED)
    {
        // Попытка переподключения каждые 5 секунд
        if (millis() - lastConnectionAttempt > CONNECTION_RETRY_DELAY)
        {
            Serial.println("[WiFi] Переподключение...");
            connectToWiFi();
            lastConnectionAttempt = millis();
        }
        delay(1000);
        return;
    }

    // Индикация работы (мигание LED)
    static unsigned long lastBlink = 0;
    if (millis() - lastBlink > 2000)
    {
        digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
        lastBlink = millis();
    }

    // Проверка данных от STM32 через UART
    if (Serial.available() > 0)
    {
        String data = Serial.readStringUntil('\n');
        data.trim();

        if (data.length() > 0)
        {
            Serial.print("[STM32] Получено: ");
            Serial.println(data);

            // Отправка данных на сервер
            sendToServer(data);
        }
    }

    // Poll server for commands from browser
    pollCommands();

    delay(10);
}

// ============================================================================
// ФУНКЦИИ
// ============================================================================

/**
 * Подключение к WiFi сети
 */
void connectToWiFi()
{
    Serial.print("[WiFi] Подключение к ");
    Serial.print(WIFI_SSID);
    Serial.print("...");

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20)
    {
        delay(500);
        Serial.print(".");
        attempts++;

        // Мигание LED при подключении
        digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println();
        Serial.println("[WiFi] ✓ Подключено!");
        Serial.print("[WiFi] IP адрес: ");
        Serial.println(WiFi.localIP());
        Serial.print("[WiFi] Сигнал (RSSI): ");
        Serial.print(WiFi.RSSI());
        Serial.println(" dBm");
        Serial.print("[WiFi] Сервер: ");
        Serial.println(SERVER_URL);
        digitalWrite(LED_BUILTIN, HIGH); // LED ON (успешное подключение)
    }
    else
    {
        Serial.println();
        Serial.println("[WiFi] ✗ Ошибка подключения!");
        Serial.println("[WiFi] Проверьте SSID и пароль");
        digitalWrite(LED_BUILTIN, LOW); // LED OFF (ошибка)
    }
}

/**
 * Отправка данных на Node.js сервер
 */
void sendToServer(String jsonData)
{
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("[HTTP] ✗ WiFi не подключен!");
        return;
    }

    HTTPClient http;
    http.begin(SERVER_URL);
    http.addHeader("Content-Type", "application/json");

    Serial.print("[HTTP] Отправка на сервер: ");
    Serial.println(jsonData);

    int httpResponseCode = http.POST(jsonData);

    if (httpResponseCode > 0)
    {
        Serial.print("[HTTP] ✓ Код ответа: ");
        Serial.println(httpResponseCode);

        String response = http.getString();
        if (response.length() > 0)
        {
            Serial.print("[HTTP] Ответ сервера: ");
            Serial.println(response);
        }
    }
    else
    {
        Serial.print("[HTTP] ✗ Ошибка: ");
        Serial.println(http.errorToString(httpResponseCode));
    }

    http.end();
}

// ============================================================================
// ДОПОЛНИТЕЛЬНЫЕ ФУНКЦИИ (для будущего расширения)
// ============================================================================

/**
 * Poll server for commands from browser and forward to STM32
 * Command format sent to STM32: "C:F:70\n" (forward 70%)
 */
void pollCommands()
{
    if (WiFi.status() != WL_CONNECTED) return;
    if (millis() - lastCmdPoll < CMD_POLL_INTERVAL) return;
    lastCmdPoll = millis();

    HTTPClient http;
    http.begin(COMMAND_URL);
    int code = http.GET();

    if (code == 200)
    {
        String response = http.getString();
        if (response.length() > 2) // Not just "{}"
        {
            StaticJsonDocument<128> doc;
            if (!deserializeJson(doc, response) && doc.containsKey("action"))
            {
                String action = doc["action"].as<String>();
                int spd = doc["speed"] | 70;
                String cmd = "";

                if      (action == "forward")  cmd = "C:F:" + String(spd);
                else if (action == "backward") cmd = "C:B:" + String(spd);
                else if (action == "left")     cmd = "C:L:" + String(spd);
                else if (action == "right")    cmd = "C:R:" + String(spd);
                else if (action == "stop")     cmd = "C:S";
                else if (action == "motor")
                {
                    int id    = doc["id"] | 0;
                    String dir = doc["dir"] | "stop";
                    char d = (dir == "forward") ? 'F' : (dir == "backward") ? 'B' : 'S';
                    cmd = "C:M:" + String(id) + ":" + String(d) + ":" + String(spd);
                }

                if (cmd.length() > 0)
                {
                    Serial.println(cmd); // Send to STM32 via UART
                }
            }
        }
    }
    http.end();
}

/**
 * Парсинг JSON и добавление метаданных
 */
String addMetadata(String jsonData)
{
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, jsonData);

    if (error)
    {
        Serial.print("[JSON] Ошибка парсинга: ");
        Serial.println(error.c_str());
        return jsonData; // Возвращаем оригинал если ошибка
    }

    // Добавляем метаданные ESP32
    doc["timestamp"] = millis();
    doc["wifi_rssi"] = WiFi.RSSI();
    doc["esp_id"] = "ESP32_001";
    doc["free_heap"] = ESP.getFreeHeap();

    String output;
    serializeJson(doc, output);
    return output;
}

/**
 * Отправка тестовых данных (для проверки без STM32)
 */
void sendTestData()
{
    StaticJsonDocument<128> doc;
    doc["test"] = true;
    doc["message"] = "Hello from ESP32";
    doc["uptime"] = millis();
    doc["chip"] = "ESP32-WROOM-32D";

    String jsonData;
    serializeJson(doc, jsonData);

    sendToServer(jsonData);
}
