#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>



///////////proba1 szlaban

// Konfiguracja OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Zmienne systemowe
int wolneMiejsca = 5;
const int LED_ZIELONA = 26;
const int LED_CZERWONA = 27;
String receivedValue = "";

// UUID dla BLE 
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string value = pCharacteristic->getValue();
      if (value.length() > 0) {
        receivedValue = "";
        for (int i = 0; i < value.length(); i++)
          receivedValue += value[i];
      }
    }
};

void setup() {
  Serial.begin(115200);
  pinMode(LED_ZIELONA, OUTPUT);
  pinMode(LED_CZERWONA, OUTPUT);

  digitalWrite(LED_ZIELONA, LOW);
  digitalWrite(LED_CZERWONA, LOW);
  // Start OLED
  display.begin(0x3C, true);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(10, 20);
  display.print("WOLNE MIEJSCA: ");
  display.println(wolneMiejsca);
  display.display();

  // Konfiguracja BLE
  BLEDevice::init("Parking_ESP32_iOS");
  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(SERVICE_UUID);
  BLECharacteristic *pCharacteristic = pService->createCharacteristic(
                                         CHARACTERISTIC_UUID,
                                         BLECharacteristic::PROPERTY_READ |
                                         BLECharacteristic::PROPERTY_WRITE
                                       );

  pCharacteristic->setCallbacks(new MyCallbacks());
  pService->start();
  pServer->getAdvertising()->start();
}

void loop() {
  if (receivedValue != "") {
    Serial.println("Otrzymano: " + receivedValue);

    if (wolneMiejsca > 0) {
    
    // SZLABAN OTWARTY
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 10);
    display.println("AUTORYZACJA: " + receivedValue);
    display.setTextSize(2);
    display.setCursor(0, 30);
    display.println(" OTWARTY ");
    display.display();
    
    digitalWrite(LED_ZIELONA, HIGH);
    digitalWrite(LED_CZERWONA, LOW);
    delay(5000);
    
    digitalWrite(LED_ZIELONA, LOW);
    digitalWrite(LED_CZERWONA, HIGH); //szlaban zamkniety miejsce zajete
    wolneMiejsca--;
    
//brak miejsc
    } else {
      display.clearDisplay();
      display.setTextSize(2);
      display.setCursor(0, 20);
      display.println(" BRAK ");
      display.println(" MIEJSC ");
      display.display();
      delay(3000);
    }

    // Powrót do stanu wolnego
    receivedValue = "";
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    display.setCursor(10, 25);
    display.print("WOLNE MIEJSCA: ");
    display.println(wolneMiejsca);
    display.display();
  }
}

