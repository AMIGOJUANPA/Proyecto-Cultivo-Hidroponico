/*
 * Sistema hidropónico de recirculación — Arduino MEGA 2560
 * ---------------------------------------------------------
 * Bomba 12 V DC con módulo de relé JQC-3FF-S-Z.
 * HC-SR04 para nivel, DS18B20 para temperatura, TDS Meter V1.0 para EC.
 * LCD I2C 20x4 (configurable a 16x2).
 *
 * SEGURIDAD:
 * - Sin flotadores independientes, este firmware es un prototipo.
 * - El lazo de seguridad recomendado es un relé de seguridad cableado
 *   en serie con la bomba y controlado por los flotadores NC.
 * - El relé principal debe quedar OFF durante el reset y ante falla.
 *
 * Librerías necesarias:
 *   LiquidCrystal_I2C, OneWire, DallasTemperature
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <EEPROM.h>
#include <avr/wdt.h>
#include <avr/io.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#include "config.h"

// ============================================================
//  OBJETOS
// ============================================================
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);
OneWire oneWire(PIN_DS18B20);
DallasTemperature sensors(&oneWire);

// ============================================================
//  CALIBRACIÓN
// ============================================================
struct Calibration {
  uint32_t magic;
  uint16_t version;
  float ecSlope;
  float ecOffset;
  float sensorHeightCm;
  float levelLowCm;
  float levelHighCm;
  uint16_t crc;
};

Calibration calib;

// ============================================================
//  FALLAS
// ============================================================
enum Fault {
  FAULT_LOW_LEVEL    = 1 << 0,
  FAULT_HIGH_LEVEL   = 1 << 1,
  FAULT_LEVEL_SENSOR = 1 << 2,
  FAULT_MAX_RUN      = 1 << 3,
  FAULT_NO_FLOW      = 1 << 4,
  FAULT_WATCHDOG     = 1 << 5
};

uint16_t faultFlags = 0;

// ============================================================
//  ESTADO
// ============================================================
enum State { ST_INIT, ST_IDLE, ST_IRRIGATING, ST_FAULT };
State state = ST_INIT;

bool pumpRunning = false;
bool firstCycleDone = false;
bool tempRequestPending = false;
bool levelValid = false;
bool levelReady = false;
bool lowLevelActive = false;
bool highLevelActive = false;
bool warnTemp = false;
bool warnEC = false;
bool warnLevel = false;
bool resetButtonPressed = false;

float lastLevelCm = 0.0;
float lastTempC = 25.0;
float lastEC = 0.0;
float lastTDS = 0.0;

uint8_t levelFailCount = 0;

unsigned long pumpStart = 0;
unsigned long pumpStop = 0;
unsigned long bootTime = 0;
unsigned long lastLevelRead = 0;
unsigned long lastTempRequest = 0;
unsigned long lastECRead = 0;
unsigned long lastLCD = 0;
unsigned long resetPressStart = 0;
unsigned long lowLevelSince = 0;

bool lowLevelCandidate = false;

// ============================================================
//  ENTRADAS CON DEBOUNCE
// ============================================================
class DebouncedInput {
public:
  DebouncedInput(uint8_t p, bool activeHigh)
    : pin(p), activeHigh(activeHigh), lastRaw(false), stable(false), lastChange(0) {}

  void begin() {
    // Activo en bajo: pull-up interno (contacto a GND).
    // Activo en alto: requiere pull-down externo; no usar el pull-up.
    pinMode(pin, activeHigh ? INPUT : INPUT_PULLUP);
    lastRaw = digitalRead(pin);
    stable = lastRaw;
    lastChange = millis();
  }

  bool update() {
    bool raw = digitalRead(pin);
    if (raw != lastRaw) {
      lastRaw = raw;
      lastChange = millis();
    }
    if (millis() - lastChange >= FLOAT_DEBOUNCE_MS) {
      stable = raw;
    }
    return activeHigh ? (stable == HIGH) : (stable == LOW);
  }

private:
  uint8_t pin;
  bool activeHigh;
  bool lastRaw;
  bool stable;
  unsigned long lastChange;
};

DebouncedInput floatLow(PIN_FLOAT_LOW, FLOAT_FAULT_ACTIVE_HIGH);
DebouncedInput floatHigh(PIN_FLOAT_HIGH, FLOAT_FAULT_ACTIVE_HIGH);

// ============================================================
//  PROTOTIPOS
// ============================================================
void initPins();
void loadCalibration();
void saveCalibration();
void resetCalibrationDefaults();
bool calibrationPlausible();
uint16_t crc16(const uint8_t* data, size_t len);

float readDistanceCm();
float readLevelCm();
void updateTemperature();
float readECRawVoltage();
float convertEC(float voltage, float tempC);

void pumpOn();
void pumpOff();
void failSafe();
void latchFault(uint16_t code);
void clearFaults();
bool faultConditionStillPresent();
void updateResetButton();
void updateControl();

void updateLCD();
void lcdLine(uint8_t row, const char* text);
const char* faultText();
const char* faultTextFor(uint16_t flags);
const char* stateText();
void updateAlarm();
void updateSerial();
void printStatus();
void printHelp();

uint8_t relayOffLevel();
uint8_t relayOnLevel();

// ============================================================
//  SETUP
// ============================================================
void setup() {
  wdt_disable();

  uint8_t mcusr = MCUSR;
  MCUSR = 0;
  bool wdtResetFlag = (mcusr & _BV(WDRF)) != 0;

  // Protección durante el arranque: si el bus I2C o 1-Wire se cuelga,
  // el Mega se reinicia en lugar de quedar bloqueado. Al final de setup()
  // se pasa al watchdog de 2 s.
  wdt_enable(WDTO_8S);
  wdt_reset();

  Serial.begin(115200);
  Serial.setTimeout(1000);

  initPins();
  wdt_reset();

  lcd.init();
  lcd.backlight();
  wdt_reset();

  sensors.begin();
  sensors.setWaitForConversion(false);
  wdt_reset();

#if EC_USE_2V56
  analogReference(INTERNAL2V56);
#endif
  analogRead(PIN_EC);   // descartar primera lectura
  delay(10);
  wdt_reset();

  loadCalibration();
  wdt_reset();

  pumpOff();
  pumpStop = millis() - PUMP_MIN_OFF_MS;
  bootTime = millis();

  if (USE_FLOATS) {
    floatLow.begin();
    floatHigh.begin();
  }

  if (wdtResetFlag) {
    faultFlags |= FAULT_WATCHDOG;
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("Hidroponia MEGA"));
  lcd.setCursor(0, 1);
  lcd.print(F("Iniciando..."));

#if LCD_ROWS >= 4
  lcd.setCursor(0, 2);
  lcd.print(USE_FLOATS ? F("Flotadores OK") : F("SIN FLOTADORES"));
  lcd.setCursor(0, 3);
  lcd.print(F("PROTOTIPO"));
#endif

  delay(1500);
  wdt_reset();
  lcd.clear();

  wdt_enable(WDTO_2S);
  wdt_reset();
}

// ============================================================
//  LOOP
// ============================================================
void loop() {
  wdt_reset();

  // Flotadores
  if (USE_FLOATS) {
    lowLevelActive = floatLow.update();
    highLevelActive = floatHigh.update();
  }

  // Nivel ultrasónico
  if (millis() - lastLevelRead >= LEVEL_INTERVAL_MS) {
    lastLevelRead = millis();
    float lvl = readLevelCm();
    levelReady = true;
    if (lvl < 0) {
      if (levelFailCount < 255) levelFailCount++;
      if (levelFailCount >= 3) {
        levelValid = false;
        warnLevel = true;
      }
    } else {
      levelFailCount = 0;
      lastLevelCm = lvl;
      levelValid = true;
      warnLevel = false;
    }
  }

  // Temperatura no bloqueante
  if (!tempRequestPending && (millis() - lastTempRequest >= TEMP_INTERVAL_MS)) {
    sensors.requestTemperatures();
    tempRequestPending = true;
    lastTempRequest = millis();
  }
  updateTemperature();

  // EC/TDS
  if (millis() - lastECRead >= EC_INTERVAL_MS) {
    lastECRead = millis();
    float v = readECRawVoltage();
    float ec = convertEC(v, warnTemp ? 25.0 : lastTempC);
    lastEC = ec;
    lastTDS = ec * EC_TDS_FACTOR;
    warnEC = (lastEC < EC_MIN_PLAUSIBLE) || (lastEC > EC_MAX_PLAUSIBLE);
  }

  updateResetButton();
  updateControl();
  updateAlarm();

  if (millis() - lastLCD >= LCD_INTERVAL_MS) {
    lastLCD = millis();
    updateLCD();
  }

  updateSerial();
  wdt_reset();
}

// ============================================================
//  PINES
// ============================================================
void initPins() {
  // Relé OFF antes de configurar la salida
  digitalWrite(PIN_RELAY, relayOffLevel());
  pinMode(PIN_RELAY, OUTPUT);
  digitalWrite(PIN_RELAY, relayOffLevel());

  pinMode(PIN_TRIG, OUTPUT);
  digitalWrite(PIN_TRIG, LOW);
  pinMode(PIN_ECHO, INPUT);

  digitalWrite(PIN_EC_POWER, EC_POWER_ACTIVE_HIGH ? LOW : HIGH);
  pinMode(PIN_EC_POWER, OUTPUT);
  digitalWrite(PIN_EC_POWER, EC_POWER_ACTIVE_HIGH ? LOW : HIGH);

  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);

  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);

  pinMode(PIN_RESET, INPUT_PULLUP);

#if !USE_FLOATS
  // Entradas no usadas: evitar pines flotantes junto al relé.
  pinMode(PIN_FLOAT_LOW, INPUT_PULLUP);
  pinMode(PIN_FLOAT_HIGH, INPUT_PULLUP);
#endif

#if USE_FLOW_SWITCH
  pinMode(PIN_FLOW, FLOW_ACTIVE_LOW ? INPUT_PULLUP : INPUT);
#else
  // Entrada no usada: evitar pin flotante.
  pinMode(PIN_FLOW, INPUT_PULLUP);
#endif
}

uint8_t relayOffLevel() {
  return RELAY_ACTIVE_LOW ? HIGH : LOW;
}

uint8_t relayOnLevel() {
  return RELAY_ACTIVE_LOW ? LOW : HIGH;
}

// ============================================================
//  CALIBRACIÓN / EEPROM
// ============================================================
uint16_t crc16(const uint8_t* data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= (uint16_t)data[i] << 8;
    for (uint8_t j = 0; j < 8; j++) {
      if (crc & 0x8000) crc = (crc << 1) ^ 0x1021;
      else crc <<= 1;
    }
  }
  return crc;
}

void resetCalibrationDefaults() {
  // Poner a cero también el padding de la struct para que el CRC sea
  // determinista entre compilaciones y no se reescriba la EEPROM en cada boot.
  memset(&calib, 0, sizeof(calib));
  calib.magic = EEPROM_MAGIC;
  calib.version = EEPROM_VERSION;
  calib.ecSlope = EC_SLOPE_DEFAULT;
  calib.ecOffset = EC_OFFSET_DEFAULT;
  calib.sensorHeightCm = SENSOR_HEIGHT_CM;
  calib.levelLowCm = LEVEL_LOW_CM;
  calib.levelHighCm = LEVEL_HIGH_CM;
  calib.crc = crc16((const uint8_t*)&calib, offsetof(Calibration, crc));
  EEPROM.put(0, calib);
}

bool calibrationPlausible() {
  if (!isfinite(calib.ecSlope) || calib.ecSlope <= 0.0) return false;
  if (!isfinite(calib.ecOffset)) return false;
  if (!isfinite(calib.sensorHeightCm) || calib.sensorHeightCm <= 0.0) return false;
  if (!isfinite(calib.levelLowCm) || !isfinite(calib.levelHighCm)) return false;
  if (calib.levelLowCm < 0.0) return false;
  if (calib.levelHighCm <= calib.levelLowCm + LEVEL_HYST_CM) return false;
  if (calib.levelHighCm > calib.sensorHeightCm) return false;
  return true;
}

void loadCalibration() {
  EEPROM.get(0, calib);
  uint16_t crc = crc16((const uint8_t*)&calib, offsetof(Calibration, crc));
  bool invalid = (calib.magic != EEPROM_MAGIC) ||
                 (calib.version != EEPROM_VERSION) ||
                 (calib.crc != crc) ||
                 !calibrationPlausible();
  if (invalid) {
    resetCalibrationDefaults();
    Serial.println(F("Calibracion: valores por defecto"));
  }
}

void saveCalibration() {
  calib.crc = crc16((const uint8_t*)&calib, offsetof(Calibration, crc));
  EEPROM.put(0, calib);
}

// ============================================================
//  SENSORES
// ============================================================
float readDistanceCm() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);

  unsigned long duration = pulseIn(PIN_ECHO, HIGH, 30000UL);
  if (duration == 0) return -1.0;
  return duration / 58.0;
}

float readLevelCm() {
  const int N = 7;
  float samples[N];
  int valid = 0;

  for (int i = 0; i < N; i++) {
    float d = readDistanceCm();
    if (d >= LEVEL_MIN_VALID_CM && d <= LEVEL_MAX_VALID_CM) {
      samples[valid++] = d;
    }
    delay(20);
  }

  if (valid < 3) return -1.0;

  // Ordenar para mediana
  for (int i = 1; i < valid; i++) {
    float key = samples[i];
    int j = i - 1;
    while (j >= 0 && samples[j] > key) {
      samples[j + 1] = samples[j];
      j--;
    }
    samples[j + 1] = key;
  }

  float median = samples[valid / 2];
  float level = calib.sensorHeightCm - median;

  if (level < 0.0) level = 0.0;
  if (level > calib.sensorHeightCm) level = calib.sensorHeightCm;
  return level;
}

void updateTemperature() {
  if (!tempRequestPending) return;
  if (millis() - lastTempRequest < 800UL) return;

  tempRequestPending = false;
  float t = sensors.getTempCByIndex(0);

  if (t == DEVICE_DISCONNECTED_C || t == 85.0 || t < -5.0 || t > 60.0) {
    warnTemp = true;
  } else {
    lastTempC = t;
    warnTemp = false;
  }
}

float readECRawVoltage() {
#if USE_EC_POWER
  wdt_reset();
  digitalWrite(PIN_EC_POWER, EC_POWER_ACTIVE_HIGH ? HIGH : LOW);
  delay(EC_WARMUP_MS);
#endif

  // Descartar las primeras lecturas
  for (uint8_t i = 0; i < 8; i++) analogRead(PIN_EC);

  uint32_t sum = 0;
  for (uint8_t i = 0; i < 32; i++) {
    sum += analogRead(PIN_EC);
    delayMicroseconds(200);
  }

#if USE_EC_POWER
  digitalWrite(PIN_EC_POWER, EC_POWER_ACTIVE_HIGH ? LOW : HIGH);
#endif

  float vref = EC_USE_2V56 ? 2.56 : 5.0;
  wdt_reset();
  return (sum / 32.0) * vref / 1023.0;
}

float convertEC(float voltage, float tempC) {
  float ec = calib.ecSlope * voltage + calib.ecOffset;
  if (ec < 0.0) ec = 0.0;

#if !EC_MODULE_COMPENSATED
  ec = ec / (1.0 + 0.02 * (tempC - 25.0));
#endif

  return ec;
}

// ============================================================
//  CONTROL DE BOMBA
// ============================================================
void pumpOn() {
  if (pumpRunning) return;
  if (faultFlags) return;
  if (millis() - pumpStop < PUMP_MIN_OFF_MS) return;
  digitalWrite(PIN_RELAY, relayOnLevel());
  pumpRunning = true;
  pumpStart = millis();
  lowLevelCandidate = false;
}

void pumpOff() {
  bool wasRunning = pumpRunning;
  digitalWrite(PIN_RELAY, relayOffLevel());
  pumpRunning = false;
  if (wasRunning) pumpStop = millis();
}

void failSafe() {
  pumpOff();
}

void latchFault(uint16_t code) {
  if (!(faultFlags & code)) {
    faultFlags |= code;
    Serial.print(F("FALLA: "));
    Serial.println(faultTextFor(code));
  }
  state = ST_FAULT;
  pumpOff();
}

void clearFaults() {
  pumpOff();
  faultFlags = 0;
  state = ST_IDLE;
  lowLevelCandidate = false;
  // Tras borrar una falla no se salta la secuencia: se exige la espera
  // mínima de bomba apagada y la ventana de arranque.
  firstCycleDone = false;
  bootTime = millis();
  pumpStop = millis();
  Serial.println(F("Fallas borradas"));
}

bool faultConditionStillPresent() {
  if (USE_FLOATS) {
    if (lowLevelActive || highLevelActive) return true;
  } else {
    if (!levelReady) return false;
    if (!levelValid) return true;
    // Histéresis: no permitir borrar la falla justo en el umbral.
    if (lastLevelCm < calib.levelLowCm + LEVEL_HYST_CM) return true;
    if (lastLevelCm > calib.levelHighCm - LEVEL_HYST_CM) return true;
  }
  return false;
}

void updateResetButton() {
  bool pressed = (digitalRead(PIN_RESET) == LOW);

  if (pressed && !resetButtonPressed) {
    resetButtonPressed = true;
    resetPressStart = millis();
  }

  if (!pressed) {
    resetButtonPressed = false;
    return;
  }

  if (resetButtonPressed && (millis() - resetPressStart >= RESET_HOLD_MS)) {
    if (faultFlags && !faultConditionStillPresent()) {
      clearFaults();
    } else if (faultFlags) {
      Serial.println(F("Falla presente: no se borra"));
    }
    resetButtonPressed = false;
  }
}

void updateControl() {
  if (faultFlags) {
    failSafe();
    state = ST_FAULT;
    return;
  }

  if (USE_FLOATS) {
    if (lowLevelActive) {
      latchFault(FAULT_LOW_LEVEL);
      return;
    }
    if (highLevelActive) {
      latchFault(FAULT_HIGH_LEVEL);
      return;
    }
  } else {
    if (!levelReady) {
      state = ST_IDLE;
      return;
    }

    if (!levelValid) {
      // No evaluar umbrales con una lectura vieja. Solo fallar tras
      // 3 lecturas inválidas consecutivas; mientras tanto, bomba OFF.
      if (levelFailCount >= 3) {
        latchFault(FAULT_LEVEL_SENSOR);
      } else {
        pumpOff();
        state = ST_IDLE;
      }
      return;
    }

    if (lastLevelCm < calib.levelLowCm) {
      if (!lowLevelCandidate) {
        lowLevelCandidate = true;
        lowLevelSince = millis();
      } else if (millis() - lowLevelSince >= LOW_LEVEL_CONFIRM_MS) {
        latchFault(FAULT_LOW_LEVEL);
        return;
      }
    } else {
      lowLevelCandidate = false;
    }

    if (lastLevelCm > calib.levelHighCm) {
      latchFault(FAULT_HIGH_LEVEL);
      return;
    }
  }

#if USE_FLOW_SWITCH
  if (pumpRunning && (millis() - pumpStart > FLOW_GRACE_MS)) {
    bool flowOk = FLOW_ACTIVE_LOW ? (digitalRead(PIN_FLOW) == LOW)
                                  : (digitalRead(PIN_FLOW) == HIGH);
    if (!flowOk) {
      latchFault(FAULT_NO_FLOW);
      return;
    }
  }
#endif

  if (pumpRunning && (millis() - pumpStart >= PUMP_MAX_ON_MS)) {
    latchFault(FAULT_MAX_RUN);
    return;
  }

  if (pumpRunning) {
    if (millis() - pumpStart >= IRRIGATION_ON_MS) {
      pumpOff();
      state = ST_IDLE;
    } else {
      state = ST_IRRIGATING;
    }
  } else {
    if (!firstCycleDone && (millis() - bootTime < STARTUP_GRACE_MS)) {
      state = ST_IDLE;
      return;
    }

    bool offOk = firstCycleDone ? (millis() - pumpStop >= IRRIGATION_OFF_MS) : true;

    if (offOk && (millis() - pumpStop >= PUMP_MIN_OFF_MS)) {
      pumpOn();
      firstCycleDone = true;
      state = ST_IRRIGATING;
    } else {
      state = ST_IDLE;
    }
  }
}

// ============================================================
//  LCD
// ============================================================
const char* faultTextFor(uint16_t flags) {
  if (flags & FAULT_LOW_LEVEL)    return "NIVEL BAJO";
  if (flags & FAULT_HIGH_LEVEL)   return "NIVEL ALTO";
  if (flags & FAULT_LEVEL_SENSOR) return "SENSOR NIVEL";
  if (flags & FAULT_MAX_RUN)      return "BOMBA BLOQ.";
  if (flags & FAULT_NO_FLOW)      return "SIN CAUDAL";
  if (flags & FAULT_WATCHDOG)     return "WDT RESET";
  return "OK";
}

const char* faultText() {
  return faultTextFor(faultFlags);
}

const char* stateText() {
  if (faultFlags) return "FALLA";
  if (pumpRunning) return "RIEGO";
  return "IDLE";
}

void lcdLine(uint8_t row, const char* text) {
  char buf[LCD_COLS + 1];
  snprintf(buf, sizeof(buf), "%-*s", LCD_COLS, text);
  lcd.setCursor(0, row);
  lcd.print(buf);
}

void updateLCD() {
  char line[LCD_COLS + 1];
  char levelStr[8], tempStr[8], ecStr[8], tdsStr[8];

  dtostrf(lastLevelCm, 5, 1, levelStr);
  dtostrf(lastTempC, 4, 1, tempStr);
  dtostrf(lastEC, 5, 0, ecStr);
  dtostrf(lastTDS, 5, 0, tdsStr);

  if (faultFlags) {
    snprintf(line, sizeof(line), "FALLA: %s", faultText());
    lcdLine(0, line);

    snprintf(line, sizeof(line), "NIV %s T %s",
             levelValid ? levelStr : "---", tempStr);
    lcdLine(1, line);

#if LCD_ROWS >= 4
    snprintf(line, sizeof(line), "EC %s TDS %s", ecStr, tdsStr);
    lcdLine(2, line);
    lcdLine(3, "RESET 2s");
#endif

  } else {
    snprintf(line, sizeof(line), "NIV %scm %s",
             levelValid ? levelStr : "---", levelValid ? "" : "?");
    lcdLine(0, line);

    snprintf(line, sizeof(line), "T %sC EC %s%s", tempStr, ecStr,
             (warnTemp || warnEC) ? "!" : "");
    lcdLine(1, line);

#if LCD_ROWS >= 4
    snprintf(line, sizeof(line), "TDS %sppm%s", tdsStr, warnEC ? "?" : "");
    lcdLine(2, line);

    snprintf(line, sizeof(line), "%s%s%s", stateText(),
             (warnTemp || warnEC || warnLevel) ? " !" : "",
             USE_FLOATS ? "" : " SIN FLOT.");
    lcdLine(3, line);
#endif
  }
}

void updateAlarm() {
  bool alarm = (faultFlags != 0);
  bool warning = (warnTemp || warnEC || warnLevel);

  if (!alarm && !warning) {
    digitalWrite(PIN_LED, LOW);
#if BUZZER_ACTIVE
    digitalWrite(PIN_BUZZER, LOW);
#else
    noTone(PIN_BUZZER);
#endif
    return;
  }

  unsigned long period = alarm ? 250UL : 1000UL;
  bool on = ((millis() / period) % 2) == 0;

  digitalWrite(PIN_LED, on ? HIGH : LOW);

#if BUZZER_ACTIVE
  digitalWrite(PIN_BUZZER, (alarm && on) ? HIGH : LOW);
#else
  if (alarm && on) tone(PIN_BUZZER, 2000);
  else noTone(PIN_BUZZER);
#endif
}

// ============================================================
//  SERIAL / CALIBRACIÓN
// ============================================================
void printStatus() {
  Serial.print(F("Estado: "));
  Serial.println(stateText());

  Serial.print(F("Nivel: "));
  Serial.print(lastLevelCm, 1);
  Serial.print(F(" cm "));
  Serial.println(levelValid ? F("OK") : F("?"));

  Serial.print(F("Temp: "));
  Serial.print(lastTempC, 1);
  Serial.print(F(" C "));
  Serial.println(warnTemp ? F("?") : F("OK"));

  Serial.print(F("EC: "));
  Serial.print(lastEC, 0);
  Serial.print(F(" uS/cm  TDS: "));
  Serial.print(lastTDS, 0);
  Serial.println(F(" ppm"));

  Serial.print(F("Fallas: 0x"));
  Serial.println(faultFlags, HEX);

  Serial.print(F("Flotadores: "));
  Serial.println(USE_FLOATS ? F("SI") : F("NO"));

  Serial.print(F("Bomba: "));
  Serial.println(pumpRunning ? F("ON") : F("OFF"));

  Serial.print(F("Nivel bajo: "));
  Serial.print(calib.levelLowCm, 1);
  Serial.print(F(" cm  alto: "));
  Serial.println(calib.levelHighCm, 1);

  Serial.print(F("EC slope: "));
  Serial.print(calib.ecSlope, 2);
  Serial.print(F(" offset: "));
  Serial.println(calib.ecOffset, 2);
}

void printHelp() {
  Serial.println(F("Comandos:"));
  Serial.println(F("  s              estado"));
  Serial.println(F("  d              valores crudos"));
  Serial.println(F("  l <cm>         altura sensor-fondo"));
  Serial.println(F("  a <cm>         nivel bajo"));
  Serial.println(F("  b <cm>         nivel alto"));
  Serial.println(F("  e <slope> <off> calibracion EC"));
  Serial.println(F("  x              reset calibracion"));
  Serial.println(F("  r              borrar fallas"));
  Serial.println(F("  h              ayuda"));
}

void updateSerial() {
  wdt_reset();

  if (!Serial.available()) return;

  char buf[64];
  size_t len = Serial.readBytesUntil('\n', buf, sizeof(buf) - 1);
  wdt_reset();
  if (len == 0) return;
  buf[len] = '\0';

  while (len > 0 && (buf[len - 1] == '\r' || buf[len - 1] == '\n' || buf[len - 1] == ' ')) {
    buf[--len] = '\0';
  }
  if (len == 0) return;

  char cmd = (char)toupper(buf[0]);
  char* p = buf + 1;
  char* end = nullptr;

  switch (cmd) {
    case 'S':
      printStatus();
      break;

    case 'D': {
      // Alimentar el módulo EC durante la lectura; analogRead directo
      // devolvería 0 si el módulo está apagado.
      float v = readECRawVoltage();
      float ec = convertEC(v, warnTemp ? 25.0 : lastTempC);
      Serial.print(F("V_EC="));
      Serial.print(v, 3);
      Serial.print(F(" dist="));
      Serial.print(readDistanceCm(), 1);
      Serial.print(F(" nivel="));
      Serial.print(lastLevelCm, 1);
      Serial.print(F(" temp="));
      Serial.print(lastTempC, 1);
      Serial.print(F(" EC="));
      Serial.print(ec, 0);
      Serial.print(F(" TDS="));
      Serial.println(ec * EC_TDS_FACTOR, 0);
      break;
    }

    case 'L': {
      float v = strtod(p, &end);
      if (end == p || !isfinite(v) || v < 10.0 || v > 500.0 || v <= calib.levelHighCm) {
        Serial.println(F("Uso: l <altura_cm>, mayor que nivel alto"));
      } else {
        calib.sensorHeightCm = v;
        saveCalibration();
        Serial.print(F("Altura sensor: "));
        Serial.println(v, 1);
      }
      break;
    }

    case 'A': {
      float v = strtod(p, &end);
      if (end == p || !isfinite(v) || v < 0.0 || v > 200.0 ||
          v + LEVEL_HYST_CM >= calib.levelHighCm) {
        Serial.println(F("Uso: a <nivel_bajo_cm>, menor que alto - histeresis"));
      } else {
        calib.levelLowCm = v;
        saveCalibration();
        Serial.print(F("Nivel bajo: "));
        Serial.println(v, 1);
      }
      break;
    }

    case 'B': {
      float v = strtod(p, &end);
      if (end == p || !isfinite(v) || v < 0.0 || v > 200.0 ||
          v <= calib.levelLowCm + LEVEL_HYST_CM) {
        Serial.println(F("Uso: b <nivel_alto_cm>, mayor que bajo + histeresis"));
      } else {
        calib.levelHighCm = v;
        saveCalibration();
        Serial.print(F("Nivel alto: "));
        Serial.println(v, 1);
      }
      break;
    }

    case 'E': {
      float slope = strtod(p, &end);
      if (end == p || !isfinite(slope) || slope <= 0.0) {
        Serial.println(F("Uso: e <slope> <offset>"));
        break;
      }
      p = end;
      float offset = strtod(p, &end);
      if (end == p || !isfinite(offset)) {
        Serial.println(F("Uso: e <slope> <offset>"));
        break;
      }
      calib.ecSlope = slope;
      calib.ecOffset = offset;
      saveCalibration();
      Serial.print(F("EC slope: "));
      Serial.print(slope, 2);
      Serial.print(F(" offset: "));
      Serial.println(offset, 2);
      break;
    }

    case 'X':
      resetCalibrationDefaults();
      Serial.println(F("Calibracion reseteada"));
      break;

    case 'R':
      if (faultFlags && !faultConditionStillPresent()) {
        clearFaults();
      } else if (faultFlags) {
        Serial.println(F("Falla presente: no se borra"));
      } else {
        Serial.println(F("Sin fallas"));
      }
      break;

    case 'H':
      printHelp();
      break;

    default:
      printHelp();
      break;
  }

  wdt_reset();
}
