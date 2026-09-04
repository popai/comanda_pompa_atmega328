/*
 * Copyright (c) 2026 Popa Ionel, TWM
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * @brief Masina de stari a pompei: cand porneste, cand se opreste, cand refuza
 *        sa mai porneasca.
 */

#ifndef PUMP_H_
#define PUMP_H_

#include <stdint.h>
#include <stdbool.h>

/** @brief Starea controlerului. */
enum pump_state
{
	/** Bazin suficient de plin, releu declansat. */
	POMPA_OPRITA,
	/** Se umple, releu anclansat. */
	POMPA_PORNITA,
	/** Prea multe citiri esuate: releu declansat, se auto-repara. */
	AVARIE_SENZOR,
	/** Timp maxim de umplere depasit: releu declansat, iese doar la reset. */
	AVARIE_TIMP,
};

/** @brief Configureaza pinul releului in stare inactiva si initializeaza starea. */
void pump_init(void);

/**
 * @brief Un pas al masinii de stari; se apeleaza o data per ciclu de masurare.
 *
 * @param nivel_pct Nivelul de umplere 0..100. Ignorat daca valid == false.
 * @param valid true daca masuratoarea a reusit.
 */
void pump_update(uint8_t nivel_pct, bool valid);

/** @brief Starea curenta. */
enum pump_state pump_get_state(void);

/** @brief Eticheta textuala a starii, pentru log. */
const char *pump_state_str(enum pump_state st);

#endif /* PUMP_H_ */
