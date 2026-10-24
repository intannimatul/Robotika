/*******************************************************************************
 * MODUL 04: Blynk Control, RPM Telemetry & OTA Update iMCLab (ESP32)
 * Deskripsi: Program kendali Motor DC, Telemetry RPM, dan pembaruan firmware 
 *            nirkabel Over-The-Air (OTA) via Blynk IoT Cloud & ArduinoOTA.
 * Compatible: ESP32 Arduino Core 2.x & 3.x
 *******************************************************************************/

#define BLYNK_TEMPLATE_ID "TMPL6htwVkLiD"
#define BLYNK_TEMPLATE_NAME "iMCLab Motor"
#define BLYNK_AUTH_TOKEN "9MAsSFl2_mmfWPpwK3Ecn11j4ER1mkSX"

#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <ArduinoOTA.h> // Pustaka Utama untuk Fitur Over-The-Air (Modul 04)

char auth[] = BLYNK_AUTH_TOKEN;
char ssid[] = "enf";
char pass[] = "apakahitu";

// ============================================================
// PINOUT HARDWARE & PARAMETER UTAMA
// ============================================================
const int PIN_LED          = 2;     // LED Indikator Onboard
const int PIN_MOTOR_IN1    = 27;    // Input 1 Driver Motor
const int PIN_MOTOR_IN2    = 26;    // Input 2 Driver Motor
const int PIN_MOTOR_ENA    = 12;    // Enable A (PWM Kecepatan)
const byte PIN_RPM_SENSOR  = 13;    // Sensor Speed Encoder

// Parameter PWM Motor
const int PWM_FREQ         = 30000; // 30 kHz
const int PWM_CHANNEL      = 0;
const int PWM_RESOLUTION   = 8;     // 8-bit (0 - 255)
const int DUTY_STEP        = 10;
const int DUTY_MIN         = 0;
const int DUTY_MAX         = 255;

int speedDutyCycle         = 200;
bool isMotorActive         = false;

// Parameter Sensor RPM
const int ENCODER_HOLES    = 2;
volatile unsigned int pulseCount = 0;
volatile unsigned long lastDebounceUs = 0;
const unsigned long DEBOUNCE_TIME_US = 500;

float currentRPM           = 0.0;
unsigned long lastRpmCalcTime = 0;

String serialBuffer        = "";
BlynkTimer blynkTimer;

// ============================================================
// ABSTRAKSI DRIVER PWM (KOMPATIBEL CORE 2.X & 3.X)
// ============================================================
void initPWM() {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(PIN_MOTOR_ENA, PWM_FREQ, PWM_RESOLUTION);
#else
  ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(PIN_MOTOR_ENA, PWM_CHANNEL);
#endif
}

void writePWM(int duty) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(PIN_MOTOR_ENA, duty);
#else
  ledcWrite(PWM_CHANNEL, duty);
#endif
}

// ============================================================
// INTERRUPT SERVICE ROUTINE (ISR) ENCODER
// ============================================================
void IRAM_ATTR encoderISR() {
  unsigned long now = micros();
  if (now - lastDebounceUs > DEBOUNCE_TIME_US) {
    pulseCount++;
    lastDebounceUs = now;
  }
}

// ============================================================
// LOGIKA KONTROL MOTOR & HITUNG RPM
// ============================================================
void applyMotorControl() {
  digitalWrite(PIN_MOTOR_IN1, HIGH);
  digitalWrite(PIN_MOTOR_IN2, LOW);

  writePWM(speedDutyCycle);
  digitalWrite(PIN_LED, speedDutyCycle > 0 ? HIGH : LOW);

  Blynk.virtualWrite(V1, speedDutyCycle);
  Blynk.virtualWrite(V2, speedDutyCycle);
  Blynk.virtualWrite(V3, speedDutyCycle > 0 ? 1 : 0);
}

void computeRPM() {
  unsigned long now = millis();
  if (now - lastRpmCalcTime >= 1000) {
    detachInterrupt(digitalPinToInterrupt(PIN_RPM_SENSOR));

    float revPerSec = (float)pulseCount / (float)ENCODER_HOLES;
    currentRPM = revPerSec * 60.0;
    pulseCount = 0;

    attachInterrupt(digitalPinToInterrupt(PIN_RPM_SENSOR), encoderISR, FALLING);
    lastRpmCalcTime = millis();
  }
}

void pushTelemetryToBlynk() {
  Blynk.virtualWrite(V0, currentRPM);
  Blynk.virtualWrite(V1, speedDutyCycle);
  Blynk.virtualWrite(V2, speedDutyCycle);
  Blynk.virtualWrite(V3, speedDutyCycle > 0 ? 1 : 0);
}

