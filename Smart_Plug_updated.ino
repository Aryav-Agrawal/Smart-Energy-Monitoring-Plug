/*************************************************
   ESP32 Smart Socket (WITH POWER + ENERGY)
   ACS712 + Blynk IoT + OLED SSD1306
**************************************************/

#define BLYNK_TEMPLATE_ID "TMPL3C2lP5vRG"
#define BLYNK_TEMPLATE_NAME "Smart Socket"
#define BLYNK_AUTH_TOKEN "msvfqySJSsyfqd7dLn0CkIpiIJiOtrYP"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>

char ssid[] = "Aryav's S24";
char pass[] = "aryav09?";

// -------------------- OLED Display --------------------
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// -------------------- Relay + ACS712 --------------------
#define RELAY_PIN 27
#define ACS_PIN   34

const float ADC_REF = 3.3f;
const int   ADC_MAX = 4095;

float SENSITIVITY = 1.63f;
float offsetVoltage = 0.0;
bool relayState = false;

// -------------------- NEW VARIABLES --------------------
float voltage = 230.0;
float power = 0.0;
float energy = 0.0;

unsigned long lastTime = 0;

// ----------------------------------------------------
// BLYNK → Relay control
// ----------------------------------------------------
BLYNK_WRITE(V0)
{
  int value = param.asInt();
  if (value == 1) {
    digitalWrite(RELAY_PIN, LOW);
    relayState = true;
  } else {
    digitalWrite(RELAY_PIN, HIGH);
    relayState = false;
  }
}

// ----------------------------------------------------
// Offset Calibration
// ----------------------------------------------------
void calibrateOffset() {
  long sum = 0;
  const int N = 500;

  for (int i = 0; i < N; i++) {
    sum += analogRead(ACS_PIN);
    delay(2);
  }

  float avg = sum / (float)N;
  offsetVoltage = (avg / ADC_MAX) * ADC_REF;
}

// ----------------------------------------------------
// RMS reading
// ----------------------------------------------------
float measureVrms(unsigned long windowMs = 200) {
  unsigned long start = millis();
  double sumSq = 0.0;
  unsigned long samples = 0;

  while (millis() - start < windowMs) {
    int raw = analogRead(ACS_PIN);
    float v = (raw / (float)ADC_MAX) * ADC_REF;
    float d = v - offsetVoltage;

    sumSq += d * d;
    samples++;
  }
  return (samples == 0) ? 0.0 : sqrt(sumSq / samples);
}

// ----------------------------------------------------
// OLED Display Function (UPDATED)
// ----------------------------------------------------
void updateOLED(float current) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  display.setCursor(0, 0);
  display.print("Relay: ");
  display.println(relayState ? "ON" : "OFF");

  display.setCursor(0, 16);
  display.print("I: ");
  display.print(current, 2);
  display.println(" A");

  display.setCursor(0, 32);
  display.print("P: ");
  display.print(power, 1);
  display.println(" W");

  display.setCursor(0, 48);
  display.print("E: ");
  display.print(energy, 3);
  display.println(" kWh");

  display.display();
}

// ----------------------------------------------------
// SETUP
// ----------------------------------------------------
void setup() {
  Serial.begin(115200);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);

  // OLED Init
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Smart Socket");
  display.println("Connecting...");
  display.display();

  // ADC config
  analogReadResolution(12);
  analogSetPinAttenuation(ACS_PIN, ADC_11db);
  delay(300);

  calibrateOffset();

  // Connect Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.println("READY");
  display.display();

  lastTime = millis();   // initialize time
}

// ----------------------------------------------------
// LOOP
// ----------------------------------------------------
void loop() {
  Blynk.run();

  float Irms = 0.0;

  if (relayState) {
    float Vrms = measureVrms(200);
    if (Vrms > 0.0f) Irms = Vrms / SENSITIVITY;
    if (Irms < 0.02f) Irms = 0.0f;

    // -------- POWER --------
    power = voltage * Irms;

    // -------- ENERGY --------
    unsigned long currentTime = millis();
    float timeHours = (currentTime - lastTime) / 3600000.0;

    energy += (power * timeHours) / 1000.0;

    lastTime = currentTime;
  } else {
    power = 0.0;
  }

  // Serial Output
  Serial.printf("Relay: %s  I: %.3f A  P: %.2f W  E: %.4f kWh\n",
                relayState ? "ON" : "OFF", Irms, power, energy);

  // Send to Blynk
  Blynk.virtualWrite(V1, Irms);
  Blynk.virtualWrite(V2, power);
  Blynk.virtualWrite(V3, energy);

  // OLED Update
  updateOLED(Irms);

  delay(300);
}