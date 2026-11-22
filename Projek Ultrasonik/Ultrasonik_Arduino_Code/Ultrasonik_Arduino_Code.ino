#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ==========================================
// KONFIGURASI LAYAR OLED SSD1306 (128x64 I2C)
// ==========================================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C  // Alamat I2C umum: 0x3C atau 0x3D
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ==========================================
// KONFIGURASI PIN HARDWARE
// ==========================================
const int PIN_TRIG  = 6;
const int PIN_ECHO  = 7;
const int PIN_RELAY = 8;

// Konfigurasi Trigger Relay (Mayoritas modul relay menggunakan Active LOW)
#define RELAY_ON  LOW
#define RELAY_OFF HIGH

// ==========================================
// PARAMETER KALIBRASI TANGKI & SET POINT (cm)
// ==========================================
const float TANK_HEIGHT = 15.0; // Tinggi total tangki dari dasar ke sensor (cm)
const float LRV = 2.0;          // Lower Range Value: Pompa ON jika <= 20 cm
const float URV = 10.0;          // Upper Range Value: Pompa OFF jika >= 80 cm

// Variabel Kontrol & Data
bool pumpState = false;
float waterLevel = 0.0;
float waterPercentage = 0.0;

// Filter Moving Average untuk kestabilan pembacaan
const int NUM_READINGS = 5;
float readings[NUM_READINGS];
int readIndex = 0;
float totalReadings = 0.0;

void setup() {
  Serial.begin(115200);

  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  pinMode(PIN_RELAY, OUTPUT);
  
  // Pastikan pompa mati di awal
  digitalWrite(PIN_RELAY, RELAY_OFF);

  // Inisialisasi array filter
  for (int i = 0; i < NUM_READINGS; i++) {
    readings[i] = 0.0;
  }

  // Inisialisasi OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("[ERROR] OLED SSD1306 tidak terdeteksi! Periksa wiring I2C."));
    while (true); // Berhenti jika OLED gagal
  }

  // Tampilan Splash Screen Awal
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(18, 15);
  display.println(F("WATER LEVEL SYSTEM"));
  display.setCursor(26, 32);
  display.println(F("SRF05 + OLED"));
  display.drawRect(14, 48, 100, 8, SSD1306_WHITE);
  display.fillRect(16, 50, 40, 4, SSD1306_WHITE);
  display.display();
  delay(1500);
}

void loop() {
  // 1. Baca Jarak dari Sensor SRF05
  float rawDistance = readUltrasonicSRF05();

  // Validasi nilai pembacaan
  if (rawDistance > 0 && rawDistance <= TANK_HEIGHT + 10.0) {
    // 2. Haluskan data dengan Moving Average
    totalReadings -= readings[readIndex];
    readings[readIndex] = rawDistance;
    totalReadings += readings[readIndex];
    readIndex = (readIndex + 1) % NUM_READINGS;
    float smoothDistance = totalReadings / NUM_READINGS;

    // 3. Hitung Ketinggian & Persentase Air
    waterLevel = TANK_HEIGHT - smoothDistance;
    if (waterLevel < 0.0) waterLevel = 0.0;
    if (waterLevel > TANK_HEIGHT) waterLevel = TANK_HEIGHT;

    waterPercentage = (waterLevel / TANK_HEIGHT) * 100.0;
  }

  // 4. Logika Kontrol Pompa (Hysteresis)
  if (waterLevel <= LRV) {
    pumpState = true;   // Nyalakan pompa jika di bawah/sama dengan batas bawah
  } else if (waterLevel >= URV) {
    pumpState = false;  // Matikan pompa jika mencapai/melebihi batas atas
  }
  // Di antara LRV dan URV, status pompa tetap mengikuti kondisi terakhir

  digitalWrite(PIN_RELAY, pumpState ? RELAY_ON : RELAY_OFF);

  // 5. Update Antarmuka Layar OLED
  drawOLEDUI();

  // 6. Kirim Data ke Serial Monitor & Arduino IDE Serial Plotter
  sendDashboardSerial();

  delay(200); // Sampling rate ~5 Hz
}

