#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <PZEM004Tv30.h>

#define PZEM_RX_PIN 22     // ESP32 D22 (RX)
#define PZEM_TX_PIN 23     // ESP32 D23 (TX)
#define SSR_PIN 12         // SSR dikontrol dari GPIO 12
#define OVER_CURRENT_LIMIT 9.9  // Batas arus maksimum (A)

HardwareSerial PZEMSerial(1);
PZEM004Tv30 pzem(PZEMSerial, PZEM_RX_PIN, PZEM_TX_PIN);
LiquidCrystal_I2C lcd(0x27, 16, 2); // Alamat I2C 0x27, LCD 16x2

// WiFi Settings
const char* ssid = "IphoneProMax";
const char* password = "password";
const char* backendURL = "http://api-powerhouse.tk2b.my.id/api/iot/data";

// Variabel sistem
unsigned long previousLCDMillis = 0;
unsigned long previousMillis = 0;
const long lcdUpdateInterval = 1000; // Update LCD tiap 1 detik
const long interval = 5000; // Interval pengiriman data ke backend (5 detik)
const float TARIF_LISTRIK = 1352.0; // Tarif listrik per kWh
const float CURRENT_THRESHOLD = 0.04; // Batas minimum arus (40mA)
bool ssrState = true; // Status awal SSR (ON)

void setup() {
  Serial.begin(115200);
  
  // Inisialisasi PZEM
  PZEMSerial.begin(9600, SERIAL_8N1, PZEM_RX_PIN, PZEM_TX_PIN);
  pzem.setAddress(0xF8);
  
  // Inisialisasi LCD
  Wire.begin(19, 18); // SDA=GPIO19, SCL=GPIO18
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi...");

  // Inisialisasi SSR
  pinMode(SSR_PIN, OUTPUT);
  digitalWrite(SSR_PIN, HIGH); // SSR ON awal

  // Koneksi WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi Connected!");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP());
  delay(2000);
  lcd.clear();

  Serial.println("\nWiFi Connected");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  configTime(0, 0, "pool.ntp.org");
}

void loop() {
  unsigned long currentMillis = millis();
  
  // Baca data sensor setiap 1 detik
  if (currentMillis - previousMillis >= 1000) {
    float voltage = pzem.voltage();
    float current = pzem.current();
    float power = pzem.power();
    float energy = pzem.energy();
    float pf = pzem.pf();

    // Filter noise
    if (pf == 0 || current < CURRENT_THRESHOLD) {
      current = 0;
      power = 0;
    }

    // Update LCD
    if (currentMillis - previousLCDMillis >= lcdUpdateInterval) {
    previousLCDMillis = currentMillis;
      if (!isnan(voltage)) {
        updateLCD(voltage, current, power, energy, pf);
      }
    }
  }

  // Kirim data ke backend setiap 5 detik
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    
    float voltage = pzem.voltage();
    float current = pzem.current();
    float power = pzem.power();
    float energy = pzem.energy();
    float pf = pzem.pf();

    if(!isnan(voltage)) {
      // Kirim data ke backend
      sendToBackend(voltage, current, power, energy, pf);
      
      // Proteksi over current
      if (current > OVER_CURRENT_LIMIT) {
        setSSR(false);
        Serial.println("⚠️ Arus melebihi batas! SSR dimatikan.");
      }
    }
    
    // Cek update status SSR dari backend
    checkSSRState();
  }
}

void sendToBackend(float voltage, float current, float power, float energy, float pf) {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClient client;
    HTTPClient http;
    
    http.begin(client, backendURL);  // Gunakan client secure
    http.addHeader("Content-Type", "application/json");
    
    // Verifikasi koneksi sebelum mengirim data
    if (!client.connect("api-powerhouse.tk2b.my.id", 80)) {  // Port 80 (HTTP)
      Serial.println("Gagal terhubung ke server");
      return;
    }
    
    String payload = String("{") +
      "\"voltage\":" + String(voltage) + "," +
      "\"current\":" + String(current) + "," +
      "\"power\":" + String(power) + "," +
      "\"energy\":" + String(energy, 4) + "," +
      "\"pf\":" + String(pf, 2) + "," +
      "\"ssr_state\":" + String(ssrState ? "true" : "false") +
    "}";

    int httpCode = http.POST(payload);
    if (httpCode == 200) {
      Serial.println("Data terkirim ke backend");
    } else {
      Serial.printf("Error: %d - %s\n", httpCode, http.errorToString(httpCode).c_str());
    }
    http.end();
  }
}

void checkSSRState() {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClient client;
    HTTPClient http;

    http.begin(client, String(backendURL) + "/ssr-status");

    int httpCode = http.GET();
    
    if (httpCode == 200) {
      String response = http.getString();
      bool newState = (response == "true");
      if (newState != ssrState) {
        setSSR(newState);
      }
    }
    http.end();
  }
}

void setSSR(bool state) {
  ssrState = state;
  digitalWrite(SSR_PIN, state ? HIGH : LOW);
  Serial.println("SSR diubah ke: " + String(state ? "ON" : "OFF"));
  
  // Tampilkan status di LCD baris kedua
  lcd.setCursor(13, 1);
  lcd.print(state ? "ON " : "OFF");
}

void updateLCD(float voltage, float current, float power, float energy, float pf) {
  lcd.clear();
  
  // Baris 1: Tegangan dan Daya
  lcd.setCursor(0, 0);
  lcd.print(energy, 3); // Tampilkan 3 digit desimal
  lcd.print("kWh");
  lcd.setCursor(14, 0);
  lcd.print(ssrState ? "ON " : "OFF");

  // Baris 2: Menampilkan energi (kWh) dan nilai uang
  float cost = energy * TARIF_LISTRIK;
  lcd.setCursor(0, 1);
  lcd.print(formatRupiah(cost));
  
  // Hitung nilai uang
  lcd.setCursor(11, 1);
  lcd.print(current, 2);
  lcd.print("A");
}

String formatRupiah(unsigned long angka) {
  String hasil = "";
  String strAngka = String(angka);

  int len = strAngka.length();
  int hitung = 0;

  for (int i = len - 1; i >= 0; i--) {
    hasil = strAngka[i] + hasil;
    hitung++;
    if (hitung % 3 == 0 && i != 0) {
      hasil = "." + hasil;
    }
  }

  return "Rp." + hasil;
}
