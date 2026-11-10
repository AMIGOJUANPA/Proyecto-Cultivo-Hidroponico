# 02 — Diagrama de conexiones

## 1. Dominio de señal (SELV 5 V)

```text
                         +-------------------------------+
                         |        ARDUINO MEGA 2560      |
                         |                               |
  [HC-SR04]   TRIG ----->| D30                           |
              ECHO <-----| D31  (1k serie opcional)      |
                         |                               |
  [DS18B20]   DATA <---->| D4   (pull-up 4.7k a 5V)      |
                         |      (vía módulo TDS)         |
                         |                               |
  [Módulo relé] IN <-----| D5   (pull-up 10k a 5V)       |
                         |                               |
  [High-side EC] <-------| D6                            |
  [Buzzer] <-------------| D7                            |
  [LED alarma] <---------| D8   (330 ohm a GND)          |
  [Flotador bajo] ------>| D9   (pull-up 10k, NC)        |
  [Flotador alto] ------>| D10  (pull-up 10k, NC)        |
                         |                               |
  [LCD I2C] SDA <------->| D20                           |
            SCL <------->| D21                           |
                         |                               |
  [TDS OUT] ------------>| A0   (1k serie + 100nF a GND) |
  [Reset] -------------->| D22  (a GND, pull-up interno) |
  [Flow switch] -------->| D23  (opcional, pull-up)      |
                         +-------------------------------+
```

Rieles de señal:

```text
 [Fuente 5V 2A] ---+--- riel 5V ---> Arduino 5V, LCD, HC-SR04, DS18B20, TDS, relé VCC
                    |
                    +--- riel GND ---> Arduino GND, LCD GND, sensores GND, relé GND
                    |
                    +--- masa en estrella en el negativo de la fuente de 5V
```

Notas:

- El HC-SR04 va a 5 V. ECHO entrega 5 V; el Mega tolera 5 V. La resistencia de 1 kΩ en serie limita corriente ante transitorios, no divide la señal de forma apreciable.
- El DS18B20 lleva pull-up de 4,7 kΩ entre DATA y 5 V.
- **D4 compartido:** según el montaje actual, D4 va al módulo TDS y de ahí a la DS18B20. Antes de conectarlo, confirma que el módulo no tenga su propio maestro 1-Wire ni una pull-up distinta que contienda con la del Arduino. Si hay duda, deja la sonda del módulo para su compensación interna y conecta una **segunda DS18B20 directa a D4**. El firmware no cambia.
- El módulo de relé JQC-3FF-S-Z se alimenta a 5 V. Si tiene jumper JD-VCC, retíralo y alimenta JD-VCC desde el riel de 5 V; verifica si la masa del opto está realmente separada. Confirma además que el contacto NO/COM conmuta **la bomba** y no el módulo TDS.
- El LED de alarma va con 330 Ω a GND. El buzzer activo de 5 V puede ir directo si consume menos de 20 mA; si no, transistor.
- Los flotadores son contactos secos. El contacto **NC** va en serie con la bobina del relé de seguridad (fallo seguro ante rotura). El contacto **NO** va a GND y a la entrada del Arduino con pull-up interno: al activarse el flotador, la entrada pasa a LOW (`FLOAT_FAULT_ACTIVE_HIGH 0`). No compartas un mismo contacto entre el lazo de 12 V y la entrada de 5 V.
- El TDS Meter V1.0 se alimenta a 5 V (confirma la tensión en la placa; no asumas que “corriente del toma” significa 220 V). Su salida va a A0 con 1 kΩ serie y 100 nF a GND. Si usas la referencia interna de 2,56 V, verifica que la salida nunca supere ese valor.
- **“Se conecta a la corriente del toma”** describe la alimentación del módulo, no la señal que excita la sonda. Para determinar si la sonda recibe AC o DC hay que medir entre sus electrodos con el módulo encendido (DC, AC y, si es posible, osciloscopio). Si es DC continua, la sonda se polariza y se corroe.
- El high-side switch de D6 es un P-MOSFET o un load switch. No uses un MOSFET N en el negativo del módulo: la salida analógica quedaría flotante y podría alimentarse a través de A0.

## 2. Dominio de potencia 12 V DC (solo si la bomba es de 12 V DC)

> **Condición de validez:** este diagrama asume que la bomba es de **12 V DC**. La tensión real aún no está confirmada; “se conecta a la corriente del toma” puede significar una toma de 12 V o un enchufe de 220 V AC. **Si la bomba es de 220 V AC, no uses esta sección:** pasa a §5.

