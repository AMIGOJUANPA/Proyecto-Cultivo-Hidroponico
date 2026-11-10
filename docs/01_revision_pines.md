# 01 — Revisión del mapa de pines y correcciones

## 1. Datos confirmados

| Dato | Respuesta | Consecuencia |
|------|-----------|--------------|
| LCD | I2C con backpack | D20/D21; no se necesitan 6 pines. |
| Temperatura | DS18B20 de 3 cables; D4 va al módulo TDS y de ahí a la sonda | D4, pull-up 4,7 kΩ, 1-Wire. **Verificar que el módulo TDS no contentione el bus**; si lo hace, usar una segunda DS18B20 directa a D4. |
| Bomba | **12 V DC declarada, no confirmada.** La respuesta “se conecta a la corriente del toma” es ambigua | Si es 12 V DC: relé + diodo flyback. Si es 220 V AC: **no usar este diseño de potencia**; ver §3.2. Falta corriente nominal y de arranque. |
| Relé | JQC-3FF-S-Z, 1 canal, LED verde/rojo, VCC/GND/IN y NO/COM/NC | Probable activo bajo; verificar JD-VCC y capacidad de contacto. El usuario indica que conmuta la bomba; confirmar que no conmuta el TDS. |
| Flotadores | No hay | El ultrasónico no es seguridad y el relé principal tampoco. Se recomiendan 2 SPDT + relé de seguridad cableado. |
| EC/TDS | TDS Meter V1.0, 0–2,3 V | “Corriente del toma” describe la **alimentación del módulo**, no la excitación de la sonda. Verificar AC/DC con multímetro/osciloscopio. |
| Flotador alto | No hay; tanque debajo del cultivo | El riesgo real es desbordamiento de la mesa. |

## 2. Mapa de pines validado

| Pin | Función | Veredicto | Corrección / condición |
|-----|---------|-----------|------------------------|
| D2 | HC-SR04 TRIG | OK | Sin cambios. |
| D3 | HC-SR04 ECHO | OK | Mega es 5 V. 1 kΩ serie opcional; 100 pF a GND si el cable es largo. |
| D4 | DS18B20 DATA | OK | Pull-up 4,7 kΩ a 5 V. No usar alimentación parásita. |
| D5 | Relé IN | OK con condición | 10 kΩ a 5 V si activo bajo; 10 kΩ a GND si activo alto. Probar 20 arranques con bomba desconectada. |
| D6 | Habilitación EC | **Corregir** | No MOSFET N en baja lado: la salida analógica queda flotante y puede alimentarse por A0. Usar high-side P-MOS/load switch o conmutar la excitación de la sonda. |
| D7 | Buzzer | OK | Buzzer activo 5 V; transistor si >20 mA. |
| D8 | LED de alarma | **Añadir** | 330 Ω a GND. |
| D9 | Flotador bajo | Recomendado | NC al lazo de seguridad (abre al bajar). NO a GND para la entrada del Arduino, activo bajo. Pull-up 10 kΩ + 100 nF; debounce 100 ms. |
| D10 | Flotador alto | Recomendado | NC al lazo de seguridad (abre al subir). NO a GND para la entrada del Arduino, activo bajo. Debe ir en la **mesa/canal**, no en el reservorio. |
| D20 | LCD SDA | OK | I2C del Mega. Pull-ups del backpack. |
| D21 | LCD SCL | OK | Cables cortos; si el LCD es paralelo, cambiar el mapa. |
| A0 | EC/TDS OUT | OK con mejoras | 1 kΩ serie + 100 nF a GND. Verificar que la salida no supere la referencia. |
| A1 | Reserva analógica | OK | Libre; el flow switch va en D23. |
| A2 | Reserva | OK | Libre porque el sensor es DS18B20, no NTC. |
| D22 | Reset de falla | **Añadir** | Botón a GND con pull-up; 2 s. |
| D23 | Flow switch | Opcional | Entrada digital con pull-up u opto. |
| D24 | Segundo relé | Opcional | Relleno automático. |
| 5V/GND | Rieles | **Corregir** | Fuente 5 V 2 A externa; masa en estrella; fusible. No alimentar todo desde el regulador del Mega. |

