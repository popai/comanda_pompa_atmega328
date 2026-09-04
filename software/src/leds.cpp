/*
 * Copyright (c) 2026 Popa Ionel, TWM
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <Arduino.h>

#include "config.h"
#include "leds.h"

/* Cate LED-uri sunt aprinse in bargraph, 0..LED_COUNT. Se pastreaza intre
 * apeluri pentru histerezis.
 */
static uint8_t nr_aprinse;

static enum pump_state stare_afisata = POMPA_OPRITA;

static uint32_t t_blink;
static bool faza_blink;

/**
 * @brief Nivelul minim (%) la care LED-ul k (1..LED_COUNT) este aprins.
 *
 * Pragurile sunt asezate la mijlocul fiecarei trepte, nu la capat: cu 7 LED-uri
 * si 100%, primul se aprinde la ~7%, iar ultimul la ~92%. Un bazin aproape gol
 * arata deci "un LED", nu "zero", ceea ce distinge vizual nivelul mic de
 * afisajul stins.
 */
static uint8_t prag_led(uint8_t k)
{
	return (uint8_t)(((uint16_t)k * 100u - 50u) / LED_COUNT);
}

static void scrie_bargraph(uint8_t n)
{
	for (uint8_t i = 0; i < LED_COUNT; i++)
	{
		digitalWrite(LED_PINS[i], (i < n) ? HIGH : LOW);
	}
}

static void scrie_toate(bool aprins)
{
	for (uint8_t i = 0; i < LED_COUNT; i++)
	{
		digitalWrite(LED_PINS[i], aprins ? HIGH : LOW);
	}
}

/** @brief Aprinde LED-urile impare sau pare - tipar imposibil de confundat cu
 *         un bargraph real.
 */
static void scrie_alternant(bool impare)
{
	for (uint8_t i = 0; i < LED_COUNT; i++)
	{
		bool impar = (i % 2) == 0; /* i==0 este LED 1 */
		digitalWrite(LED_PINS[i], (impar == impare) ? HIGH : LOW);
	}
}

void leds_init(void)
{
	for (uint8_t i = 0; i < LED_COUNT; i++)
	{
		digitalWrite(LED_PINS[i], LOW);
		pinMode(LED_PINS[i], OUTPUT);
	}

	nr_aprinse = 0;
	t_blink = millis();
	faza_blink = false;
}

void leds_set(uint8_t nivel_pct, enum pump_state st)
{
	stare_afisata = st;

	/* Histerezis pe fiecare treapta: fara el, LED-ul de varf palpaie continuu
	 * cand nivelul sta exact pe un prag, iar apa nu sta niciodata perfect
	 * nemiscata.
	 */
	while (nr_aprinse < LED_COUNT &&
		   (int16_t)nivel_pct >= (int16_t)prag_led(nr_aprinse + 1) + LED_HIST_PCT)
	{
		nr_aprinse++;
	}

	while (nr_aprinse > 0 &&
		   (int16_t)nivel_pct < (int16_t)prag_led(nr_aprinse) - LED_HIST_PCT)
	{
		nr_aprinse--;
	}
}

void leds_tick(void)
{
	uint32_t now = millis();

	switch (stare_afisata)
	{
	case AVARIE_SENZOR:
		if (now - t_blink >= LED_BLINK_SENZOR_MS)
		{
			t_blink = now;
			faza_blink = !faza_blink;
			scrie_toate(faza_blink);
		}
		break;

	case AVARIE_TIMP:
		if (now - t_blink >= LED_BLINK_TIMP_MS)
		{
			t_blink = now;
			faza_blink = !faza_blink;
			scrie_alternant(faza_blink);
		}
		break;

	default:
		/* Bargraph static: scrierea la fiecare trecere e ieftina si repara
		 * afisajul dupa iesirea dintr-un tipar de avarie.
		 */
		scrie_bargraph(nr_aprinse);
		t_blink = now;
		faza_blink = false;
		break;
	}
}