```text
============================================================
 POTENCIA 12 V DC — línea gruesa
============================================================

 [Fuente 12V] --+-- F1 --+-- [Relé seguridad COM/NO] --+-- [Relé principal COM/NO] --+-- Bomba +
                |        |                              |                              |
                |        |                              |                              +-- Bomba − -- 0V
                |        |                              |
                |        |                              +-- D1 flyback: cátodo a +, ánodo a −
                |        |                              +-- C1 100nF / 100V
                |        |
                |        +-- [Bobina relé seguridad] <-- flotador bajo NC <-- flotador alto NC <-- +12V
                |                     |
                |                     +-- D2 flyback de la bobina: cátodo a +, ánodo a −
                |
                +-- 0V común de potencia (estrella con el 0V de la fuente de 5V)
```

Conexión recomendada, en orden:

1. **+12 V → F1 (fusible de la bomba) → contacto del relé de seguridad → contacto del relé principal → bomba + → bomba − → 0 V.**
2. **Diodo D1 en paralelo con la bomba:** cátodo al +, ánodo al −. Sin este diodo, la inductancia de la bomba genera un pico que erosiona los contactos y puede destruir el relé.
3. **C1 de 100 nF / 100 V en paralelo con la bomba:** reduce el ruido de escobillas.
4. **Bobina del relé de seguridad:** +12 V → fusible F2 → flotador bajo NC → flotador alto NC → bobina → 0 V. Diodo D2 en paralelo con la bobina.
5. **Fusible F3 de 0,5 A en la línea de 5 V de control.**

## 3. Lógica del lazo de seguridad

```text
Flotador bajo (reservorio):
  agua por encima del mínimo → contacto NC cerrado
  agua por debajo del mínimo → contacto NC abierto → relé de seguridad OFF → bomba OFF

Flotador alto (mesa/canal):
  nivel normal → contacto NC cerrado
  nivel alto → contacto NC abierto → relé de seguridad OFF → bomba OFF
```

Si un cable se rompe, el lazo se abre y la bomba se detiene. Eso es **fallo seguro**.

Si no se instalan flotadores, el sistema queda sin protección independiente. El ultrasónico puede detectar nivel bajo en el reservorio, pero no el desbordamiento de la mesa. En ese caso, el firmware funciona en modo prototipo y muestra una advertencia; no debe operar sin supervisión.

## 4. Separación de masas

> Válido para el escenario de 12 V DC. Si hay 220 V AC, la separación obligatoria es **red–SELV** (optoacoplador o relé intermedio) y la tierra de protección es independiente de la masa de señal.

| Masa | Unión |
|------|-------|
| Masa de señal (5 V) | Arduino GND, LCD, HC-SR04, DS18B20, TDS, entrada del módulo relé. |
| Masa de potencia (12 V) | Negativo de la bomba, bobina del relé de seguridad, fuente de 12 V. |
| Punto de estrella | Negativo de la fuente de 5 V y negativo de la fuente de 12 V, si comparten referencia. |
| Tierra de protección | Solo si la fuente de 12 V o la caja metálica lo requieren. No es la masa de señal. |

La corriente de la bomba no debe pasar por las pistas del Arduino ni por el cable de GND de los sensores. Usa cables separados y de sección adecuada.

## 5. Advertencia sobre 220 V

La bomba se declaró como 12 V DC, pero eso **no está confirmado**. Si se alimenta de un enchufe de 220 V AC, el proyecto deja de ser SELV en la etapa de potencia y cambia por completo:

| Elemento | 12 V DC (si se confirma) | 220 V AC (si se confirma) |
|----------|--------------------------|---------------------------|
| Conmutación | Módulo relé JQC-3FF-S-Z + relé de seguridad de 12 V | Contactor o relé de potencia con contacto rated para motor a 220 V AC; bobina de 220 V o 12 V según diseño |
| Protección | Fusible DC | Fusible/breaker AC + **RCD/GFCI** + puesta a tierra de protección |
| Montaje | Borneras, cables de 1 mm², caja IP54 | Caja cerrada, prensaestopas, cable de red de sección adecuada, borneras, **nunca protoboard** |
| Aislamiento | Separación de masas de señal y potencia | Separación física entre red y SELV; optoacoplador o relé intermedio |
| Verificación | Multímetro y pinza DC | Persona competente, medición de aislamiento y tierra |

Reglas mínimas si hay 220 V:

1. **No prototipar 220 V en protoboard ni en cables sueltos.**
2. Usar un contactor/relé para cargas inductivas, no el módulo JQC de prototipo, y dimensionarlo por la **corriente de arranque** del motor.
3. Instalar RCD/GFCI en la toma y puesta a tierra de protección; la tierra no es la masa de señal.
4. Mantener la red y el agua en compartimentos separados, con alivio de tensión y prensaestopas.
5. El Arduino solo debe gobernar la bobina de baja tensión mediante un módulo optoacoplado o un relé intermedio; nunca llevar 220 V a la protoboard.
6. Hacer verificar el montaje por una persona competente antes de energizar.

Mientras la tensión no esté confirmada, **no compres la fuente, el relé de potencia ni el cable** y no conectes la bomba. La única parte que podría ir a la red en el escenario de 12 V DC son los adaptadores comerciales certificados; no los abras ni sustituyas su cable de red.

