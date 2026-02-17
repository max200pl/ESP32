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

const char *WIFI_SSID = "YOUR_WIFI_SSID";         // ← ИЗМЕНИТЕ НА ИМЯ ВАШЕЙ WiFi СЕТИ
const char *WIFI_PASSWORD = "YOUR_WIFI_PASSWORD"; // ← ИЗМЕНИТЕ НА ПАРОЛЬ

// ============================================================================
// НАСТРОЙКИ СЕРВЕРА
// ============================================================================

const char *SERVER_URL = "http://192.168.1.100:3000/data"; // ← ИЗМЕНИТЕ IP НА ВАШ КОМПЬЮТЕР

// ============================================================================
// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
// ============================================================================

unsigned long lastConnectionAttempt = 0;
const unsigned long CONNECTION_RETRY_DELAY = 5000; // 5 секунд между попытками

// ============================================================================
// ПРОТОТИПЫ ФУНКЦИЙ
// ============================================================================

void connectToWiFi();
void sendToServer(String jsonData);
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

    delay(10); // Небольшая задержка для стабильности
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
