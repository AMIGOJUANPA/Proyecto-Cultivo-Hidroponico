# 05 — Rutinas de calibración

## 1. HC-SR04 (nivel del reservorio)

### Montaje

- Sensor perpendicular a la superficie del agua, rígido, por encima del nivel máximo.
- Respeta la zona muerta: mínimo 2 cm según el datasheet; en la práctica, deja 3–5 cm entre el sensor y el nivel máximo.
- Usa un pozo de calma o un tubo corto para evitar oleaje, espuma y condensación.
- Si hay condensación, valora un JSN-SR04T impermeable.
- El haz tiene ~15° de apertura; evita paredes cercanas y objetos que devuelvan eco falso.

### Procedimiento

1. Mide con una regla la distancia desde la cara del sensor hasta el fondo del reservorio. Llamémosla `ALTURA_SENSOR_CM`.
2. Con el reservorio a un nivel conocido, mide la altura real del agua `NIVEL_REAL_CM` desde el fondo.
3. Lee la distancia del sensor al agua `DISTANCIA_MEDIDA_CM` con el código, usando mediana de 7 muestras.
4. Calcula `ALTURA_SENSOR_CM = DISTANCIA_MEDIDA_CM + NIVEL_REAL_CM`.
5. Repite a 2–3 niveles distintos. Si los valores de `ALTURA_SENSOR_CM` no coinciden, corrige el montaje o ajusta un offset.
6. Ajusta `NIVEL_BAJO_CM` y `NIVEL_ALTO_CM` según la geometría real. No uses porcentajes inventados.
7. Comprueba con la bomba encendida y apagada. Si el nivel oscila, aumenta el filtro de mediana y usa pozo de calma.

### Fórmula

```text
distancia_cm = pulseIn(ECHO, HIGH) / 58.0     // aproximación a 20 °C
nivel_cm     = ALTURA_SENSOR_CM - distancia_cm
```

Con compensación de temperatura del aire:

```text
v_m_s = 331.3 + 0.606 * T_aire_C
distancia_cm = v_m_s * t_us / 20000.0
```

La temperatura del agua no es la temperatura del aire. Si el sensor está en una cámara húmeda, mide el aire cerca del sensor o acepta el error y calibra a la temperatura de trabajo.

### Filtro

- 7 muestras, ordenar y tomar la mediana.
- Descartar `pulseIn == 0` (timeout).
- Descartar distancias < 2 cm o > altura máxima.
- Si hay 3 fallos consecutivos, declarar falla de sensor.
- Limitar el cambio máximo por muestra (rate limit) para ignorar ecos falsos.

## 2. EC/TDS (TDS Meter V1.0)

### Antes de calibrar

- Identifica el módulo exacto y si la sonda recibe AC o DC. Si no lo sabes, mide con un osciloscopio o un multímetro en la sonda: una tensión continua fija indica excitación DC; una alterna indica AC.
- **No uses 342 ppm TDS como si fueran 1413 µS/cm.** Son escalas distintas. Usa una solución certificada en µS/cm a 25 °C.
- Soluciones típicas: 1413 µS/cm (0,01 M KCl) y 12,88 mS/cm (0,1 M KCl). Para rango bajo, 84 µS/cm.
- La sonda debe estar limpia. Enjuaga con agua destilada y sécala suavemente; no frotes los electrodos.
- No toques las paredes del recipiente. Evita burbujas.
- La sonda debe quedar aislada de metal conectado a masa dentro del agua (cuerpo de bomba, chasis). Si no, la lectura se hunde.

### Procedimiento de un punto

1. Prepara la solución patrón a 25 °C. Anota su EC real y su incertidumbre.
2. Sumerge la sonda, agita suavemente y espera 30–60 s.
3. Lee el ADC promedio de 32 muestras y la temperatura.
4. Calcula la tensión: `V = raw * VREF / 1023`.
5. Ajusta el offset con agua destilada o con el cero del módulo, si es estable.
6. Calcula la pendiente: `EC_uScm = PENDIENTE * V + OFFSET`.
7. Guarda `PENDIENTE` y `OFFSET` en EEPROM.
8. Verifica con una segunda solución. Si el error es >10 %, usa calibración de dos puntos.

### Procedimiento de dos puntos

Con dos soluciones, por ejemplo 1413 µS/cm y 12,88 mS/cm:

```text
PENDIENTE = (EC2 - EC1) / (V2 - V1)
OFFSET    = EC1 - PENDIENTE * V1
```

Si el módulo es no lineal, usa una tabla de interpolación o un polinomio. No forces una recta si los puntos se desvían.

### Compensación de temperatura

```text
EC_25 = EC_T / (1 + 0.02 * (T - 25))
```

