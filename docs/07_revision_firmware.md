# 07 — Revisión del firmware y compilación

## 1. Entorno de compilación

| Elemento | Versión / valor |
|----------|-----------------|
| Arduino CLI | 1.5.1 (incluido en Arduino IDE 2.x) |
| Core | `arduino:avr` 1.8.8 |
| Placa | Arduino Mega 2560 (`arduino:avr:mega`) |
| LCD | LiquidCrystal I2C 1.1.2 |
| 1-Wire | OneWire 2.3.8 |
| DS18B20 | DallasTemperature 4.0.6 |

Comando usado:

```powershell
& "C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe" `
  compile --fqbn arduino:avr:mega --warnings all `
  "C:\Users\PC\Documents\Hydroponics STEAM\firmware\hidroponia_mega"
```

Resultado: **compila sin errores**. El sketch no emite advertencias con `--warnings all`; las advertencias que aparecen provienen de la librería LiquidCrystal I2C (parámetros no usados en varios métodos) y del core AVR (`new.cpp`), no del código del proyecto.

## 2. Uso de memoria

| Variante de `config.h` | Flash | RAM global |
|-----------------------|------:|-----------:|
| Por defecto (sin flotadores, EC 2,56 V, LCD 20x4) | 21 242 B (8 %) | 788 B (9 %) |
| `USE_FLOATS 1` + `USE_FLOW_SWITCH 1` | 21 200 B (8 %) | 790 B (9 %) |
| `BUZZER_ACTIVE 0` (buzzer pasivo) | 21 242 B (8 %) | 788 B (9 %) |
| `EC_USE_2V56 0` + `EC_MODULE_COMPENSATED 1` + `USE_EC_POWER 0` | 20 968 B (8 %) | 788 B (9 %) |
| `RELAY_ACTIVE_LOW 0` + `FLOAT_FAULT_ACTIVE_HIGH 1` | 21 242 B (8 %) | 788 B (9 %) |
| LCD 16x2 (`LCD_COLS 16`, `LCD_ROWS 2`) | 20 848 B (8 %) | 736 B (8 %) |

Todas las variantes se compilaron con `exit=0`. El margen de RAM es amplio; el límite real no es la memoria, sino la seguridad del hardware.

## 3. Correcciones aplicadas en esta revisión

