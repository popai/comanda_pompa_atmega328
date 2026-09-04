# comanda_pompa — controler de umplere bazin

Firmware pentru o placă custom cu **ATmega328PU la 16 MHz** care umple automat un
bazin: comandă releul unei pompe în funcție de nivelul citit cu un senzor
ultrasonic pe RS485 și afișează gradul de umplere pe un bargraph de 7 LED-uri.

Senzorul (**A0121A4**, Modbus RTU) este montat deasupra bazinului și privește în
jos, deci distanța măsurată **scade** pe măsură ce bazinul se umple.

## Logica de funcționare

| Condiție | Acțiune |
|---|---|
| Nivel ≤ 50% (bazin gol sau sub jumătate) | Pornește pompa |
| Nivel ≥ 95% (bazin plin) | Oprește pompa |
| Între 50% și 95% | Păstrează starea curentă |

Banda 50%…95% **este** histerezisul: odată pornită, pompa merge până la plin,
deci nu poate clipi în jurul niciunui prag. Peste asta, un filtru median pe 3
citiri și confirmarea pe 3 citiri consecutive împiedică o singură valoare
aberantă (val, reflexie parazită) să comande releul.

### Protecții

- **Timp maxim de umplere** — 5 de minute de funcționare continuă fără să se
  atingă „plin" înseamnă puț secat, vană închisă sau senzor blocat pe o valoare.
  Pompa se oprește și intră în `AVARIE_TIMP`, stare **cu reținere**: iese doar la
  reset, pentru că repornirea automată ar ține pompa în gol la nesfârșit.
- **Senzor defect** — 5 citiri eșuate consecutive (fără răspuns, CRC greșit, în
  afara plajei) opresc pompa preventiv și trec în `AVARIE_SENZOR`. Starea se
  auto-repară după 3 citiri valide consecutive.
- Releul **nu pornește niciodată** pe baza unei citiri invalide.
- La reset pinul releului este scris `LOW` înainte de `pinMode(OUTPUT)`, ca
  pompa să nu pornească pentru câteva microsecunde la fiecare repornire.
- Watchdog de 8 s, resetat **doar la încheierea unui ciclu de măsurare**, nu la
  fiecare trecere prin `loop()`. Astfel el nu cere doar ca procesorul să fie viu,
  ci ca măsurătorile să avanseze efectiv: dacă bucla s-ar învârti fără să mai
  măsoare (de exemplu cu `millis()` înghețat), watchdog-ul resetează placa și
  pompa se oprește.

## Hardware

### Pinout

| Funcție | Port AVR | Pin Arduino |
|---|---|---|
| LED 1 | PC3 | A3 |
| LED 2 | PC2 | A2 |
| LED 3 | PB5 | 13 |
| LED 4 | PB4 | 12 |
| LED 5 | PB3 | 11 |
| LED 6 | PB2 | 10 |
| LED 7 | PB1 | 9 |
| Releu pompă (activ **HIGH**) | PC4 | A4 |
| RS485 DE + /RE (legate împreună) | PC5 | A5 |
| RS485 RO → RX software | PD5 | 5 |
| RS485 DI ← TX software | PD6 | 6 |

D0/D1 rămân libere pentru upload și pentru logul de debug.
PC4/PC5 sunt și SDA/SCL, dar proiectul nu folosește I2C — fără conflict.

### Senzorul

MAX485 în half-duplex: `DE` și `/RE` sunt legate la același pin, `HIGH` = emisie,
`LOW` = recepție. Firmware-ul ridică linia doar cât durează interogarea.

Comunicația se face pe `SoftwareSerial` la 9600 8N1, protocol Modbus RTU:

- comandă: `01 03 01 01 00 01 D4 36`
- răspuns: 7 octeți — `adresă, funcție, lungime, dataHi, dataLo, crcLo, crcHi`
- valoarea este în zecimi de milimetru; `(dataHi * 256 + dataLo) / 10` = cm
- `dataHi == 0xFF` înseamnă „în afara plajei"

Protocolul, CRC-ul și filtrul de sincronizare sunt preluate neschimbat din
implementarea Zephyr validată pe hardware
(`ncs_app/senzor_umplere/src/dist_sensor.c`). Filtrul aruncă octeți până
întâlnește `0x01` urmat de `0x03`, ceea ce este necesar pentru că la alimentare
senzorul emite un banner text (`APP Run...`) lung exact cât un cadru Modbus.

