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


////////////////////////////////////////////////////////////////////////////////
//  1.    Konfiguracja OLED
////////////////////////////////////////////////////////////////////////////////

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);



////////////////////////////////////////////////////////////////////////////////
//    2.         Piny
////////////////////////////////////////////////////////////////////////////////

const int LED_ZIELONA = 26; //szlaban w gore
const int LED_CZERWONA = 27; //szlaban w dol i zajete miejsce
const int LDR_PIN= 32; // fotorezystor do wykrywania zajecia miejsca


////////////////////////////////////////////////////////////////////////////////
//    3.         WIFI MQTT NTP - zawarte w config.h
////////////////////////////////////////////////////////////////////////////////

#include "config.h"

WiFiClient espClient;
PubSubClient client(espClient);

////////////////////////////////////////////////////////////////////////////////
//    4.         ZMIENNE DO LOGIKI PARKINGU
////////////////////////////////////////////////////////////////////////////////

int wolneMiejsca = 5;
int mojeMiejsce=1;
String receivedValue = "";
unsigned long lastMsg = 0;
bool miejsceZajete = false;
String rejestracjaZajete = "";


////////////////////////////////////////////////////////////////////////////////
//    5.         ZMIENNE DO LOGIKI BLE
////////////////////////////////////////////////////////////////////////////////

volatile bool noweDaneBLE = false;
String rejestracjaBLE = "";

// UUID dla BLE 
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

class MyCallbacks: public BLECharacteristicCallbacks {
  public:
    void onWrite(BLECharacteristic *pCharacteristic) override {

        std::string value = pCharacteristic->getValue();
        
        Serial.print("\nNowa wiadomość. Liczba bajtów: ");
        Serial.println(value.length());
        
        if (value.length() > 0) {
          rejestracjaBLE = "";
          for (int i = 0; i < value.length(); i++)
            rejestracjaBLE += value[i];
        }
        Serial.print("Przechwycony tekst: ");
        Serial.println(rejestracjaBLE);

        noweDaneBLE = true; // Flaga do obsługi w loopie
      }
};


////////////////////////////////////////////////////////////////////////////////
//    6.         FUNKCJE
////////////////////////////////////////////////////////////////////////////////
void connectToWiFi();
void polMqtt();
void publishWjazd(String rejestracja);
void publishWyjazd(String rejestracja);
void setup();
void loop();
void controlSystem();
void oledEkranGlowny();
void oledSzlabanOtwarty(String rej);
void oledBrakMiejsc();
void oledWyjazdPojazdu(String rej);

////////////////////////////////////////////////////////////////////////////////
//    7.         IMPLEMENTACJA FUNKCJI
////////////////////////////////////////////////////////////////////////////////

//WiFi Connect
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
      client.subscribe(mqtt_topic);
    } else {
      Serial.print(" błąd rc=");
      Serial.print(client.state());
      Serial.println(" -> Ponawiam za 5 sekund...");
      delay(5000);
    }
  }
}

//Mqtt publish rejestracja wjazdu + wyjazdu
void publishWjazd(String rejestracja) {
  if (!client.connected()) {
    polMqtt();
  } 
  time_t now;
  time(&now);
  String payload = "{\"akcja\":\"WJAZD\",\"rejestracja\":\"" + rejestracja + "\",\"miejsce\":" + String(mojeMiejsce) + ",\"timestamp\":" + String(now) + "}";
  
  client.publish(mqtt_topic, payload.c_str());
  Serial.println("MQTT WJAZD: " + payload);
}

void publishWyjazd(String rejestracja) {
  if (!client.connected()) {
    polMqtt();
  }
  
  time_t now;
  time(&now);
  String payload = "{\"akcja\":\"WYJAZD\",\"rejestracja\":\"" + rejestracja + "\",\"miejsce\":" + String(mojeMiejsce) + ",\"timestamp\":" + String(now) + "}";
  
  client.publish(mqtt_topic, payload.c_str());
  Serial.println("MQTT WYJAZD: " + payload);
}

void controlSystem() {
  //// Kontrola systemu co 6 sekund
  unsigned long now = millis();
  static unsigned long lastMsg = 0;
  if (now - lastMsg > 6000) { 
    lastMsg = now;
    Serial.println("System działa, czekam na zdarzenia...");
    Serial.print(miejsceZajete ? "ZAJĘTE" : "WOLNE");
    Serial.print(" | Wolne miejsca: ");
    Serial.print(wolneMiejsca);
    Serial.print(" | Odczyt fotorezystora: ");
    Serial.println(analogRead(LDR_PIN));
  } 
}


////////////////////////////////////////////////////////////////////////////////
// 8. FUNKCJE EKRANU OLED 
////////////////////////////////////////////////////////////////////////////////

void oledEkranGlowny() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(10, 25);
  display.print("WOLNE MIEJSCA: ");
  display.println(wolneMiejsca);
  display.display();
}

void oledSzlabanOtwarty(String rej) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 10);
  display.println("AUTORYZACJA: " + rej);
  display.setTextSize(2);
  display.setCursor(0, 30);
  display.println(" OTWARTY ");
  display.display();
}