| # | Problema detectado | Corrección | Motivo |
|---|--------------------|------------|--------|
| 1 | Al arrancar, la primera lectura inválida del HC-SR04 podía declarar `FAULT_LEVEL_SENSOR` de inmediato. | Se exigen 3 fallos consecutivos; mientras tanto la bomba queda OFF y no se evalúan umbrales con una lectura vieja. | Evitar fallos falsos por eco perdido o espuma. |
| 2 | `clearFaults()` ponía `firstCycleDone = true`, lo que obligaba a esperar `IRRIGATION_OFF_MS` (30 min) antes de reanudar. | Se reinicia la secuencia (`firstCycleDone = false`, `bootTime = millis()`, `pumpStop = millis()`). | Tras un reset manual se exige la espera mínima de 60 s, no 30 min. |
| 3 | `clearFaults()` no forzaba el relé a OFF. | Se llama a `pumpOff()` al inicio. | Defensa en profundidad: borrar fallas nunca debe dejar la bomba activa. |
| 4 | `pumpOn()` no verificaba fallas ni el tiempo mínimo apagado. | Guardas internas de `faultFlags` y `PUMP_MIN_OFF_MS`. | Evitar arranques accidentales si se llama desde otro punto. |
| 5 | El CRC de la EEPROM incluía padding no inicializado de la `struct`. | `memset(&calib, 0, sizeof(calib))` antes de cargar los valores por defecto. | CRC determinista y menor desgaste de EEPROM. |
| 6 | `DebouncedInput` usaba siempre `INPUT_PULLUP`, incluso con flotadores activos en alto. | `pinMode(pin, activeHigh ? INPUT : INPUT_PULLUP)`. | Con activo en alto hace falta pull-down externo, no pull-up. |
| 7 | El flow switch aparecía en A1 y en D23 según el documento. | Se unificó en **D23** (digital). A1 queda como reserva analógica. | Evitar un mapa de pines ambiguo. |
| 8 | El comando serial `d` leía A0 con el módulo EC posiblemente apagado. | `d` llama a `readECRawVoltage()`, que alimenta el módulo y espera el warm-up. | Evitar lecturas falsas de 0. |
| 9 | `readBytesUntil()` podía bloquear hasta 1 s sin resetear el watchdog. | `wdt_reset()` inmediatamente después de la lectura serial. | Margen adicional frente al WDT de 2 s. |
| 10 | El LCD mostraba el último nivel válido aunque el sensor estuviera en falla. | Muestra `NIV ---cm ?` cuando `levelValid == false`. | No confundir una lectura vieja con el nivel actual. |
| 11 | `latchFault()` imprimía el texto de la falla de mayor prioridad, no necesariamente la recién activada. | `faultTextFor(code)` imprime el código que se acaba de latchar. | Diagnóstico correcto por serial. |
| 12 | El borrado de fallas no tenía histéresis. | `faultConditionStillPresent()` usa `LEVEL_HYST_CM` en los umbrales. | Evitar borrar y volver a fallar por oscilación en el umbral. |
| 13 | El `delay(1000)` del warm-up EC ocurría sin reset previo del WDT. | `wdt_reset()` antes del warm-up. | Mantener el WDT de 2 s con margen. |
| 14 | Pines de flotadores/flow switch no usados podían quedar flotantes. | Pull-up interno cuando la entrada no se usa. | Evitar ruido y consumo innecesario. |
| 15 | Los comandos seriales aceptaban `nan`/`inf` y umbrales incoherentes (bajo ≥ alto, alto > altura del sensor). | Se valida `isfinite()` y la relación entre umbrales antes de guardar. | Evitar una calibración que inutilice la protección de nivel. |
| 16 | `loadCalibration()` aceptaba datos con CRC válido pero valores imposibles. | `calibrationPlausible()` rechaza pendiente ≤ 0, valores no finitos y umbrales incoherentes. | Recuperar valores por defecto ante EEPROM corrupta. |
| 17 | Una sonda EC desconectada podía leerse como 0 µS/cm sin aviso. | `EC_MIN_PLAUSIBLE` pasa de 0 a 10 µS/cm. | Distinguir sonda seca/desconectada de una lectura válida. |
| 18 | Un cuelgue del bus I2C o 1-Wire durante `setup()` ocurría con el WDT deshabilitado. | WDT de 8 s durante `setup()`, con `wdt_reset()` entre etapas; al final se pasa a 2 s. | El arranque también queda protegido y el relé permanece OFF. |

## 4. Verificaciones estáticas realizadas

- El relé se escribe a OFF **antes** de `pinMode(OUTPUT)` y se vuelve a escribir después.
- `wdt_disable()` al inicio de `setup()` y `wdt_enable(WDTO_2S)` al final.
- `MCUSR` se lee y se limpia; `WDRF` activa `FAULT_WATCHDOG`.
- El DS18B20 se lee sin bloquear (`setWaitForConversion(false)` + espera de 800 ms).
- El ADC de EC usa 8 lecturas de descarte + 32 muestras promediadas.
- La compensación de temperatura usa 25 °C si el DS18B20 es inválido.
- La calibración se guarda en EEPROM con magic, versión y CRC.
- Los comandos seriales usan `strtod`/`snprintf`/`dtostrf`; no se usa `%f` en `snprintf`.
- `F()` se usa para las cadenas constantes y ahorrar RAM.
- La rama de flotadores, la de flow switch, el buzzer pasivo y la referencia de 5 V compilan correctamente.

## 5. Limitaciones que el firmware **no** puede resolver

