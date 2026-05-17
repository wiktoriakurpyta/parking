#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <PubSubClient.h>
#include <WiFi.h>



///////////proba1 szlaban

// Konfiguracja OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);


//wifi i mqtt
const char* ssid="wifi_ssid";
const char* password="password";
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600; // Strefa czasowa 
const int daylightOffset_sec = 3600; // Czas letni

WiFiClient espClient;
PubSubClient client(espClient);

// Zmienne systemowe
int wolneMiejsca = 5;
const int LED_ZIELONA = 26;
const int LED_CZERWONA = 27;
int mojeMiejsce=1;
String receivedValue = "";
unsigned long lastMsg = 0;

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


//WifiConnect
void connectToWiFi() {
  Serial.print("Łączenie z WiFi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(WiFi.localIP());
  }
  else {
    Serial.println("Nie można połączyć z WiFi");
  }
}

//MQTT Connect
void polMqtt() {
  while (!client.connected()) {
    if (WiFi.status() != WL_CONNECTED) {
      connectToWiFi();
    }

    Serial.print("Łączenie z brokerem HiveMQ...");
    
    // Losowe ID klienta
    String clientId = "Parking_Client_" + String(random(0, 10000));
    
    if (client.connect(clientId.c_str())) {
      Serial.println(" POŁĄCZONO!");
      //topic
      client.subscribe("topic");
    } else {
      Serial.print(" błąd rc=");
      Serial.print(client.state());
      Serial.println(" -> Ponawiam za 5 sekund...");
      delay(5000);
    }
  }
}

//Mqtt publish rejestracja wjazdu
void publishWjazd(String rejestracja) {
  if (!client.connected()) {
    polMqtt();
  }
  
  time_t now;
  time(&now);
  // Format: {"akcja":"WJAZD", "rejestracja":"KR12345", "miejsce":1, "timestamp":1234567890}
  String payload = "{\"akcja\":\"WJAZD\",\"rejestracja\":\"" + rejestracja + "\",\"miejsce\":" + String(mojeMiejsce) + ",\"timestamp\":" + String(now) + "}";
  
  client.publish("topic", payload.c_str());
  Serial.println("MQTT WJAZD: " + payload);
}


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

  connectToWiFi();

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  client.setServer(mqtt_server, mqtt_port);

  // Konfiguracja BLE
  BLEDevice::init("Parking_ESP32");
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
  if(WiFi.status() != WL_CONNECTED) {
    connectToWiFi();
  }
  if (!client.connected()) {
    polMqtt();
  }
  client.loop();

  if (receivedValue != "") {
    String rejestracja = receivedValue;
    Serial.println("Otrzymano: " + rejestracja);

    if (wolneMiejsca > 0) {
    
    // SZLABAN OTWARTY
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 10);
    display.println("AUTORYZACJA: " + rejestracja);
    display.setTextSize(2);
    display.setCursor(0, 30);
    display.println(" OTWARTY ");
    display.display();
    
    digitalWrite(LED_ZIELONA, HIGH);
    digitalWrite(LED_CZERWONA, LOW);
    delay(5000);
    //wjazd powiódł sie
    digitalWrite(LED_ZIELONA, LOW);
    digitalWrite(LED_CZERWONA, HIGH); //szlaban zamkniety miejsce zajete
    wolneMiejsca--;
    
    publishWjazd(rejestracja); //publikacja wjazdu do MQTT
 
    } else {

       //brak miejsc

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

