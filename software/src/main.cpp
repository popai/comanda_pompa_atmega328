/*
 * Copyright (c) 2026 Popa Ionel, TWM
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Controler de umplere bazin pe ATmega328PU (16 MHz):
 *   - senzor ultrasonic A0121A4 pe RS485/Modbus RTU, montat deasupra bazinului;
 *   - releu de pompa, activ pe HIGH;
 *   - bargraph de 7 LED-uri pentru nivel.
 *
 * Logica: sub jumatate porneste pompa, plin o opreste. Peste asta, trei
 * protectii: histerezis, timp maxim de umplere si oprire la senzor defect.
 */

#include <Arduino.h>
#include <avr/wdt.h>

#include "config.h"
#include "dist_sensor.h"
#include "leds.h"
#include "pump.h"

static uint32_t t_ultima_masuratoare;

/**
 * @brief Converteste distanta masurata in grad de umplere.
 *
 * Senzorul priveste in jos, deci distanta scade cand bazinul se umple.
 * Saturarea la 0 si 100 acopera si cazurile in care apa depaseste cota "plin"
 * sau senzorul vede dincolo de cota "gol".
 */
static uint8_t dist_la_procent(uint16_t cm)
{
	if (cm <= DIST_PLIN_CM)
	{
		return 100;
	}
	if (cm >= DIST_GOL_CM)
	{
		return 0;
	}

	return (uint8_t)(((uint32_t)(DIST_GOL_CM - cm) * 100UL) /
					 (uint32_t)(DIST_GOL_CM - DIST_PLIN_CM));
}

void setup()
{
	/* Releul primul: intre reset si aici pinul este intrare, deci pompa e
	 * oprita; pump_init() o tine oprita si dupa ce pinul devine iesire.
	 */
	pump_init();
	leds_init();

	Serial.begin(SERIAL_BAUD);
	Serial.println();
	Serial.println(F("=== Controler umplere bazin ==="));
	Serial.print(F("Plin la "));
	Serial.print(DIST_PLIN_CM);
	Serial.print(F(" cm, gol la "));
	Serial.print(DIST_GOL_CM);
	Serial.print(F(" cm; pornire sub "));
	Serial.print(PRAG_PORNIRE_PCT);
	Serial.print(F("%, oprire peste "));
	Serial.print(PRAG_OPRIRE_PCT);
	Serial.println(F("%"));

	dist_sensor_init();

	/* Senzorul isi emite bannerul de pornire si ignora interogarile pana la
	 * ~1 s de la alimentare. Aici inca nu are ce reseta watchdog-ul, deci
	 * asteptarea se face inaintea activarii lui.
	 */
	delay(WARMUP_MS);

	/* Un ciclu de masurare dureaza cel mult DIST_RETRIES * DIST_TIMEOUT_MS
	 * (~1,7 s), deci 8 s lasa marja larga. Bootloader-ul optiboot al placii
	 * trateaza corect resetul de watchdog.
	 */
	wdt_enable(WDTO_8S);

	/* Prima masuratoare imediat, nu dupa un interval. */
	t_ultima_masuratoare = millis() - INTERVAL_MASURARE_MS;
}

void loop()
{
	leds_tick();

	if (millis() - t_ultima_masuratoare < INTERVAL_MASURARE_MS)
	{
		return;
	}
	t_ultima_masuratoare = millis();

	/* Ultimul nivel valid: la o citire esuata bargraph-ul il pastreaza, in loc
	 * sa cada la zero si sa sugereze un bazin gol care nu exista. Daca
	 * esecurile continua, pump_update() intra oricum in AVARIE_SENZOR si
	 * afisajul trece pe tiparul de eroare.
	 */
	static uint8_t ultimul_nivel;

	uint16_t cm = 0;
	enum dist_status st = dist_sensor_measure(&cm);
	bool valid = (st == DIST_OK);
	uint8_t nivel = valid ? dist_la_procent(cm) : ultimul_nivel;

	if (valid)
	{
		ultimul_nivel = nivel;
	}

	pump_update(nivel, valid);
	leds_set(nivel, pump_get_state());

	/* Watchdog-ul se reseteaza abia aici, nu la intrarea in loop().
	 *
	 * loop() este reapelata imediat de main()-ul Arduino, deci un reset pus
	 * inaintea iesirii premature de mai sus l-ar mangaia de mii de ori pe
	 * secunda si ar transforma watchdog-ul intr-un simplu detector de blocaj
	 * al procesorului. Daca insa millis() ingheata - intreruperi ramase
	 * dezactivate - bucla s-ar invarti la nesfarsit fara sa mai masoare, iar
	 * pompa ar ramane pornita definitiv: exact ce trebuie sa prinda watchdog-ul.
	 *
	 * Resetat aici, el cere dovada ca un ciclu complet s-a incheiat. In cel mai
	 * rau caz intre doua resetari trec INTERVAL_MASURARE_MS plus durata maxima
	 * a masuratorii (DIST_RETRIES * DIST_TIMEOUT_MS), adica ~2,7 s - confortabil
	 * sub cele 8 s ale watchdog-ului.
	 */
	wdt_reset();

	if (valid)
	{
		Serial.print(cm);
		Serial.print(F(" cm | "));
		Serial.print(nivel);
		Serial.print(F("% | "));
	}
	else
	{
		Serial.print(F("--- | senzor: "));
		Serial.print(dist_status_detail(st));
		Serial.print(F(" | "));
	}
	Serial.println(pump_state_str(pump_get_state()));
}