void oledBrakMiejsc() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0, 20);
  display.println(" BRAK ");
  display.println(" MIEJSC ");
  display.display();
  delay(3000);
}

void oledWyjazdPojazdu(String rej) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 20);
  display.println("WYJAZD POJAZDU:");
  display.println(rej);
  display.display();
}

////////////////////////////////////////////////////////////////////////////////
// 9. SETUP I GLOWNA LOGIKA W LOOPIE
////////////////////////////////////////////////////////////////////////////////

void setup() {
  Serial.begin(115200);
  pinMode(LED_ZIELONA, OUTPUT);
  pinMode(LED_CZERWONA, OUTPUT);
  pinMode(LDR_PIN, INPUT);

  digitalWrite(LED_ZIELONA, LOW);
  digitalWrite(LED_CZERWONA, LOW);

  display.begin(0x3C, true);
  oledEkranGlowny();

  // Konfiguracja BLE na czystej pamieci
  BLEDevice::init("SmartParking_IOT");
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
  
  connectToWiFi();
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  client.setServer(mqtt_server, mqtt_port);
}


void loop() {
  if(WiFi.status() != WL_CONNECTED) {
    connectToWiFi();
  }
  if (!client.connected()) {
    polMqtt();
  }
  client.loop();

  controlSystem();

  /////////////////////////////////////////////////////////////////////////////////////
  //Stan 1 - szlaban otwarty, dioda zielona swieci, oczekiwanie na zajecie miejsca przez samochod
  ////////////////////////////////////////////////////////////////////////////////

  if (!miejsceZajete && noweDaneBLE) {
    String rejestracja = rejestracjaBLE;
    noweDaneBLE = false; // Reset flagi
    Serial.println("Otrzymano żądanie wjazdu: " + rejestracja);

    if (wolneMiejsca > 0) {
      oledSzlabanOtwarty(rejestracja);
      digitalWrite(LED_ZIELONA, HIGH); //szlaban w gore
      digitalWrite(LED_CZERWONA, LOW);
      
      //oczekiwanie na zajecie miejsca przez samochód 15 sekund
      Serial.println("Szlaban otwarty dla: " + rejestracja + " Miejsce " + String(mojeMiejsce) +"czeka na zajęcie...  ");
      unsigned long startTime = millis();
      bool samochodZajalMiejsce = false;

      while (millis() - startTime < 15000) {
        int ldrValue = analogRead(LDR_PIN); //odczyt napiecia z pinu 32
        if (ldrValue < 1500) {   // Próg zajęcia miejsca, do dostosowania
          samochodZajalMiejsce = true;
          break;
        }
        delay(100);
    }

    ////////////////////////////////////////////////////////////////////////////////
    // Wjazd powiódł się - szlaban zamkniety miejsce zajete, dioda gasnie, publikacja wjazdu do MQTT
    ////////////////////////////////////////////////////////////////////////////////
    
    digitalWrite(LED_ZIELONA, LOW);

    if(samochodZajalMiejsce) {
      Serial.println("Miejsce zajęte przez: " + rejestracja);
      wolneMiejsca--;
      miejsceZajete = true; //miejsce zajete
      rejestracjaZajete = rejestracja; //zapamietanie rejestracji zajetego miejsca
  
      digitalWrite(LED_CZERWONA, HIGH);
      publishWjazd(rejestracja); //publikacja wjazdu do MQTT
    } 

    ////////////////////////////////////////////////////////////////////////////////
    // Wjazd nie powiódł się - szlaban zamkniety miejsce wolne,
    ////////////////////////////////////////////////////////////////////////////////

    else {
      Serial.println("Brak zajęcia miejsca przez: " + rejestracja);
      digitalWrite(LED_CZERWONA, LOW);
    }
    oledEkranGlowny();
   }

   ////////////////////////////////////////////////////////////////////////////////
   // Brak miejsc - szlaban zamkniety, komunikat na OLED
   ////////////////////////////////////////////////////////////////////////////////
   
   else {   
    oledBrakMiejsc();
    oledEkranGlowny();
  }
}

  /////////////////////////////////////////////////////////////////////////////////
  // Stan 2 - miejsce zajete, oczekiwanie na zwolnienie miejsca przez samochod, odczyt z fotorezystora, publikacja wyjazdu do MQTT
  /////////////////////////////////////////////////////////////////////////////////

  if (miejsceZajete) {
    int ldrValue = analogRead(LDR_PIN);
    if (ldrValue > 1500) {
      Serial.println("Miejsce zwolnione przez: " + rejestracjaZajete);
      wolneMiejsca++;
      miejsceZajete = false; //miejsce wolne
      digitalWrite(LED_CZERWONA, LOW);
      oledWyjazdPojazdu(rejestracjaZajete);
      publishWyjazd(rejestracjaZajete); //publikacja wyjazdu do MQTT
      rejestracjaZajete = ""; 
      delay(4000); //czas na przejazd samochodu i zamkniecie szlabanu
      oledEkranGlowny();
    }
  }

  delay(200); 
}