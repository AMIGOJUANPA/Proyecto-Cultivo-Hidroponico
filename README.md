# Sistema hidropónico de recirculación — Arduino MEGA 2560

## Estado del diseño

Revisión eléctrica y de firmware del prototipo. **El sistema NO es apto para operación desatendida sin flotadores independientes de seguridad.** El HC-SR04 mide el nivel del reservorio, pero no detecta el desbordamiento de la mesa/canal ni sustituye un contacto de seguridad ante fallo del sensor, del relé o del programa.

> **Advertencia por respuestas pendientes:** no está confirmada la tensión real de la bomba ni la excitación de la sonda TDS. Si la bomba es de **220 V AC**, no uses el diagrama de potencia de 12 V DC de este documento ni pruebes en protoboard. Sin flotadores, el relé principal no es una protección independiente.

**Estado de compilación:** verificado con Arduino CLI 1.5.1, core `arduino:avr` 1.8.8 y placa Mega 2560. El sketch compila sin errores ni advertencias propias; en la configuración por defecto usa 21 242 B de flash (8 %) y 788 B de RAM global (9 %). Detalle, variantes y correcciones en `docs/07_revision_firmware.md`.

| Elemento | Confirmado | Implicación |
|----------|-----------|-------------|
| Arduino MEGA 2560 + protoboard | Sí | Prototipo, no PCB final. |
| HC-SR04 | Sí | Nivel del reservorio. No es sensor de seguridad. |
| LCD | I2C con backpack | D20/D21. |
| Temperatura | DS18B20 de 3 cables | D4 con pull-up 4,7 kΩ. |
| Bomba | 12 V DC declarada, **sin confirmar** | Relé + diodo flyback solo si es DC. Falta corriente nominal y de arranque. La respuesta “corriente del toma” es ambigua: si es 220 V AC, el diseño de potencia cambia por completo. |
| Módulo de relé | JQC-3FF-S-Z, 1 canal, LED verde/rojo | Entrada VCC/GND/IN y contactos NO/COM/NC. Sin jumper JD-VCC visible. Verificar nivel activo, capacidad de contacto y que conmute la bomba (no el TDS). |
| EC/TDS | TDS Meter V1.0, salida 0–2,3 V | Confirmar alimentación del módulo y si la sonda recibe AC o DC. |
| Flotadores | **No instalados** | El relé principal no es una protección independiente. Se recomiendan 2 SPDT: bajo en reservorio, alto en mesa/canal. |
| DS18B20 | D4 va al módulo TDS y de ahí a la sonda | Verificar si el bus 1-Wire se comparte sin conflicto; si no, usar una segunda sonda directa a D4. |

## Mapa de pines final

| Pin | Función | Condición |
|-----|---------|-----------|
| D30 | HC-SR04 Sensor Ultrasonido TRIG | Directo. |
| D31 | HC-SR04 Sensor Ultrasonido ECHO | 1 kΩ serie opcional; 100 pF a GND si el cable es largo. |
| D6 | DS18B20 DATA | Pull-up 4,7 kΩ a 5 V. |
| D2 | Relé IN | 10 kΩ a 5 V si es activo bajo; a GND si es activo alto. | |
| D20 | LCD SDA | I2C. |
| D21 | LCD SCL | I2C. |
| A0 | EC/TDS OUT | 1 kΩ serie + 100 nF a GND. Referencia 2,56 V o 5 V según módulo. |
| A1 | Reserva analógica | Libre. |
| A2 | Reserva | Libre al usar DS18B20. |
| D13 | Switch | Opcional, recomendado. |

## Decisiones críticas

1. **La bomba se declaró de 12 V DC, pero no está confirmado.** La respuesta “se conecta a la corriente del toma” puede significar 12 V DC o 220 V AC. Si es 220 V AC, el diagrama de potencia de 12 V DC no aplica y hace falta contactor/relé de motor, RCD, tierra y caja cerrada; **nada de protoboard**. Falta además la corriente nominal y de arranque para elegir fusible, diodo y calibre de cable.
2. **No hay flotadores.** El ultrasónico sirve para control y visualización, pero no para seguridad. El relé principal solo conmuta la bomba; no es una protección independiente ante sensor, firmware o contacto pegado. Se recomienda añadir dos flotadores SPDT y un relé de seguridad cableado en serie con la bomba. Sin ellos, el sistema queda como prototipo de laboratorio, nunca desatendido.
3. **TDS Meter V1.0.** “Se conecta a la corriente del toma” describe la alimentación del módulo, no la excitación de la sonda. Si es un clon del DFRobot SEN0244, puede tener excitación AC y entrada para DS18B20; si es una versión DC, la sonda se polariza. Hay que medir la sonda en AC/DC. No compartas el mismo DS18B20 entre el módulo TDS y el Arduino sin verificar el bus: usa dos sondas o deja que el módulo compense.
4. **Relé JQC-3FF-S-Z.** Es un relé de bobina de 5 V, contacto SPDT. El módulo suele ser activo en bajo. Verifica si tiene jumper JD-VCC y si la masa del opto está realmente separada. El contacto del relé es la barrera de conmutación, no de seguridad.
5. **DS18B20.** D4 va al módulo TDS y de ahí a la sonda. Si el módulo maneja el bus o carga el dato con otra pull-up, el Arduino no podrá leerla. Verifica el bus; si hay duda, añade una segunda sonda DS18B20 directa a D4.

## Archivos

- `docs/01_revision_pines.md` — validación y correcciones del mapa de pines.
- `docs/02_diagrama_conexiones.md` — diagramas ASCII de control y potencia (12 V DC condicional; requisitos para 220 V AC).
- `docs/03_lista_materiales.md` — lista de materiales adicional.
- `docs/04_firmware_estructura.md` — pseudocódigo, máquina de estados y estructura del sketch.
- `docs/05_calibracion.md` — calibración de HC-SR04, EC/TDS, DS18B20 y pruebas de seguridad.
- `docs/06_riesgos.md` — riesgos y mitigaciones.
- `docs/07_revision_firmware.md` — compilación, correcciones aplicadas, variantes y pendientes.
- `firmware/README.md` — cómo compilar y cargar el sketch.
- `firmware/hidroponia_mega/` — sketch parametrizable.

## Próximos pasos

1. **Confirmar la tensión de la bomba:** 12 V DC o 220 V AC. Si es 220 V AC, rediseñar la etapa de potencia antes de continuar.
2. Medir la **corriente nominal y de arranque** de la bomba a su tensión real.
3. Confirmar el **modelo exacto del TDS Meter V1.0** y medir si la sonda recibe AC o DC.
4. Verificar el **bus D4 compartido** entre el módulo TDS y la DS18B20; si hay contención, usar una segunda sonda.
5. Decidir si se instalan los **dos flotadores SPDT** y el relé de seguridad.
6. Calibrar el HC-SR04 con el reservorio real.
7. Calibrar EC/TDS con solución patrón a 25 °C.
