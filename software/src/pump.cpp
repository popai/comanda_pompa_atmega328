/*
 * Copyright (c) 2026 Popa Ionel, TWM
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <Arduino.h>

#include "config.h"
#include "pump.h"

static enum pump_state state;

/* Citiri consecutive care sustin tranzitia curenta. Se reseteaza la orice
 * citire care o contrazice, deci un varf izolat nu comanda releul.
 */
static uint8_t confirmari;

/* Citiri esuate, respectiv reusite, consecutive - pentru intrarea in si iesirea
 * din AVARIE_SENZOR.
 */
static uint8_t esecuri;
static uint8_t reveniri;

static uint32_t t_pornire;

/* Fereastra filtrului median. Elimina citirea singulara aberanta (reflexie
 * parazita, val) fara sa intarzie raspunsul ca o medie alunecatoare.
 */
static uint8_t hist[3];
static bool hist_plina;

const char *pump_state_str(enum pump_state st)
{
	switch (st)
	{
	case POMPA_OPRITA:
		return "OPRITA";
	case POMPA_PORNITA:
		return "PORNITA";
	case AVARIE_SENZOR:
		return "AVARIE_SENZOR";
	case AVARIE_TIMP:
		return "AVARIE_TIMP";
	default:
		return "?";
	}
}

static void releu_set(bool pornit)
{
	digitalWrite(PIN_RELEU, pornit ? RELEU_ACTIV : RELEU_INACTIV);
}

static uint8_t median3(uint8_t nivel)
{
	uint8_t a, b, c;

	if (!hist_plina)
	{
		/* Prima citire valida dupa pornire sau dupa o avarie: umplem fereastra
		 * cu ea, ca sa nu introducem o valoare inventata in mediana.
		 */
		hist[0] = hist[1] = hist[2] = nivel;
		hist_plina = true;
	}
	else
	{
		hist[0] = hist[1];
		hist[1] = hist[2];
		hist[2] = nivel;
	}

	a = hist[0];
	b = hist[1];
	c = hist[2];

	if (a > b)
	{
		uint8_t t = a;
		a = b;
		b = t;
	}
	if (b > c)
	{
		b = c;
	}
	return (a > b) ? a : b;
}

void pump_init(void)
{
	/* Nivelul se scrie inaintea lui pinMode(): altfel pinul ar trece printr-o
	 * stare de iesire cu ultimul continut al registrului PORT, iar pe un releu
	 * activ pe HIGH asta inseamna pompa pornita pentru cateva microsecunde la
	 * fiecare reset.
	 */
	digitalWrite(PIN_RELEU, RELEU_INACTIV);
	pinMode(PIN_RELEU, OUTPUT);
	releu_set(false);

	state = POMPA_OPRITA;
	confirmari = 0;
	esecuri = 0;
	reveniri = 0;
	hist_plina = false;
}

enum pump_state pump_get_state(void)
{
	return state;
}

void pump_update(uint8_t nivel_pct, bool valid)
{
	uint8_t nivel;

	if (!valid)
	{
		reveniri = 0;
		confirmari = 0;

		if (esecuri < 255)
		{
			esecuri++;
		}

		/* AVARIE_TIMP are prioritate: e cu retinere si nu vrem sa fie stearsa
		 * de o avarie care se auto-repara.
		 */
		if (esecuri >= MAX_ESECURI_SENZOR && state != AVARIE_TIMP)
		{
			if (state != AVARIE_SENZOR)
			{
				releu_set(false);
				state = AVARIE_SENZOR;
				hist_plina = false;
			}
		}
		return;
	}

	esecuri = 0;
	nivel = median3(nivel_pct);

	switch (state)
	{
	case AVARIE_TIMP:
		/* Cu retinere: put secat sau senzor blocat. Repornirea automata ar
		 * lasa pompa sa mearga in gol la nesfarsit.
		 */
		releu_set(false);
		break;

	case AVARIE_SENZOR:
		if (++reveniri >= CONFIRMARI_REVENIRE)
		{
			/* Revenim in OPRITA, nu direct in PORNITA: pornirea se
			 * re-evalueaza pe drumul normal, cu confirmari.
			 */
			state = POMPA_OPRITA;
			confirmari = 0;
			reveniri = 0;
		}
		break;

	case POMPA_OPRITA:
		if (nivel <= PRAG_PORNIRE_PCT)
		{
			if (++confirmari >= CONFIRMARI_SCHIMBARE)
			{
				releu_set(true);
				state = POMPA_PORNITA;
				t_pornire = millis();
				confirmari = 0;
			}
		}
		else
		{
			confirmari = 0;
		}
		break;

	case POMPA_PORNITA:
		if (millis() - t_pornire > MAX_TIMP_UMPLERE_MS)
		{
			releu_set(false);
			state = AVARIE_TIMP;
			confirmari = 0;
			break;
		}

		if (nivel >= PRAG_OPRIRE_PCT)
		{
			if (++confirmari >= CONFIRMARI_SCHIMBARE)
			{
				releu_set(false);
				state = POMPA_OPRITA;
				confirmari = 0;
			}
		}
		else
		{
			confirmari = 0;
		}
		break;
	}
}
