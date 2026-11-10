# 03 — Lista de materiales adicional

No compres los elementos marcados como **“según medición”** hasta confirmar la tensión y la corriente de la bomba, y el modelo exacto del TDS Meter V1.0.

> **Si la bomba resulta de 220 V AC, esta lista no sirve para la etapa de potencia.** En ese caso hacen falta contactor/relé de potencia para motor, RCD/GFCI, puesta a tierra, caja cerrada y cable de red; el módulo JQC-3FF-S-Z de prototipo no es la solución final. Ver `02_diagrama_conexiones.md` §5.

## 1. Protección y conmutación

| Cantidad | Elemento | Especificación / nota |
|---------:|----------|-----------------------|
| 1 | Módulo relé JQC-3FF-S-Z | Ya lo tienes. 1 canal, bobina 5 V, contacto SPDT. Verifica corriente de bobina y capacidad de contacto. |
| 1 | Relé de seguridad | **Solo si la bomba es 12 V DC.** 12 V DC, bobina de baja corriente, contactos rated para la corriente de la bomba. Usa el contacto **NO** para la bomba: si la bobina se desenergiza, la bomba se apaga. Ejemplo: relé automotriz 12 V 30 A con zócalo, o módulo relé 12 V con contactos adecuados. |
| 1 | Diodo flyback de bomba | 1N5408 (3 A / 1000 V) si la bomba consume <3 A. Para más corriente, usa uno de 6 A o 10 A. Verifica la corriente real. |
| 1 | Diodo flyback de bobina | 1N4007 para la bobina del relé de seguridad. |
| 1 | Condensador de bomba | 100 nF / 100 V cerámico, en paralelo con la bomba. |
| 3 | Portafusibles + fusibles | F1 bomba: 1,5–2× la corriente nominal. F2 bobina de seguridad: 0,5 A. F3 control 5 V: 0,5 A. No improvises el calibre. |
| 1 | Interruptor/desconectador DC | 2 polos, rated para la corriente de la bomba, para mantenimiento. |

## 2. Sensores de seguridad

| Cantidad | Elemento | Especificación / nota |
|---------:|----------|-----------------------|
| 2 | Flotadores SPDT | IP68, contacto seco, NC/NO configurable, rated ≥1 A a 12 V. Uno para nivel bajo en reservorio y otro para nivel alto en mesa/canal. |
| 1 | Flow switch | Opcional pero recomendado. Detecta bomba obstruida o sin caudal. Entrada a D23. |
| 1 | Sensor de corriente | Opcional. Para verificar que la bomba está apagada cuando se ordena OFF. |

## 3. Alimentación

| Cantidad | Elemento | Especificación / nota |
|---------:|----------|-----------------------|
| 1 | Fuente 12 V DC | **Solo si la bomba es 12 V DC.** Corriente ≥ corriente de la bomba + 30 %. Certificada, con carcasa. No comprar hasta medir tensión y corriente. |
| 1 | Convertidor 12 V→5 V | ≥2 A, con fusible de entrada y salida. Alternativa: fuente de 5 V 2 A independiente. |
| 1 | Fusible 5 V | 0,5 A. |
| 1 | Condensador 100 µF / 25 V | En el riel de 5 V, cerca del Arduino. |
| 2 | Condensadores 100 nF | Desacoplo en 5 V y en la entrada analógica. |

## 4. Acondicionamiento de señal

| Cantidad | Elemento | Especificación / nota |
|---------:|----------|-----------------------|
| 1 | DS18B20 extra | Si el que tienes está conectado al módulo TDS, necesitas otro para D4. |
| 1 | Resistencia 4,7 kΩ 1/4 W | Pull-up de DS18B20. |
| 3 | Resistencias 10 kΩ 1/4 W | Pull-up del relé y de los dos flotadores. |
| 2 | Resistencias 1 kΩ 1/4 W | Serie en ECHO y en A0. |
| 1 | Resistencia 330 Ω 1/4 W | LED de alarma. |
| 2 | Condensadores 100 pF / 100 nF | Filtro de ECHO y de A0. |
| 1 | P-MOSFET high-side o load switch | Para D6. Ejemplo: AO3401 o equivalente, con 10 kΩ de gate a 5 V y 100 Ω serie. |
| 1 | Transistor NPN/2N7002 + resistencias | Nivelador para el gate del P-MOSFET, si no usas load switch. |
| 1 | Buzzer activo 5 V | Si consume >20 mA, transistor 2N2222/BC547, 1 kΩ en base y 1N4148 en el colector. |
| 1 | LED 5 mm + 330 Ω | Alarma visual. |
| 1 | Pulsador | Reset de falla, a GND. |

## 5. Cableado y mecánica

| Cantidad | Elemento | Especificación / nota |
|---------:|----------|-----------------------|
| 1 | Caja IP54/IP65 | Separa la electrónica del agua. Prensaestopas y alivio de tensión. |
| 1 | Borneras / regletas | Para 12 V, 5 V, GND y sensores. No uses solo protoboard para la bomba. |
| 1 | Cable 2×1 mm² | Alimentación de bomba 12 V (solo si se confirma 12 V DC). |
| 1 | Cable 2×0,5 mm² | Alimentación de control 5 V. |
| 1 | Cable apantallado 2×0,2 mm² | Flotadores y sensores de nivel, si el recorrido es largo. |
| 1 | Funda termorretráctil, terminales, bridas | Aislamiento y sujeción. |
| 1 | Etiquetas | “12 V”, “5 V”, “GND”, “BOMBA”, “NO TOCAR”. |

## 6. Calibración y herramientas

| Cantidad | Elemento | Especificación / nota |
|---------:|----------|-----------------------|
| 1 | Solución patrón EC | 1413 µS/cm a 25 °C y/o 12,88 mS/cm. No uses 342 ppm TDS como si fuera 1413 µS/cm. |
| 1 | Termómetro de referencia | Para calibrar la compensación de temperatura. |
| 1 | Multímetro | Continuidad, tensión, resistencia. |
| 1 | Pinza amperimétrica DC | Para medir la corriente real de la bomba. |
| 1 | Regla o cinta métrica | Calibración del HC-SR04 y altura del reservorio. |

## 7. Elementos que no debes comprar todavía

- Fuente, relé de potencia o cable de bomba: primero confirma **12 V DC vs 220 V AC** y mide la corriente.
- Fuente de 12 V de corriente arbitraria: falta la corriente de la bomba.
- Diodo de 3 A si la bomba consume más de 3 A: se quemaría.
- Relé de seguridad de contacto insuficiente: verifica la corriente de arranque.
- Solución de calibración incorrecta: 342 ppm TDS no es 1413 µS/cm.
- Segunda DS18B20: primero verifica si el bus D4 compartido funciona; si hay contención, entonces cómprala.
