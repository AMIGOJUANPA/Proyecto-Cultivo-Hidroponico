# Firmware — Hidroponía MEGA 2560

Sketch único y parametrizable: `hidroponia_mega/hidroponia_mega.ino` + `hidroponia_mega/config.h`.

## Requisitos

| Elemento | Versión probada |
|----------|-----------------|
| Arduino Mega 2560 | `arduino:avr:mega` |
| Core Arduino AVR | 1.8.8 |
| LiquidCrystal I2C | 1.1.2 |
| OneWire | 2.3.8 |
| DallasTemperature | 4.0.6 |

## Arduino IDE

1. **Library Manager**: instala `LiquidCrystal I2C`, `OneWire` y `DallasTemperature`.
2. Abre `hidroponia_mega/hidroponia_mega.ino`.
3. Selecciona **Arduino Mega or Mega 2560** y el puerto.
4. Ajusta `config.h` antes de cargar:
   - `USE_FLOATS` (`0` si aún no hay flotadores).
   - `RELAY_ACTIVE_LOW` según el módulo real.
   - `EC_USE_2V56` según la salida del TDS Meter.
   - `LCD_COLS` / `LCD_ROWS` según el LCD.
   - Umbrales de nivel y calibración EC.
5. Compila y carga.

## Arduino CLI

```powershell
arduino-cli core install arduino:avr
arduino-cli lib install "LiquidCrystal I2C" OneWire DallasTemperature
arduino-cli compile --fqbn arduino:avr:mega --warnings all .\hidroponia_mega
arduino-cli upload  --fqbn arduino:avr:mega -p COM3 .\hidroponia_mega
```

Cambia `COM3` por el puerto real.

## Configuración mínima antes de energizar la bomba

- **Confirma la tensión de la bomba:** 12 V DC o 220 V AC. Si es 220 V AC, el diagrama de potencia de 12 V DC no aplica; usa `../docs/02_diagrama_conexiones.md` §5.
- **Mide la corriente nominal y de arranque:** sin ese dato no se elige fusible, relé ni cable.
- `RELAY_ACTIVE_LOW`: verifica el módulo con la bomba desconectada.
- `USE_FLOATS 0`: el sketch muestra `SIN FLOTADORES`; es solo un prototipo.
- `EC_SLOPE_DEFAULT` y `EC_OFFSET_DEFAULT`: son valores de ejemplo; calibra con solución patrón.
- `SENSOR_HEIGHT_CM`, `LEVEL_LOW_CM`, `LEVEL_HIGH_CM`: mide la geometría real.
- `FLOAT_FAULT_ACTIVE_HIGH`: `0` para contacto a GND con pull-up interno.

## Seguridad

- La bomba **nunca** se alimenta desde el Arduino; el relé solo conmuta.
- D5 debe tener pull-up externo de 10 kΩ si el módulo es activo en bajo.
- D6 (alimentación del módulo EC) debe tener pull-down externo de 10 kΩ.
- Sin flotadores SPDT y relé de seguridad cableado, el sistema **no** es apto para operación desatendida.

## Documentación

- `../README.md` — resumen y mapa de pines.
- `../docs/01_revision_pines.md` — validación de pines.
- `../docs/02_diagrama_conexiones.md` — diagramas ASCII.
- `../docs/03_lista_materiales.md` — BOM.
- `../docs/04_firmware_estructura.md` — máquina de estados y pseudocódigo.
- `../docs/05_calibracion.md` — calibración.
- `../docs/06_riesgos.md` — riesgos.
- `../docs/07_revision_firmware.md` — compilación, correcciones y pendientes.