## Calibrare

**Obligatoriu la montaj.** În [`include/config.h`](include/config.h):

```c
static const uint16_t DIST_PLIN_CM = 20;   // senzor -> suprafața apei, bazin plin
static const uint16_t DIST_GOL_CM  = 150;  // senzor -> fundul bazinului
```

Procedura: montează placa pe bazin, deschide monitorul serial, citește distanța
raportată cu bazinul gol și cu bazinul plin, pune cele două valori aici și
recompilează. Nivelul se calculează liniar între ele, saturat la 0 și 100%.

Restul parametrilor reglabili — praguri, numărul de confirmări, timeout-uri,
timpul maxim de umplere — sunt tot acolo. `config.h` este singurul fișier care
trebuie atins pentru reglaje; în restul codului nu există numere magice.

## Indicații pe LED-uri

**Funcționare normală** — bargraph, LED-urile 1..7 de la nivel mic la nivel mare.
Pragurile sunt așezate la mijlocul fiecărei trepte (≈7, 21, 35, 50, 64, 78, 92%),
cu o zonă moartă de ±2%, ca LED-ul de vârf să nu pâlpâie când nivelul stă exact
pe un prag. Un bazin aproape gol arată „un LED", nu zero — asta distinge vizual
nivelul mic de afișajul stins.

**Avarie** — două tipare imposibil de confundat cu un bargraph real:

| Tipar | Semnificație |
|---|---|
| Toate 7 clipind sincron, 2 Hz | `AVARIE_SENZOR` — senzorul nu răspunde |
| Alternanță 1-3-5-7 / 2-4-6, 1 Hz | `AVARIE_TIMP` — timp maxim depășit, cere reset |

## Compilare și încărcare

Proiect [PlatformIO](https://platformio.org/), env `uno` (ATmega328P la 16 MHz,
bootloader Arduino):

```sh
pio run                 # compilare
pio run -t upload       # încărcare
pio device monitor      # log de debug, 115200 baud
```

Binarul rezultă la `.pio/build/uno/comanda_pompa_uno.hex` — numele este compus
din numele proiectului și al plăcii de către [`extra_script.py`](extra_script.py).

### Ieșirea pe serial

Câte o linie la fiecare ciclu de măsurare (1 s):

```
=== Controler umplere bazin ===
Plin la 20 cm, gol la 150 cm; pornire sub 50%, oprire peste 95%
98 cm | 40% | OPRITA
97 cm | 40% | OPRITA
96 cm | 41% | PORNITA
--- | senzor: fara raspuns | PORNITA
```

## Structura codului

| Fișier | Rol |
|---|---|
| [`include/config.h`](include/config.h) | Pini, cotele bazinului, praguri, timpi — singurul loc de reglat |
| [`src/dist_sensor.cpp`](src/dist_sensor.cpp) | Driver Modbus RTU pe RS485: CRC, sincronizare cadru, reîncercări |
| [`src/pump.cpp`](src/pump.cpp) | Mașina de stări, filtrul median, comanda releului |
| [`src/leds.cpp`](src/leds.cpp) | Bargraph cu histerezis și tiparele de avarie |
| [`src/main.cpp`](src/main.cpp) | `setup()`/`loop()`, watchdog, conversia distanță → procent |

Bucla principală este non-blocantă (`millis()`, fără `delay()`), cu excepția
ciclului de măsurare: cel mult 3 × 550 ms, iar în cazul bun răspunsul vine în
~100 ms.

## Punere în funcțiune

1. `pio run` — compilare curată, fără warning-uri.
2. Alimentează placa **fără senzor conectat** și urmărește monitorul: după 5
   citiri eșuate trebuie să intre în `AVARIE_SENZOR`, cu releul oprit și toate
   LED-urile clipind. Confirmă protecția de senzor defect.
3. Conectează senzorul pe masă, apropie și depărtează o placă plană și verifică
   în log că distanța corespunde realității și că bargraph-ul urcă și coboară.
4. Verifică tranzițiile: releul anclanșează sub 50%, declanșează la 95% și **nu**
   comută între aceste valori.
5. Pentru testul de timp maxim, coboară temporar `MAX_TIMP_UMPLERE_MS` la 30 s,
   ține nivelul sub 50% și confirmă trecerea în `AVARIE_TIMP`.
6. Montează pe bazin, calibrează cele două cote și urmărește un ciclu complet de
   umplere.