1. **Relé pegado:** si el contacto del relé principal se suelda, el firmware no lo detecta. Solo el relé de seguridad cableado con los flotadores corta la bomba.
2. **Desbordamiento de la mesa:** el HC-SR04 mide el reservorio, no la mesa. Sin flotador alto en la mesa/canal no hay protección.
3. **Rotura de cable del flotador:** el firmware no puede verla si el contacto se cablea como NO. La seguridad depende de usar NC en el lazo cableado.
4. **Sensor de nivel falso:** espuma, condensación, oleaje o un eco de una pared pueden dar una lectura plausible pero incorrecta. El pozo de calma y el montaje son parte de la seguridad.
5. **Marcha en seco por bomba obstruida:** el nivel del reservorio puede ser correcto y la bomba no mover agua. Se necesita flow switch o sensor de corriente.
6. **TDS Meter V1.0:** la excitación de la sonda no está confirmada (“corriente del toma” es la alimentación). Si la sonda es DC, se polariza y se corroe; el pulso de D6 reduce el daño, pero no lo elimina. Hay que medir la sonda y confirmar el modelo.
7. **Calibración EC:** los valores por defecto (`EC_SLOPE_DEFAULT = 1000`, `EC_OFFSET_DEFAULT = 0`) son marcadores de posición. No son una calibración real.

## 6. Respuestas del usuario incorporadas

| Respuesta del usuario | Interpretación | Acción / consecuencia |
|-----------------------|----------------|-----------------------|
| “La corriente de la bomba no la conozco, hay que medirla” | No hay dato de corriente nominal ni de arranque. | Bloquea la elección de fuente, fusible, diodo y contacto. Medir antes de comprar. |
| “Se conecta a la corriente del toma” (bomba) | No distingue 12 V DC de 220 V AC. | **No asumir 12 V DC.** Confirmar tensión; si es 220 V AC, rediseñar potencia y descartar el diagrama de 12 V DC. |
| “El relé solo hace que prenda o apague” | El relé conmuta la bomba, no regula ni protege. | Correcto como conmutación; **no** es seguridad independiente. Añadir flotadores + relé de seguridad. |
| “Se conecta a la corriente del toma” (TDS) | Describe la alimentación del módulo. | No confundir con la excitación de la sonda. Medir la sonda en AC/DC antes de decidir si D6 mitiga la polarización. |
| “Tiene VCC, GND, IN y NO/COM/NC; sin jumper JD-VCC visible” | Módulo de relé de 1 canal, probable activo bajo, sin aislamiento confirmado. | Mantener `RELAY_ACTIVE_LOW 1` y pull-up de 10 kΩ en D5. Verificar nivel activo y capacidad de contacto. |
| “Arduino Pin 4 va al TDS y de ahí sale al DS18B20” | Bus 1-Wire compartido entre módulo TDS y sonda. | Verificar contención; si hay duda, segunda DS18B20 directa a D4. |
| “No pondré flotadores, el relé es el que activa la bomba” | No habrá protección independiente de nivel. | `USE_FLOATS 0`; el sistema queda como prototipo y no debe operar sin supervisión. |

Estas aclaraciones son de hardware y documentación: no cambian `config.h` ni el sketch, así que las cifras de compilación y memoria de §1–§2 siguen vigentes. La única decisión de firmware asociada es mantener `USE_FLOATS 0` y `RELAY_ACTIVE_LOW 1` hasta verificar el relé y instalar los flotadores.

## 7. Pendientes bloqueantes antes de conectar la bomba

