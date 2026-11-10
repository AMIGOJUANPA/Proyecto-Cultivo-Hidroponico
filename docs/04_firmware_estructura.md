# 04 — Estructura del firmware

## 1. Máquina de estados

```text
INIT
  ├─ apagar relé, inicializar pines, LCD, sensores
  ├─ cargar calibración de EEPROM
  └─ si todo OK → IDLE

IDLE
  ├─ bomba OFF
  ├─ esperar intervalo de riego
  └─ si no hay falla y toca regar → IRRIGATING

IRRIGATING
  ├─ bomba ON
  ├─ si pasa el tiempo de riego → IDLE
  ├─ si nivel bajo / flotador bajo → FAULT_LOW_LEVEL
  ├─ si flotador alto → FAULT_HIGH_LEVEL
  ├─ si tiempo máximo de bomba → FAULT_MAX_RUN
  └─ si flow switch sin caudal → FAULT_NO_FLOW

FAULT_*
  ├─ bomba OFF
  ├─ buzzer + LED
  ├─ LCD con código de falla
  ├─ latch: no se borra solo
  └─ botón reset 2 s + condición desaparecida → IDLE
```

> **Configuración actual:** `USE_FLOATS 0`. El usuario no instalará flotadores, así que las transiciones por flotador no se evalúan. La única protección de nivel es el HC-SR04 y el tiempo máximo de bomba; por eso el sistema queda como prototipo y no debe operar sin supervisión.

## 2. Pseudocódigo principal

```text
setup():
  wdt_disable()
  wdt_enable(8 s)          // protección durante el arranque
  configurar pines
  relé OFF desde el primer instante
  inicializar LCD, DS18B20, EEPROM
  cargar calibración
  wdt_enable(2 s)

loop():
  wdt_reset()

  leer flotadores con debounce
  si flotador bajo abierto:
      latch(FAULT_LOW_LEVEL)
  si flotador alto abierto:
      latch(FAULT_HIGH_LEVEL)

  si hay falla crítica:
      fail_safe()
      actualizar LCD y alarma
      return

  leer nivel ultrasónico (mediana de 7)
  si timeout o fuera de rango:
      3 fallos consecutivos → latch(FAULT_LEVEL_SENSOR), bomba OFF
      mientras tanto: no evaluar umbrales con una lectura vieja

  leer temperatura DS18B20 (no bloqueante)
  si temperatura inválida:
      warning(TEMP_SENSOR)   // no detiene la bomba

  cada 10 s:
      leer EC/TDS con compensación de temperatura
      si fuera de rango plausible:
          warning(EC_SENSOR) // no detiene la bomba

  control de riego:
      si no hay falla y nivel OK:
          si toca regar y ha pasado el tiempo mínimo OFF:
              bomba ON
          si terminó el tiempo ON:
              bomba OFF
      si bomba ON supera el tiempo máximo:
          latch(FAULT_MAX_RUN)

  actualizar LCD cada 500 ms
  actualizar alarma
```

## 3. Histéresis y temporización

Para el nivel del reservorio:

```text
nivel_ok = false si altura < NIVEL_BAJO_CM
nivel_ok = true  si altura > NIVEL_BAJO_CM + HISTERESIS_CM
```

Para el riego:

```text
tiempo ON configurable   (ej. 5 min)
tiempo OFF configurable  (ej. 30 min)
tiempo mínimo OFF        (ej. 60 s)
tiempo máximo ON         (ej. 30 min, falla si se supera)
gracia de arranque       (ej. 5 s para que el nivel se estabilice)
```

La histéresis se usa al validar y borrar fallas de nivel; el riego se temporiza por tiempo ON/OFF. El tiempo máximo ON detecta una bomba que no se apaga o un relé pegado, pero **no sustituye** el lazo cableado de seguridad.

## 4. Fallas y comportamiento

| Falla | ¿Detiene bomba? | ¿Latch? | Comportamiento |
|-------|-----------------|---------|----------------|
| Flotador bajo abierto | Sí | Sí | Alarma “NIVEL BAJO”. Reset manual tras rellenar. |
| Flotador alto abierto | Sí | Sí | Alarma “NIVEL ALTO / DRENAJE”. Reset manual. |
| HC-SR04 timeout | Sí | Sí | Alarma “SENSOR NIVEL”. Sin nivel no hay control seguro. |
| Nivel bajo por ultrasónico | Sí | Sí | Solo si no hay flotador bajo. Alarma “MARCHA EN SECO”. |
| Nivel alto por ultrasónico | Sí | Sí | Solo aplica al reservorio, no a la mesa. |
| DS18B20 inválido | No | Warning | Usa 25 °C para compensar EC; muestra “TEMP?”. |
| EC fuera de rango | No | Warning | Muestra “EC?”; no es crítico para la bomba. |
| Tiempo máximo ON | Sí | Sí | Alarma “BOMBA BLOQUEADA”. |
| Flow switch sin caudal | Sí | Sí | Alarma “SIN CAUDAL”. |
| Watchdog reset | Sí | Sí | Al arrancar, relé OFF; muestra “WDT RESET”. |

## 5. Funciones del sketch

```text
void setup();
void loop();

void initPins();
void loadCalibration();
void saveCalibration();
void resetCalibrationDefaults();
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
```

## 6. Estructura de configuración

```cpp
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

#define RELAY_ACTIVE_LOW      1
#define USE_FLOATS            0   // cambiar a 1 al instalar flotadores
#define USE_EC_POWER          1   // cambiar a 0 si el módulo EC queda siempre alimentado
#define EC_USE_2V56           1   // 1: referencia interna 2.56 V; 0: 5 V
#define EC_MODULE_COMPENSATED 0   // 1 si el módulo TDS ya compensa temperatura
```

## 7. Watchdog

- `wdt_enable(WDTO_8S)` durante `setup()` para proteger la inicialización de I2C/1-Wire; al final se cambia a `WDTO_2S`.
- `wdt_reset()` entre las etapas lentas del arranque.
- `wdt_enable(WDTO_2S)` al final de `setup()`.
- `wdt_reset()` al final de cada `loop()`.
- `wdt_disable()` al inicio de `setup()` para evitar bootloop si el WDT quedó activo.
- Al reiniciar por watchdog, el pin del relé vuelve a entrada; el pull-up externo de 10 kΩ mantiene el relé apagado.
- No usar `delay()` largos. El DS18B20 se lee de forma no bloqueante. La única espera larga es `EC_WARMUP_MS` (1 s), con `wdt_reset()` antes y después.

## 8. Modo calibración por serial

Comandos sugeridos a 115200 baudios:

| Comando | Acción |
|---------|--------|
| `s` | Mostrar estado, nivel, temperatura, EC, TDS y fallas. |
| `d` | Mostrar tensión de EC, distancia, nivel y temperatura actuales (alimenta el módulo EC durante la lectura). |
| `l <cm>` | Altura desde la cara del sensor al fondo del reservorio. |
| `a <cm>` | Umbral de nivel bajo. |
| `b <cm>` | Umbral de nivel alto. |
| `e <slope> <offset>` | Calibración EC: pendiente en µS/cm por voltio y offset en µS/cm. |
| `x` | Restaurar calibración por defecto. |
| `r` | Borrar fallas si la condición ya desapareció. |
| `h` | Ayuda. |

La calibración se guarda en EEPROM con un número mágico y CRC. Si el CRC no coincide o los valores no son plausibles, se usan valores por defecto y se avisa por serial (`Calibracion: valores por defecto`).
