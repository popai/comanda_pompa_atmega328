/*
 * Copyright (c) 2026 Popa Ionel, TWM
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * @brief Senzor ultrasonic A0121A4 pe RS485, Modbus RTU 9600 8N1.
 *
 * Protocolul este preluat neschimbat din implementarea Zephyr validata pe
 * hardware (senzor_umplere/src/dist_sensor.c): aceeasi comanda, acelasi CRC,
 * aceeasi formula de conversie.
 */

#ifndef DIST_SENSOR_H_
#define DIST_SENSOR_H_

#include <stdint.h>

/** @brief Rezultatul unei masuratori. */
enum dist_status
{
	/** Cadru valid, CRC corect, valoare in plaja acceptata. */
	DIST_OK,
	/** Senzorul nu a raspuns in timpul alocat, dupa toate reincercarile. */
	DIST_ERR_NO_REPLY,
	/** Cadru primit, dar CRC gresit sau functie Modbus neasteptata. */
	DIST_ERR_CRC,
	/** Senzorul a raportat "in afara plajei" sau valoarea e implauzibila. */
	DIST_ERR_RANGE,
};

/** @brief Porneste portul serial software si pune RS485-ul pe receptie. */
void dist_sensor_init(void);

/**
 * @brief Interogheaza senzorul si decodeaza raspunsul.
 *
 * Blocheaza cel mult DIST_RETRIES * DIST_TIMEOUT_MS. In cazul bun raspunsul
 * vine in ~100 ms si functia se intoarce imediat.
 *
 * @param[out] cm Distanta in centimetri. Scrisa doar la DIST_OK.
 * @return Statusul masuratorii.
 */
enum dist_status dist_sensor_measure(uint16_t *cm);

/** @brief Eticheta textuala a unui status, pentru log. */
const char *dist_status_detail(enum dist_status st);

#endif /* DIST_SENSOR_H_ */
