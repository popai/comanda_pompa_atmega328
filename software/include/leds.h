/*
 * Copyright (c) 2026 Popa Ionel, TWM
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * @brief Bargraph de 7 LED-uri pentru nivel, cu tipare distincte de avarie.
 */

#ifndef LEDS_H_
#define LEDS_H_

#include <stdint.h>

#include "pump.h"

/** @brief Configureaza pinii ca iesiri si stinge toate LED-urile. */
void leds_init(void);

/**
 * @brief Actualizeaza ce trebuie afisat; se apeleaza dupa fiecare masuratoare.
 *
 * @param nivel_pct Nivelul 0..100.
 * @param st Starea pompei - decide intre bargraph si tiparele de avarie.
 */
void leds_set(uint8_t nivel_pct, enum pump_state st);

/**
 * @brief Conduce clipirea; se apeleaza la fiecare trecere prin loop().
 *
 * Fara delay(): tiparele avanseaza pe millis().
 */
void leds_tick(void);

#endif /* LEDS_H_ */
