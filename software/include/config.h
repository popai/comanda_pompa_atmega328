/*
 * Copyright (c) 2026 Popa Ionel, TWM
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * @brief Singurul loc de reglat: pini, cotele bazinului, praguri, timpi.
 */

#ifndef CONFIG_H_
#define CONFIG_H_

#include <Arduino.h>

/* ------------------------------------------------------------------ Pini ---
 *
 * Maparea urmeaza cablajul placii custom (ATmega328PU, 16 MHz). In paranteza
 * portul AVR, ca sa poata fi verificata direct pe schema.
 */

/* LED 1..7, de la nivelul cel mai mic la cel mai mare. */
static const uint8_t LED_PINS[] = {
	A3, /* PC3 - LED 1 */
	A2, /* PC2 - LED 2 */
	13, /* PB5 - LED 3 */
	12, /* PB4 - LED 4 */
	11, /* PB3 - LED 5 */
	10, /* PB2 - LED 6 */
	9,	/* PB1 - LED 7 */
};

#define LED_COUNT ((uint8_t)(sizeof(LED_PINS) / sizeof(LED_PINS[0])))

/* Releul pompei. Activ pe HIGH: la reset pinul este intrare cu pull-up dezactivat,
 * deci releul ramane declansat pana cand setup() decide altfel.
 */
static const uint8_t PIN_RELEU = A4; /* PC4 */
static const uint8_t RELEU_ACTIV = HIGH;
static const uint8_t RELEU_INACTIV = LOW;

/* RS485: DE si /RE sunt legate impreuna pe acelasi pin - HIGH = emisie,
 * LOW = receptie.
 */
static const uint8_t PIN_RS485_DE = A5; /* PC5 */
static const uint8_t PIN_RS485_RX = 5;	/* PD5 <- RO */
static const uint8_t PIN_RS485_TX = 6;	/* PD6 -> DI */

/* -------------------------------------------------- Geometria bazinului ---
 *
 * DE CALIBRAT LA MONTAJ. Senzorul priveste in jos, deci distanta scade pe
 * masura ce bazinul se umple.
 */

/** Distanta senzor -> suprafata apei cu bazinul plin (cm). */
static const uint16_t DIST_PLIN_CM = 20;

/** Distanta senzor -> fundul bazinului, adica bazin gol (cm). */
static const uint16_t DIST_GOL_CM = 150;

/* ------------------------------------------------------ Praguri pompa ---- */

/** Sub acest nivel pompa porneste ("mai putin de jumatate plin"). */
static const uint8_t PRAG_PORNIRE_PCT = 50;

/** Peste acest nivel pompa se opreste ("plin").
 *
 * 95 si nu 100: ultimii centimetri intra in zona oarba a senzorului, iar
 * valurile facute chiar de jetul pompei fac citirea instabila acolo.
 */
static const uint8_t PRAG_OPRIRE_PCT = 95;

/** Citiri valide consecutive care confirma o tranzitie de stare.
 *
 * Impreuna cu filtrul median, impiedica o singura citire aberanta sa comande
 * releul.
 */
static const uint8_t CONFIRMARI_SCHIMBARE = 3;

/* --------------------------------------------------------------- Senzor ---
 *
 * Valorile de plaja sunt aceleasi ca in implementarea Zephyr validata pe
 * hardware (senzor_umplere/src/dist_sensor.c).
 */

static const uint16_t DIST_MIN_CM = 2;
static const uint16_t DIST_MAX_CM = 400;

/** Reincercari per masuratoare. */
static const uint8_t DIST_RETRIES = 3;

/** Timp de asteptare a cadrului complet, per incercare (ms). */
static const uint16_t DIST_TIMEOUT_MS = 550;

/** Perioada ciclului de masurare (ms). */
static const uint16_t INTERVAL_MASURARE_MS = 1000;

/** Pauza la pornire, inainte de prima interogare (ms).
 *
 * La alimentare senzorul isi emite bannerul text de start si ignora
 * interogarile. Masurat pe hardware: la ~509 ms inca nu raspunde, la ~1077 ms
 * raspunde corect.
 */
static const uint16_t WARMUP_MS = 1500;

/* ------------------------------------------------------------ Protectii ---*/

/** Citiri esuate consecutive dupa care pompa se opreste preventiv. */
static const uint8_t MAX_ESECURI_SENZOR = 5;

/** Citiri valide consecutive care sting avaria de senzor. */
static const uint8_t CONFIRMARI_REVENIRE = 3;

/** Timp maxim de functionare continua a pompei (ms).
 *
 * Depasirea inseamna ca nivelul nu creste desi pompa merge: put secat, vana
 * inchisa sau senzor blocat pe o valoare. Avaria este cu retinere (latch) -
 * iese doar la reset - pentru ca repornirea automata ar arde pompa in gol.
 */
static const uint32_t MAX_TIMP_UMPLERE_MS = 5UL * 60UL * 1000UL;

/* ------------------------------------------------------------------ LED ---*/

/** Zona moarta la pragurile bargraph-ului (puncte procentuale). */
static const uint8_t LED_HIST_PCT = 2;

/** Semiperioada clipirii la avarie de senzor (ms) - 2 Hz. */
static const uint16_t LED_BLINK_SENZOR_MS = 250;

/** Semiperioada alternantei la avarie de timp (ms) - 1 Hz. */
static const uint16_t LED_BLINK_TIMP_MS = 500;

/* ---------------------------------------------------------------- Debug ---*/

/** Viteza portului serial de log (D0/D1, liber pe aceasta placa). */
static const uint32_t SERIAL_BAUD = 115200;

#endif /* CONFIG_H_ */
