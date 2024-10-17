#include <WiFi.h>
#include <HTTPClient.h>
#include <NimBLEDevice.h>
#include <ArduinoJson.h>
#include "decoder.h"

TheengsDecoder decoder;

// Ustawienia Wi-Fi
const char* ssid = "SSID";
const char* password = "PASS";

// Inicjalizacja globalnych zmiennych
NimBLEScan* pBLEScan;
StaticJsonDocument<1024> doc;

// URL serwera
const String serverUrl = "https://drkomp.pl/blegate/index.php"; // Zmień na swój URL

// Flaga do kontrolowania wysyłania danych na serwer
bool sendData = true;  // Ustaw na false, aby wyłączyć wysyłanie danych

// Funkcja wysyłająca dane JSON na serwer
void sendDataToServer(JsonObject& BLEdata) {
  if (sendData && WiFi.status() == WL_CONNECTED) {  // Sprawdzenie flagi sendData
    HTTPClient http;
    http.begin(serverUrl);
    http.addHeader("Content-Type", "application/json");

    // Dodajemy adres MAC ESP32 do obiektu JSON
    String espMacAddress = WiFi.macAddress();
    BLEdata["gate_mac"] = espMacAddress; // Dodanie adresu MAC ESP32

    // Serializujemy obiekt JSON do Stringa
    String jsonData;
    serializeJson(BLEdata, jsonData);

    // Wysyłamy żądanie POST
    int httpResponseCode = http.POST(jsonData);

    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println("Odpowiedź serwera: " + response);
    } else {
      Serial.println("Błąd przy wysyłaniu danych: " + String(httpResponseCode));
    }
    
    http.end();
  } else if (!sendData) {
    Serial.println("Wysyłanie danych jest wyłączone.");
  } else {
    Serial.println("Brak połączenia z Wi-Fi");
  }
}

// Klasa obsługująca znalezione urządzenia
class MyAdvertisedDeviceCallbacks : public NimBLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice* advertisedDevice) {
    JsonObject BLEdata = doc.to<JsonObject>();
    String mac_address = advertisedDevice->getAddress().toString().c_str();
    mac_address.replace(":", "-");
    mac_address.toUpperCase();
    BLEdata["id"] = mac_address;

    // Dodajemy oryginalny ciąg pakietu do obiektu JSON
    String originalPayload = getOriginalPayload(advertisedDevice);
    BLEdata["original_payload"] = originalPayload;

    if (advertisedDevice->haveName()) {
      BLEdata["name"] = advertisedDevice->getName().c_str();
    }

    if (advertisedDevice->haveManufacturerData()) {
      char* manufacturerdata = BLEUtils::buildHexData(NULL, (uint8_t*)advertisedDevice->getManufacturerData().data(), advertisedDevice->getManufacturerData().length());
      BLEdata["manufacturerdata"] = manufacturerdata;
      free(manufacturerdata);
    }

    if (advertisedDevice->haveRSSI()) {
      BLEdata["rssi"] = advertisedDevice->getRSSI();
    }

    if (advertisedDevice->haveTXPower()) {
      BLEdata["txpower"] = advertisedDevice->getTXPower();
    }

    if (advertisedDevice->haveServiceData()) {
      int serviceDataCount = advertisedDevice->getServiceDataCount();
      for (int j = 0; j < serviceDataCount; j++) {
        std::string serviceData = convertServiceData(advertisedDevice->getServiceData(j));
        BLEdata["servicedata"] = serviceData;
        BLEdata["servicedatauuid"] = advertisedDevice->getServiceDataUUID(j).toString().c_str();
      }
    }

    if (decoder.decodeBLEJson(BLEdata)) {
      serializeJson(BLEdata, Serial);  // Wyświetlenie danych w konsoli

      // Wysłanie danych do serwera
      sendDataToServer(BLEdata);
    }
  }

  std::string convertServiceData(std::string deviceServiceData) {
    int serviceDataLength = (int)deviceServiceData.length();
    char spr[2 * serviceDataLength + 1];
    for (int i = 0; i < serviceDataLength; i++) {
      sprintf(spr + 2 * i, "%.2x", (unsigned char)deviceServiceData[i]);
    }
    spr[2 * serviceDataLength] = 0;
    return spr;
  }

  String getOriginalPayload(BLEAdvertisedDevice* advertisedDevice) {
    const uint8_t* payload = advertisedDevice->getPayload();
    size_t length = advertisedDevice->getPayloadLength();
    String payloadString;

    for (size_t i = 0; i < length; i++) {
      if (i > 0) payloadString += " "; // dodanie spacji między bajtami
      payloadString += String(payload[i], HEX); // konwersja bajtów do formatu szesnastkowego
    }

    return payloadString;
  }
};

void setup() {
  Serial.begin(115200);

  // Połączenie z Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Łączenie z WiFi...");
  }
  Serial.println("Połączono z Wi-Fi");

  // Inicjalizacja BLE
  Serial.println("Rozpoczynam skanowanie...");
  NimBLEDevice::init("");
  pBLEScan = NimBLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks(), false);
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(97);
  pBLEScan->setWindow(37);
  pBLEScan->setMaxResults(0);
  pBLEScan->setDuplicateFilter(true);
}

void loop() {
  if (!pBLEScan->isScanning()) {
    pBLEScan->start(30, nullptr, false);
  }

  delay(2000); // Czas oczekiwania między kolejnymi skanami
}