// ============================================================
// INISIALISASI ARDUINO OTA (PENTING MODUL 04)
// ============================================================
void setupOTA() {
  ArduinoOTA.setHostname("iMCLab-ESP32-OTA");

  ArduinoOTA.onStart([]() {
    String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
    Serial.println("[OTA] Proses Update Dimulai: " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\n[OTA] Update Selesai! Rebooting...");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("[OTA] Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("[OTA] Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });

  ArduinoOTA.begin();
  Serial.println("[OTA] Service ArduinoOTA Siap!");
}

// ============================================================
// BLYNK VIRTUAL PIN HANDLERS
// ============================================================
BLYNK_CONNECTED() {
  Blynk.syncVirtual(V2);
  Blynk.syncVirtual(V3);
}

BLYNK_WRITE(V2) {
  speedDutyCycle = constrain(param.asInt(), DUTY_MIN, DUTY_MAX);
  isMotorActive = (speedDutyCycle > 0);
  applyMotorControl();
  Serial.printf("[BLYNK] Set Duty Cycle: %d\n", speedDutyCycle);
}

BLYNK_WRITE(V3) {
  isMotorActive = param.asInt();
  if (isMotorActive) {
    if (speedDutyCycle == 0) speedDutyCycle = 200;
  } else {
    speedDutyCycle = 0;
  }
  applyMotorControl();
  Serial.printf("[BLYNK] Motor Status: %s\n", isMotorActive ? "ON" : "OFF");
}

// ============================================================
// PARSER PERINTAH SERIAL (CLI)
// ============================================================
void processSerialCommand(String cmd) {
  cmd.trim();
  if (cmd.startsWith("PWM:")) {
    int val = cmd.substring(4).toInt();
    speedDutyCycle = constrain(val, DUTY_MIN, DUTY_MAX);
    isMotorActive = (speedDutyCycle > 0);
    applyMotorControl();
    Serial.printf("[CLI] Direct PWM Set -> %d | RPM: %.1f\n", speedDutyCycle, currentRPM);
    return;
  }

  if (cmd.length() == 0) return;
  char option = cmd.charAt(0);

  switch (option) {
    case '1':
      if (speedDutyCycle == 0) speedDutyCycle = 200;
      isMotorActive = true;
      applyMotorControl();
      Serial.printf("[CLI] Motor ON | PWM: %d | RPM: %.1f\n", speedDutyCycle, currentRPM);
      break;

    case '0':
      speedDutyCycle = 0;
      isMotorActive = false;
      applyMotorControl();
      Serial.println("[CLI] Motor OFF");
      break;

    case '+':
      speedDutyCycle = min(speedDutyCycle + DUTY_STEP, DUTY_MAX);
      isMotorActive = true;
      applyMotorControl();
      Serial.printf("[CLI] Kecepatan NAIK -> PWM: %d | RPM: %.1f\n", speedDutyCycle, currentRPM);
      break;

    case '-':
      speedDutyCycle = max(speedDutyCycle - DUTY_STEP, DUTY_MIN);
      isMotorActive = (speedDutyCycle > 0);
      applyMotorControl();
      Serial.printf("[CLI] Kecepatan TURUN -> PWM: %d | RPM: %.1f\n", speedDutyCycle, currentRPM);
      break;

    case 'R':
    case 'r':
      Serial.printf("[TELEMETRY] RPM: %.1f | PWM: %d\n", currentRPM, speedDutyCycle);
      break;

    default:
      Serial.println("[CLI] Perintah tidak dikenal! Gunakan 1, 0, +, -, PWM:xxx, atau R.");
      break;
  }
}

// ============================================================
// SETUP & LOOP PROGRAM
// ============================================================
void setup() {
  Serial.begin(115200);

  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_MOTOR_IN1, OUTPUT);
  pinMode(PIN_MOTOR_IN2, OUTPUT);
  pinMode(PIN_MOTOR_ENA, OUTPUT);
  pinMode(PIN_RPM_SENSOR, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(PIN_RPM_SENSOR), encoderISR, FALLING);

  initPWM();

  digitalWrite(PIN_MOTOR_IN1, HIGH);
  digitalWrite(PIN_MOTOR_IN2, LOW);
  writePWM(0);

  lastRpmCalcTime = millis();

  // Koneksi ke Blynk Cloud
  Blynk.begin(auth, ssid, pass);
  blynkTimer.setInterval(1000L, pushTelemetryToBlynk);

  // Inisialisasi Fitur OTA
  setupOTA();

  Serial.println("==================================================");
  Serial.println("   iMCLab ESP32 - Blynk, RPM & OTA Ready          ");
  Serial.println("==================================================");
}

void loop() {
  Blynk.run();
  blynkTimer.run();
  ArduinoOTA.handle(); // WAJIB: Memproses sinyal pembaruan firmware nirkabel
  computeRPM();

  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (serialBuffer.length() > 0) {
        processSerialCommand(serialBuffer);
        serialBuffer = "";
      }
    } else {
      serialBuffer += c;
    }
  }
}