## 3. Correcciones críticas

### 3.1 El HC-SR04 no es un sensor de seguridad

El ultrasónico mide el nivel del reservorio. No detecta:

- Desbordamiento de la mesa/canal por drenaje obstruido.
- Relé pegado: la bomba sigue corriendo y el reservorio no sube; la mesa puede desbordar.
- Condensación, espuma, oleaje o eco falso.
- Rotura del cable: no hay circuito de fallo seguro.

Por eso se recomiendan dos flotadores SPDT:

1. **Nivel bajo en el reservorio:** NC cerrado con agua; abre al bajar. Protege marcha en seco.
2. **Nivel alto en la mesa/canal o bandeja de drenaje:** NC cerrado en seco; abre al subir. Protege desbordamiento y relé pegado.

Los flotadores se cablean en serie con la bobina de un **relé de seguridad** cuyos contactos van en serie con la bomba. Así la protección funciona aunque el Arduino se cuelgue o el relé principal se pegue.

### 3.2 Bomba: tensión aún no confirmada

La bomba se declaró como **12 V DC**, pero la respuesta “se conecta a la corriente del toma” no distingue entre una toma de 12 V DC y un enchufe de 220 V AC. Esa diferencia cambia por completo la etapa de potencia:

| Si la bomba es… | Etapa de potencia | Estado |
|-----------------|-------------------|--------|
| **12 V DC** | Fuente 12 V DC + relé principal + relé de seguridad + diodo flyback. Diagrama de §2 de `02_diagrama_conexiones.md`. | Aplicable solo tras confirmar la tensión. |
| **220 V AC** | Contactor o relé para cargas inductivas con contacto rated para motor a 220 V AC; RCD/GFCI; puesta a tierra; caja aislante; borneras; cable de sección adecuada. **Nada de protoboard.** | **No usar el diagrama de 12 V DC.** Requiere rediseño y verificación por persona competente. |

**Arquitectura para 12 V DC (si se confirma):**

```text
+12 V → F1 → contacto **NO** del relé de seguridad → contacto COM/NO del módulo relé → bomba → 0 V
Diodo flyback en paralelo con la bomba: cátodo a +, ánodo a −
Condensador 100 nF / 100 V en paralelo con la bomba

Circuito de la bobina del relé de seguridad:
+12 V → flotador bajo NC → flotador alto NC → bobina del relé de seguridad → 0 V
Diodo flyback en paralelo con la bobina
```

El relé principal lo controla D5. El relé de seguridad lo controlan los flotadores. La bomba solo funciona si ambos contactos están cerrados. Si el relé principal se pega, el flotador abre el relé de seguridad y corta la bomba.

**Si se confirma 220 V AC:** el relé JQC-3FF-S-Z de prototipo **no** es la solución final. Se necesita un contactor o relé de potencia con contacto rated para la corriente de arranque del motor, cableado dentro de una caja cerrada, con tierra de protección y RCD en la toma. El Arduino solo debe gobernar la bobina de baja tensión a través de un módulo optoacoplado o un relé intermedio; la conmutación de red debe quedar físicamente separada. Este cambio no se puede validar en protoboard ni sin medir la corriente real.

> **Pendiente bloqueante:** medir tensión y corriente (nominal y de arranque) antes de comprar fuente, relé, fusible o cable.

### 3.3 El módulo JQC-3FF-S-Z

- Bobina de 5 V, contacto SPDT. Verifica la corriente de bobina y la capacidad de contacto en la hoja de datos.
- La mayoría de módulos con LED verde (alimentación) y rojo (relé activo) son **activos en bajo**.
- Si tiene jumper **JD-VCC**, retíralo y alimenta JD-VCC desde una fuente de 5 V separada. Verifica si la masa del lado de entrada está realmente separada; muchos módulos no aíslan galvánicamente.
- No uses el módulo para conmutar 220 V. Para 12 V DC es aceptable si el contacto y la pista soportan la corriente de arranque.
- Prueba de arranque: con la bomba desconectada, alimenta el Arduino 20 veces y confirma que el relé no se activa durante el reset.

