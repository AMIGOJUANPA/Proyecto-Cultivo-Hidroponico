# 06 — Riesgos y mitigaciones

## 1. Matriz de riesgos

| Riesgo | Causa típica | Severidad | Mitigación |
|--------|--------------|-----------|------------|
| Tensión de bomba mal identificada | Se asumió 12 V DC, pero “corriente del toma” puede ser 220 V AC | **Crítica: choque eléctrico, incendio, destrucción del relé** | Medir tensión antes de comprar o conectar nada. Si es 220 V AC, rediseñar potencia según `02_diagrama_conexiones.md` §5. |
| Relé principal tomado como seguridad | El relé solo conmuta; no protege ante sensor, firmware o contacto pegado | Alta: bomba imparable | No usarlo como única protección. Añadir flotadores + relé de seguridad cableado. |
| Contención en el bus 1-Wire | D4 compartido entre módulo TDS y DS18B20 | Media: lecturas de temperatura falsas o bloqueo | Verificar el circuito; usar segunda DS18B20 directa a D4 si hay duda. |
| Excitación DC de la sonda TDS | Módulo clon sin oscilador AC | Media: polarización y corrosión de la sonda | Medir la sonda; pulsar D6; preferir módulo con excitación AC. |
| Marcha en seco | Reservorio vacío, bomba sin cebar, entrada obstruida | Alta: sobrecalentamiento y daño de bomba | Flotador bajo NC en serie con relé de seguridad; confirmación de nivel; tiempo máximo ON; flow switch. |
| Desbordamiento de la mesa | Drenaje obstruido, relé pegado, bomba sobredimensionada | Alta: agua sobre electrónica y piso | Flotador alto NC en mesa/bandeja; rebosadero independiente; relé de seguridad cableado; alarma. |
| Relé pegado | Contactos soldados por arco o sobrecorriente | Alta: bomba imparable | Relé de seguridad en serie; flotador alto; prueba periódica; relé rated para motor; diodo flyback. |
| Fallo del HC-SR04 | Condensación, espuma, oleaje, eco falso, cable roto | Media-alta: nivel erróneo | Pozo de calma; mediana; timeout; plausibilidad; flotadores independientes. |
| Fallo del DS18B20 | Cable roto, clon, alimentación parásita | Baja para la bomba; media para EC | 3 hilos, pull-up 4,7 kΩ, chequeo −127/85 °C, segunda sonda si hace falta. |
| Deriva de EC/TDS | Sonda sucia, polarización DC, calibración vieja | Media: nutrientes mal dosificados | Excitación AC o pulsada; limpieza; calibración periódica; guardar fecha y CRC. |
| Corrosión de sonda EC | DC continua en los electrodos | Media: sonda inservible | No dejar DC continua; usar AC o pulso; limitar corriente; apagar entre mediciones. |
| Ruido EMI | Motor de bomba, relé, cables largos | Media: lecturas falsas, resets | Diodo flyback, 100 nF en bomba, snubber si aplica, cables apantallados, separación de potencia y señal, watchdog. |
| Reinicio por brownout | Arranque de bomba, fuente insuficiente | Media: bomba se apaga o relé chasquea | Fuente con margen, condensadores, cables gruesos, masa en estrella, relé OFF en boot. |
| Bloqueo I2C | LCD con cables largos, backpack defectuoso | Media: pantalla congelada | Cables cortos, pull-ups correctos, watchdog, recuperación de bus, LCD paralelo como alternativa. |
| Fallo de firmware | Código bloqueante, corrupción de memoria | Alta: bomba sin control | Watchdog (8 s en setup, 2 s en loop), máquina de estados, delays acotados con `wdt_reset()`, fail-safe, pull-up en relé. |
| Inundación de electrónica | Fuga, condensación, salpicadura | Alta: cortocircuito | Caja IP54/IP65, prensaestopas, drip loops, separar electrónica del agua, sensor de fuga. |
| Sifón | Retorno por debajo del nivel, sin rompe-sifón | Media: drenaje no deseado | Rompe-sifón en el punto alto del retorno; válvula anti-retorno; revisar tras apagar. |
| Algas/calcio en flotadores | Agua estancada, luz, nutrientes | Media: flotador pegado | Limpieza periódica; depósito oscuro; doble flotador o flow switch. |
| Congelación | Invierno, exterior | Media: rotura de bomba/tuberías | Vaciar en invierno; alarma de temperatura; protección térmica. |
| Calibración incorrecta | Solución patrón equivocada, temperatura no medida | Media: dosis errónea | Solución certificada, termómetro, dos puntos, verificación con segunda solución. |
| Contacto deficiente en protoboard | Corriente alta, cables sueltos | Media: arco, fallo intermitente | Para la bomba, usar borneras/terminales, no protoboard; cables firmes; fusible. |
| Adaptador de red en mal estado | Fuente genérica, cable dañado | Alta: choque eléctrico o incendio | Adaptador certificado, carcasa cerrada, alivio de tensión, lejos del agua, RCD en la toma. |

