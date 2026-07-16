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
const char* ssid = "YaraMaisa";
const char* password = "Ninofareza94";
// URL Backend baru (gunakan IP server)
String serverUrl = "http://10.246.88.77:8000/api/data";

// Variabel sistem
unsigned long previousSensorMillis = 0;
unsigned long previousBackendMillis = 0;
unsigned long previousLCDMillis = 0;
unsigned long previousPageMillis = 0;

const long sensorInterval = 1000;      // Baca sensor tiap 1 detik
const long backendInterval = 5000;     // Cek perubahan tiap 5 detik
const long lcdUpdateInterval = 1000;   // Update LCD tiap 1 detik
const long pageChangeInterval = 20000; // Ganti halaman tiap 20 detik

const float TARIF_LISTRIK = 1352.0;    // Tarif listrik per kWh
const float CURRENT_THRESHOLD = 0.04;  // Batas minimum arus (40mA)

bool ssrState = true;
bool lastSendSuccess = false;  // Status pengiriman terakhir
int currentPage = 0;  // 0 = Halaman 1, 1 = Halaman 2

// Variabel untuk menyimpan data sensor
float voltage = 0, current = 0, power = 0, energy = 0, pf = 0;

// Variabel untuk menyimpan data sebelumnya (deteksi perubahan)
float lastVoltage = -1, lastCurrent = -1, lastPower = -1;
float lastEnergy = -1, lastPf = -1;
bool lastSsrState = false;

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
  digitalWrite(SSR_PIN, LOW); // SSR ON awal
  lastSsrState = true;

  // Koneksi WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  // Tampilkan IP Address selama 8 detik
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi Connected!");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP());
  delay(8000);
  lcd.clear();

  Serial.println("\nWiFi Connected");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  configTime(0, 0, "pool.ntp.org");
}

void loop() {
  unsigned long currentMillis = millis();
  
  // Baca sensor setiap 1 detik
  if (currentMillis - previousSensorMillis >= sensorInterval) {
    previousSensorMillis = currentMillis;
    
    voltage = pzem.voltage();
    current = pzem.current();
    power = pzem.power();
    energy = pzem.energy();
    pf = pzem.pf();

    // Filter noise
    if (pf == 0 || current < CURRENT_THRESHOLD) {
      current = 0;
      power = 0;
    }

    // Update LCD setiap 1 detik
    if (currentMillis - previousLCDMillis >= lcdUpdateInterval) {
      previousLCDMillis = currentMillis;
      if (!isnan(voltage)) {
        updateLCD(voltage, current, power, energy, pf, currentPage);
      }
    }
  }

  // Ganti halaman LCD setiap 20 detik
  if (currentMillis - previousPageMillis >= pageChangeInterval) {
    previousPageMillis = currentMillis;
    currentPage = (currentPage == 0) ? 1 : 0;
    Serial.println("Ganti ke halaman: " + String(currentPage + 1));
  }

  // Cek dan kirim data jika ada perubahan (setiap 5 detik)
  if (currentMillis - previousBackendMillis >= backendInterval) {
    previousBackendMillis = currentMillis;
    
    if (!isnan(voltage)) {
      // Cek apakah ada perubahan data
      bool dataChanged = false;
      
      if (abs(voltage - lastVoltage) > 0.1 || 
          abs(current - lastCurrent) > 0.01 || 
          abs(power - lastPower) > 0.1 || 
          abs(energy - lastEnergy) > 0.001 || 
          abs(pf - lastPf) > 0.01 || 
          ssrState != lastSsrState) {
        dataChanged = true;
      }
      
      // Kirim hanya jika ada perubahan
      if (dataChanged) {
        bool sendSuccess = sendToBackend(voltage, current, power, energy, pf);
        
        if (sendSuccess) {
          // Update nilai terakhir yang berhasil dikirim
          lastVoltage = voltage;
          lastCurrent = current;
          lastPower = power;
          lastEnergy = energy;
          lastPf = pf;
          lastSsrState = ssrState;
          lastSendSuccess = true;
          Serial.println("✅ Data berhasil dikirim (ada perubahan)");
        } else {
          lastSendSuccess = false;
          Serial.println("❌ Gagal mengirim data");
        }
      } else {
        // Tidak ada perubahan, tidak kirim data
        Serial.println("⏸️ Tidak ada perubahan data, skip kirim");
      }
    }
    
    // Cek update status SSR dari backend (tetap dilakukan meskipun data tidak berubah)
    checkSSRState();
  }
}

bool sendToBackend(float voltage, float current, float power, float energy, float pf) {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClient client;
    HTTPClient http;
    
    http.begin(client, serverUrl);  // Gunakan URL baru
    http.addHeader("Content-Type", "application/json");
    
    String payload = String("{") +
      "\"voltage\":" + String(voltage) + "," +
      "\"current\":" + String(current) + "," +
      "\"power\":" + String(power) + "," +
      "\"energy\":" + String(energy, 4) + "," +
      "\"pf\":" + String(pf, 2) + "," +
      "\"ssr_state\":" + String(ssrState ? "true" : "false") +
    "}";

    int httpCode = http.POST(payload);
    http.end();
    
    if (httpCode == 200 || httpCode == 201) {
      return true;
    } else {
      Serial.printf("Error: %d\n", httpCode);
      return false;
    }
  }
  return false;
}

void checkSSRState() {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClient client;
    HTTPClient http;
    http.begin(client, serverUrl + "/ssr-status");

    int httpCode = http.GET();
    
    if (httpCode == 200) {
      String response = http.getString();
      bool newState = (response == "true");
      if (newState != ssrState) {
        setSSR(newState);
        // SSR berubah, akan trigger kirim data di next cycle
        lastSsrState = ssrState; // Update agar terdeteksi perubahan
      }
    }
    http.end();
  }
}

void setSSR(bool state) {
  ssrState = state;
  digitalWrite(SSR_PIN, state ? LOW : HIGH);
  Serial.println("SSR diubah ke: " + String(state ? "ON" : "OFF"));
}

void updateLCD(float voltage, float current, float power, float energy, float pf, int page) {
  lcd.clear();
  
  if (page == 0) {
    // ============ HALAMAN 1 ============
    // Baris 1: kWh dan Watt
    lcd.setCursor(0, 0);
    lcd.print(energy, 3);
    lcd.print("kWh ");
    lcd.print(power, 1);
    lcd.print("W");
    
    // Baris 2: Rupiah dan Status Kirim
    float cost = energy * TARIF_LISTRIK;
    lcd.setCursor(0, 1);
    lcd.print("Rp");
    lcd.print(cost, 0);
    lcd.setCursor(13, 1);
    
    // Tampilkan "ON" hanya jika berhasil kirim ke server
    if (lastSendSuccess) {
      lcd.print("ON");
    } else {
      lcd.print("  "); // Kosong jika gagal kirim
    }
    
  } else {
    // ============ HALAMAN 2 ============
    // Baris 1: Volt dan Watt
    lcd.setCursor(0, 0);
    lcd.print(voltage, 1);
    lcd.print("V ");
    lcd.print(power, 1);
    lcd.print("W");
    
    // Baris 2: Ampere dan Power Factor
    lcd.setCursor(0, 1);
    lcd.print(current, 2);
    lcd.print("A ");
    lcd.print("PF:");
    lcd.print(pf, 2);
    lcd.setCursor(13, 1);
  }
}