### 3.4 TDS Meter V1.0

- La salida típica es 0–2,3 V. Con el ADC de 10 bits y referencia de 5 V, la resolución es ~4,9 mV; con la referencia interna de 2,56 V, ~2,5 mV. La referencia de 2,56 V es mejor si la salida nunca la supera.
- No confundas **TDS (ppm)** con **EC (µS/cm)**. 342 ppm TDS no equivale a 1413 µS/cm. Calibra con una solución certificada en µS/cm a 25 °C.
- **“Se conecta a la corriente del toma” es la alimentación del módulo, no la excitación de la sonda.** Para saber si la sonda recibe AC o DC hay que medir entre sus dos electrodos con el módulo encendido: multímetro en DC y en AC, y si es posible osciloscopio. No lo deduzcas del tipo de conector ni del nombre “TDS”.
- Si el módulo es un clon con excitación DC, la sonda se polariza y se corroe. El pulso de D6 reduce el daño, pero no lo elimina. Si es AC, D6 puede usarse solo para apagar el módulo entre mediciones.
- El módulo puede tener una entrada de temperatura para DS18B20. Si tu única sonda está conectada allí y el módulo maneja el bus, el Arduino no podrá leerla en D4. Verifica el bus (§3.5) o añade una segunda DS18B20 para el Arduino; si el módulo compensa internamente, puedes usar su salida ya compensada.

### 3.5 DS18B20 y bus compartido con el módulo TDS

- 3 cables: VCC, GND, DATA. Pull-up de 4,7 kΩ entre DATA y 5 V.
- No uses alimentación parásita en cables largos.
- Verifica que no devuelva −127 °C (desconectado) ni 85 °C (valor de reset).
- Si el cable es largo, añade 100 Ω serie en DATA y 100 nF a GND en el extremo del Arduino.
- **Caso actual:** D4 va al módulo TDS y de ahí sale a la DS18B20. Eso solo funciona si el módulo se limita a pasar el bus 1-Wire sin manejarlo. Antes de conectar, mide continuidad y tensión en reposo del pin de datos del módulo; si el módulo tiene su propio maestro 1-Wire o una resistencia de pull-up distinta, habrá contención y lecturas erróneas.
- **Solución segura:** dejar la DS18B20 del módulo solo para la compensación interna del TDS (si existe) y conectar una **segunda DS18B20 directamente a D4** con su pull-up de 4,7 kΩ. El firmware ya está preparado para leer D4; no requiere cambios.

### 3.6 Alimentación y masas (válido para 12 V DC; si hay 220 V AC, ver §3.2)

- Fuente de 12 V para la bomba, con fusible (**solo si la bomba es de 12 V DC**).
- Convertidor 12 V→5 V de al menos 2 A, o fuente de 5 V independiente certificada.
- Alimenta el Arduino por el pin 5 V (regulado), nunca por VIN con 5 V.
- Masa en estrella en el negativo de la fuente de 5 V. La corriente de la bomba no debe circular por las pistas del Arduino.
- Si usas una sola fuente de 12 V con buck de 5 V, el negativo es común, pero el lazo de potencia de la bomba debe ir por cables separados y gruesos.
- Fusibles: uno en la línea de 12 V de la bomba, otro en la línea de 5 V de control. Calibre según la corriente real; no improvises.

### 3.7 Arranque seguro

- D5 debe tener pull-up externo de 10 kΩ a 5 V si el módulo es activo bajo. Durante el reset, el pin del Mega queda en alta impedancia y el pull-up mantiene el relé apagado.
- Si el módulo es activo alto, usa pull-down de 10 kΩ a GND.
- El watchdog del AVR reinicia el Mega si el programa se cuelga; al reiniciar, el pin vuelve a entrada y el pull-up apaga el relé.
- Prueba con bomba desconectada y con una carga ficticia equivalente (foco de 12 V si es DC) antes de conectar la bomba real.