## 2. Riesgos específicos del montaje actual

### Sin flotadores

El sistema actual solo tiene HC-SR04. Eso deja dos huecos:

- **Marcha en seco:** el ultrasónico puede detectar nivel bajo, pero si falla, no hay respaldo.
- **Desbordamiento de la mesa:** el ultrasónico está en el reservorio; no ve la mesa. Un drenaje obstruido puede inundar la mesa sin que el sensor lo detecte.

Mitigación mínima: instalar los dos flotadores SPDT y el relé de seguridad. Si no se instalan, el sistema debe quedarse como banco de pruebas y nunca operar sin supervisión.

El usuario indicó que “el relé es el que activa la bomba”. Eso es correcto como **conmutación**, pero no convierte al relé en una protección independiente: si el contacto se pega, si el firmware falla o si el HC-SR04 da una lectura falsa, nada corta la bomba. La seguridad contra desbordamiento y marcha en seco exige un lazo cableado con contactos NC y un relé de seguridad en serie.

### Bomba: tensión todavía no confirmada

El diseño de potencia de 12 V DC **solo es válido si se confirma que la bomba es de 12 V DC**. La respuesta “se conecta a la corriente del toma” es ambigua.

**Si es 12 V DC:**

- Corriente de arranque mayor que la nominal: el relé y el fusible deben dimensionarse con la corriente real.
- Pico inductivo: sin diodo flyback, se erosionan los contactos y puede fallar el relé.
- Fuente insuficiente: la bomba arranca, el riel de 5 V cae y el Arduino se reinicia. Fuente con margen y condensadores.

**Si es 220 V AC:**

- Riesgo de choque eléctrico y de incendio; el módulo JQC de prototipo no es la solución final.
- Hace falta contactor/relé de potencia para motor, RCD/GFCI, tierra de protección, caja cerrada y cableado de red con sección adecuada.
- **No se puede prototipar en protoboard.** El diagrama de 12 V DC de este proyecto no aplica.
- La corriente de arranque y el factor de potencia del motor definen el contactor y el breaker.

Medir tensión, corriente nominal y corriente de arranque **antes** de comprar fuente, relé, fusible o cable.

### TDS Meter V1.0

- “Se conecta a la corriente del toma” describe la **alimentación del módulo**, no la excitación de la sonda. No se puede concluir que la sonda sea AC solo por eso.
- Si es un clon con excitación DC, la sonda se polariza. El pulso por D6 reduce el tiempo de DC, pero no elimina la polarización.
- Si el módulo tiene entrada de temperatura y el único DS18B20 está allí, hay que verificar que el bus 1-Wire llegue limpio a D4. Si el módulo lo maneja o lo carga con otra pull-up, usa una segunda DS18B20 directa al Arduino.
- La salida de 0–2,3 V debe verificarse. Si puede llegar a 5 V, no uses la referencia de 2,56 V sin protección.
- Confirmar que el relé conmuta la bomba y no el módulo TDS; si conmuta ambos, separar las cargas y revisar el contacto.

## 3. Prioridad de acciones

1. **Confirmar la tensión real de la bomba (12 V DC vs 220 V AC).** Si es 220 V AC, detener el diseño de 12 V DC y rediseñar la potencia.
2. **Medir la corriente nominal y de arranque.** Sin ese dato no se elige fusible, relé, diodo ni cable.
3. **Instalar flotador bajo en el reservorio y flotador alto en la mesa/bandeja.**
4. **Añadir relé de seguridad cableado en serie con la bomba.**
5. **Poner diodo flyback y 100 nF en la bomba** (solo si es DC).
6. **Asegurar relé OFF durante el reset.**
7. **Verificar el bus D4/TDS/DS18B20 y la excitación de la sonda TDS.**
8. **Calibrar HC-SR04 y EC/TDS.**
9. **Añadir watchdog y probar fallos simulados.**
10. **Documentar y etiquetar el cableado.**