// ==========================================
// FUNGSI MEMBACA SENSOR ULTRASONIK SRF05
// ==========================================
float readUltrasonicSRF05() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);

  // Timeout 30000 us (~5 meter jarak pantul maks)
  long duration = pulseIn(PIN_ECHO, HIGH, 30000);
  if (duration == 0) return -1.0;

  // Rumus kecepatan suara: 0.0343 cm/us dibagi 2 (pulang-pergi)
  return (duration * 0.0343) / 2.0;
}

// ==========================================
// TAMPILAN MONITORING ATRAKTIF PADA OLED
// ==========================================
void drawOLEDUI() {
  display.clearDisplay();

  // --- Header Bar ---
  display.fillRect(0, 0, 128, 11, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(16, 2);
  display.print(F("TANK LEVEL MONITOR"));

  // --- Indikator Tangki Air Visual (Sebelah Kiri) ---
  // Bingkai luar tangki
  display.drawRect(4, 15, 24, 47, SSD1306_WHITE);
  // Indikator garis URV & LRV pada tangki
  int lrvPixelY = 15 + (47 - (int)((LRV / TANK_HEIGHT) * 45));
  int urvPixelY = 15 + (47 - (int)((URV / TANK_HEIGHT) * 45));
  display.drawFastHLine(2, lrvPixelY, 4, SSD1306_WHITE); // Penanda LRV
  display.drawFastHLine(2, urvPixelY, 4, SSD1306_WHITE); // Penanda URV

  // Isi tangki sesuai level air riil
  int fillHeight = (int)((waterLevel / TANK_HEIGHT) * 43.0);
  if (fillHeight > 0) {
    display.fillRect(6, 15 + 45 - fillHeight, 20, fillHeight, SSD1306_WHITE);
  }

  // --- Data Numerik & Status (Sebelah Kanan) ---
  display.setTextColor(SSD1306_WHITE);
  
  // Nilai Level (Ukuran Besar)
  display.setTextSize(2);
  display.setCursor(34, 16);
  display.print(waterLevel, 1);
  display.setTextSize(1);
  display.setCursor(96, 16);
  display.print(F("cm"));

  // Persentase (%) & Info Set Point
  display.setCursor(34, 34);
  display.print(F("[ "));
  display.print(waterPercentage, 0);
  display.print(F("% ]"));

  display.setCursor(34, 44);
  display.print(F("L:"));
  display.print((int)LRV);
  display.print(F(" U:"));
  display.print((int)URV);

  // Status Badge Pompa
  if (pumpState) {
    display.fillRoundRect(34, 53, 90, 10, 2, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    display.setCursor(44, 54);
    display.print(F("PUMP: FILLING"));
  } else {
    display.drawRoundRect(34, 53, 90, 10, 2, SSD1306_WHITE);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(50, 54);
    display.print(F("PUMP: IDLE"));
  }

  display.display();
}

// ==========================================
// MONITORING DASHBOARD DI ARDUINO IDE
// ==========================================
void sendDashboardSerial() {
  // Format standar multi-label untuk "Arduino IDE Serial Plotter"
  // Format: "Label1:Nilai1,Label2:Nilai2,..."
  Serial.print(F("Level_cm:"));
  Serial.print(waterLevel, 2);
  Serial.print(F(","));
  Serial.print(F("LRV_Setpoint:"));
  Serial.print(LRV);
  Serial.print(F(","));
  Serial.print(F("URV_Setpoint:"));
  Serial.print(URV);
  Serial.print(F(","));
  Serial.print(F("Pompa_Aktif:"));
  Serial.println(pumpState ? 100 : 0); // Diplot 100 saat ON, 0 saat OFF
}
