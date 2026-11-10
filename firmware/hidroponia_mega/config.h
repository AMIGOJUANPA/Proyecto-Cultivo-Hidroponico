#pragma once
#include <Arduino.h>

// ============================================================
//  PINES
// ============================================================
#define PIN_TRIG        2
#define PIN_ECHO        3
#define PIN_DS18B20     4
#define PIN_RELAY       5
#define PIN_EC_POWER    6
#define PIN_BUZZER      7
#define PIN_LED         8
#define PIN_FLOAT_LOW   9
#define PIN_FLOAT_HIGH  10
#define PIN_EC          A0
#define PIN_FLOW        23
#define PIN_RESET       22

// ============================================================
//  OPCIONES DE HARDWARE
// ============================================================
#define RELAY_ACTIVE_LOW       1   // JQC-3FF-S-Z: probablemente 1. Con 1, poner
                                   // pull-up externo de 10 kΩ en D5 (OFF en reset).
#define USE_FLOATS             0   // cambiar a 1 al instalar los 2 flotadores
#define USE_EC_POWER           1   // 1 si D6 controla la alimentación del módulo EC
#define EC_POWER_ACTIVE_HIGH   1   // 1 si el high-side switch se activa con HIGH.
                                   // D6 necesita pull-down externo de 10 kΩ para
                                   // que el módulo EC quede OFF durante el reset.
#define EC_USE_2V56            1   // 1: referencia interna 2,56 V; 0: 5 V
#define EC_MODULE_COMPENSATED  0   // 1 si el módulo TDS ya compensa temperatura
#define USE_FLOW_SWITCH        0   // 1 si se instala flow switch
#define FLOW_ACTIVE_LOW        1   // 1 si el flow switch cierra a GND con caudal
#define FLOAT_FAULT_ACTIVE_HIGH 0  // 0: NO a GND, pull-up interno (activo bajo).
                                   // 1: NC a GND, pull-up externo (activo alto;
                                   //    rotura de cable = falla).
                                   // El NC del lazo de seguridad es independiente.
#define BUZZER_ACTIVE          1   // 1: buzzer activo; 0: pasivo con tone()

// ============================================================
//  LCD
// ============================================================
#define LCD_ADDR   0x27
#define LCD_COLS   20
#define LCD_ROWS   4

// ============================================================
//  TIEMPOS
// ============================================================
const unsigned long LEVEL_INTERVAL_MS      = 500;
const unsigned long TEMP_INTERVAL_MS       = 2000;
const unsigned long EC_INTERVAL_MS         = 10000;
const unsigned long LCD_INTERVAL_MS        = 500;
const unsigned long FLOAT_DEBOUNCE_MS      = 100;
const unsigned long RESET_HOLD_MS          = 2000;
const unsigned long PUMP_MIN_OFF_MS        = 60000;
const unsigned long PUMP_MAX_ON_MS         = 30UL * 60UL * 1000UL;
const unsigned long IRRIGATION_ON_MS       = 5UL * 60UL * 1000UL;
const unsigned long IRRIGATION_OFF_MS      = 30UL * 60UL * 1000UL;
const unsigned long STARTUP_GRACE_MS       = 5000;
const unsigned long LOW_LEVEL_CONFIRM_MS   = 2000;
const unsigned long FLOW_GRACE_MS          = 10000;
const unsigned long EC_WARMUP_MS           = 1000;

// ============================================================
//  NIVEL  (calibrar con regla)
// ============================================================
const float SENSOR_HEIGHT_CM    = 50.0;  // cara del sensor al fondo
const float LEVEL_LOW_CM        = 8.0;   // mínimo seguro
const float LEVEL_HIGH_CM       = 40.0;  // máximo del reservorio
const float LEVEL_HYST_CM       = 2.0;
const float LEVEL_MIN_VALID_CM  = 2.0;
const float LEVEL_MAX_VALID_CM  = 200.0;

// ============================================================
//  EC/TDS  (calibrar con solución patrón)
// ============================================================
const float EC_SLOPE_DEFAULT    = 1000.0; // µS/cm por voltio
const float EC_OFFSET_DEFAULT   = 0.0;
const float EC_TDS_FACTOR       = 0.5;
const float EC_MIN_PLAUSIBLE    = 10.0;  // por debajo: sonda desconectada o seca
const float EC_MAX_PLAUSIBLE    = 5000.0;

// ============================================================
//  EEPROM
// ============================================================
#define EEPROM_MAGIC    0x48594452UL
#define EEPROM_VERSION  1