| Pendiente | Por qué bloquea | Qué hacer |
|-----------|-----------------|-----------|
| **Tensión real de la bomba** | Define si aplica el diseño de 12 V DC o si hay que rediseñar para 220 V AC. | Medir con multímetro en la toma y en la placa de la bomba. Si es 220 V AC, **no** usar el diagrama de 12 V DC ni protoboard. |
| Corriente nominal y de arranque de la bomba | Define fusible, diodo, cable y contacto del relé de seguridad. | Medir con pinza DC (o pinza AC si es de red) y leer la placa. |
| Excitación de la sonda TDS (AC/DC) | Define si la sonda se polariza y si D6 sirve como mitigación. | Medir entre electrodos con el módulo encendido: DC, AC y osciloscopio si es posible. “Corriente del toma” es la alimentación, no la excitación. |
| Bus D4 compartido con el módulo TDS | Si el módulo maneja el 1-Wire o tiene otra pull-up, el Arduino no puede leer la DS18B20. | Medir continuidad y tráfico; si hay duda, usar una segunda DS18B20 directa a D4. |
| Función real del relé | El usuario dice que “solo hace que prenda o apague” la bomba; hay que confirmar que no conmuta también el TDS. | Trazar NO/COM/NC y probar con cargas separadas. |
| Modelo exacto del TDS Meter V1.0 | Define si la sonda es AC o DC y si el módulo compensa temperatura. | Foto/etiqueta del módulo y medida en la sonda. |
| Jumper JD-VCC y nivel activo del relé | Define si la masa del opto está aislada, cómo alimentar la bobina y si D5 necesita pull-up o pull-down. | Revisar la placa, medir continuidad y probar 20 arranques con la bomba desconectada. |
| Capacidad de contacto del relé | El contacto debe soportar la corriente de arranque, no solo la nominal. | Leer la hoja de datos del JQC-3FF-S-Z y comparar con la medición. |
| Flotadores SPDT y relé de seguridad | Sin ellos no hay protección independiente. El relé principal no lo es. | Instalar 2 flotadores + relé cableado antes de operación desatendida. |
| Segunda DS18B20 | La sonda actual parece estar conectada al módulo TDS. | Añadir una DS18B20 para D4 o confirmar el circuito. |

## 8. Pruebas mínimas antes de conectar la bomba real

1. **Medir la tensión de la bomba** en la toma y en la placa. Si es 220 V AC, detenerse y rediseñar la potencia (§5 de `02_diagrama_conexiones.md`).
2. **Medir corriente nominal y de arranque** con la bomba conectada a su tensión real y con carga de agua.
3. Verificar el bus D4: desconectar el módulo TDS y confirmar que la DS18B20 responde; reconectar y comprobar que sigue respondiendo sin errores. Si falla, usar una segunda sonda directa a D4.
4. Medir la sonda TDS entre electrodos en DC y AC para identificar la excitación.
5. Alimentar el Arduino 20 veces y confirmar que el relé no se activa en el reset.
6. Medir con multímetro la salida del relé: OFF en reposo, ON solo cuando el firmware lo ordena.
7. Sustituir la bomba por una carga ficticia equivalente (foco de 12 V si es DC; carga resistiva adecuada si es AC, solo con supervisión competente) y probar el ciclo de riego.
8. Simular nivel bajo y nivel alto; confirmar bomba OFF, alarma y latch.
9. Desconectar el HC-SR04; confirmar `FAULT_LEVEL_SENSOR` tras 3 lecturas inválidas.
10. Desconectar el DS18B20; confirmar aviso `TEMP?` sin parar la bomba.
11. Probar el watchdog: bloquear el loop a propósito y confirmar reinicio con relé OFF.
12. Con los flotadores instalados, abrir el lazo de seguridad y confirmar que la bomba se detiene aunque el Arduino ordene ON.

## 9. Cómo reproducir la compilación

Ver `firmware/README.md`. En resumen:

```powershell
arduino-cli core install arduino:avr
arduino-cli lib install "LiquidCrystal I2C" OneWire DallasTemperature
arduino-cli compile --fqbn arduino:avr:mega firmware/hidroponia_mega
```

Si usas Arduino IDE: instala las tres librerías desde el Library Manager, abre `firmware/hidroponia_mega/hidroponia_mega.ino`, selecciona **Arduino Mega or Mega 2560** y compila.