- `0.02` es un valor típico para soluciones nutritivas; verifica con tu solución.
- Si el módulo TDS ya compensa internamente, no vuelvas a compensar. Configura `EC_MODULE_COMPENSATED`.
- La temperatura del agua es la del DS18B20. Si la sonda de temperatura está conectada al módulo TDS y no al Arduino, necesitas una segunda DS18B20 o usar la salida ya compensada.

### TDS

```text
TDS_ppm = EC_25 * FACTOR
```

`FACTOR` típico entre 0,5 y 0,7, según la mezcla de sales. No es una medida de nutrientes específicos: la EC mide sales totales. Informa EC y TDS por separado.

### Frecuencia de calibración

- Al poner en marcha.
- Después de limpiar o cambiar la sonda.
- Cada 1–3 meses en uso continuo.
- Si la lectura se desvía >10 % respecto a una solución conocida.

## 3. DS18B20

- No requiere calibración de fábrica, pero verifica en agua con hielo (0 °C) y compara con un termómetro de referencia.
- Comprueba que no devuelva −127 °C (desconectado) ni 85 °C (valor de reset).
- Si el cable es largo, usa 3 hilos (no parásita), 4,7 kΩ de pull-up, 100 Ω serie en DATA y 100 nF a GND en el extremo del Arduino.
- Si la sonda es un clon, la precisión puede ser peor que ±0,5 °C. Verifica.

## 4. Flotadores

> **No están instalados.** El usuario indicó que no pondrá flotadores. Esta sección describe cómo hacerlo si se decide añadirlos; sin ellos no hay protección independiente y el sistema no debe operar sin supervisión.

1. Con un multímetro, identifica COM, NO y NC.
2. Flotador bajo: elige el contacto que está **cerrado con el flotador arriba** (agua por encima del mínimo) y **abre al bajar**.
3. Flotador alto: elige el contacto que está **cerrado con el flotador abajo** (nivel normal) y **abre al subir**.
4. Conecta ambos NC en serie con la bobina del relé de seguridad.
5. Para la entrada del Arduino usa el contacto **NO a GND** (activo bajo, `FLOAT_FAULT_ACTIVE_HIGH 0`). No uses el mismo contacto para el lazo de 12 V y para el Arduino. Si en cambio cableas NC a la entrada con pull-up, configura `FLOAT_FAULT_ACTIVE_HIGH 1`; en ese caso una rotura de cable también se lee como falla.
6. Prueba: baja el flotador bajo → la bomba debe parar. Sube el flotador alto → la bomba debe parar.
7. Prueba de rotura: desconecta un cable de cada flotador → la bomba debe parar. Si no para, el cableado no es de fallo seguro.
8. Limpia los flotadores periódicamente; algas y calcio pueden pegarlos.

## 5. Relé y bomba

> Esta sección asume 12 V DC. Si la bomba resulta de 220 V AC, no la pruebes con este procedimiento: primero rediseña la potencia (§5 de `02_diagrama_conexiones.md`) y haz verificar el montaje por una persona competente.

1. Con la bomba desconectada, alimenta el Arduino 20 veces (reset, programación, encendido/apagado). El relé no debe activarse durante el arranque.
2. Conecta una carga ficticia equivalente (foco de 12 V si es DC). Prueba el programa de riego y las fallas.
3. Mide la corriente de la bomba con pinza DC (o AC si es de red). Compara con la placa. Ajusta fusible, diodo y cable.
4. Verifica el diodo flyback: cátodo al +, ánodo al −. Si está al revés, hace cortocircuito.
5. Simula relé pegado: fuerza el relé principal a ON y abre el flotador bajo. La bomba debe parar por el relé de seguridad. Si no para, el lazo no es independiente.
6. Verifica que el relé de seguridad esté energizado solo cuando ambos flotadores están cerrados. Si la bobina se queda sin tensión, la bomba debe parar.

## 6. Pruebas de sistema

| Prueba | Resultado esperado |
|--------|--------------------|
| Arranque con bomba desconectada | Relé OFF, LCD “INIT”. |
| Nivel bajo simulado | Bomba OFF, alarma, latch. |
| Nivel alto simulado | Bomba OFF, alarma, latch. |
| Rotura de cable de flotador | Bomba OFF. |
| Relé principal pegado | Bomba OFF por relé de seguridad. |
| Corte de energía | Bomba OFF; al volver, no rearranca solo si hay falla. |
| Watchdog | Reinicio, relé OFF, aviso “WDT”. |
| Sonda DS18B20 desconectada | “TEMP?”, EC compensada a 25 °C. |
| Sonda EC desconectada | “EC?” o lectura fuera de rango; bomba sigue. |
| Bomba con caudal bloqueado | Flow switch o tiempo máximo ON → falla. |
