/*
 * Copyright (c) 2026 Popa Ionel, TWM
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Port pe AVR al driverului Modbus din senzor_umplere/src/dist_sensor.c.
 * Protocolul este identic; s-au schimbat doar transportul (SoftwareSerial +
 * MAX485 in loc de UART hardware) si asteptarea cadrului (bucla pe millis() in
 * loc de ISR + semafor, pentru ca SoftwareSerial isi are deja propria receptie
 * pe intreruperi de schimbare de pin).
 */

#include <string.h>

#include <Arduino.h>
#include <SoftwareSerial.h>

#include "config.h"
#include "dist_sensor.h"

/* 1 adresa + 1 functie + 1 lungime + 2 date + 2 CRC */
#define MODBUS_FRAME_LEN 7

/* Adresa de slave si functia asteptate in raspuns, folosite si ca puncte de
 * sincronizare a cadrului.
 */
#define MODBUS_SLAVE_ADDR 0x01
#define MODBUS_FUNC_READ 0x03

/* Citirea registrului de distanta. */
static const uint8_t sz_command[] = {
	0x01, 0x03, 0x01, 0x01, 0x00, 0x01, 0xD4, 0x36};

/* uint8_t, nu char: cadrul este binar, iar 'char' cu semn ar strica atat
 * comparatia cu 0xFF cat si reconstructia CRC-ului.
 */
static uint8_t rx_buf[MODBUS_FRAME_LEN];
static uint8_t rx_buf_pos;

/* Octeti primiti si aruncati de sincronizare. Fara acest contor, "niciun octet
 * primit" nu distinge senzorul mut de senzorul care isi scrie bannerul de start.
 */
static uint16_t rx_ignored;

static SoftwareSerial sz_serial(PIN_RS485_RX, PIN_RS485_TX);

const char *dist_status_detail(enum dist_status st)
{
	switch (st)
	{
	case DIST_OK:
		return "ok";
	case DIST_ERR_NO_REPLY:
		return "fara raspuns";
	case DIST_ERR_CRC:
		return "cadru invalid";
	case DIST_ERR_RANGE:
		return "in afara plajei";
	default:
		return "necunoscut";
	}
}

static uint16_t modrtu_crc(const uint8_t *buf, int len)
{
	uint16_t crc = 0xFFFF;

	for (int pos = 0; pos < len; pos++)
	{
		crc ^= (uint16_t)buf[pos]; /* XOR byte into least sig. byte of crc */

		for (int i = 8; i != 0; i--)
		{ /* Loop over each bit */
			if ((crc & 0x0001) != 0)
			{			   /* If the LSB is set */
				crc >>= 1; /* Shift right and XOR 0xA001 */
				crc ^= 0xA001;
			}
			else
			{
				crc >>= 1; /* Else LSB is not set, just shift right */
			}
		}
	}

	return crc;
}

void dist_sensor_init(void)
{
	/* Receptie implicita: transceiverul nu tine linia ocupata decat cat dureaza
	 * propria interogare.
	 */
	digitalWrite(PIN_RS485_DE, LOW);
	pinMode(PIN_RS485_DE, OUTPUT);

	sz_serial.begin(9600);
	sz_serial.listen();
}

static void request_send(void)
{
	/* Bufferul se goleste imediat inainte de interogare: orice octet ramas de
	 * la bannerul de pornire sau de la incercarea anterioara ar decala cadrul.
	 */
	while (sz_serial.available() > 0)
	{
		(void)sz_serial.read();
		rx_ignored++;
	}

	digitalWrite(PIN_RS485_DE, HIGH);
	sz_serial.write(sz_command, sizeof(sz_command));
	/* SoftwareSerial::write() este bit-bang blocant, deci ultimul bit de stop a
	 * iesit deja cand se intoarce. Marja acopera timpul de propagare prin
	 * transceiver; fara ea, coborarea lui DE poate trunchia ultimul bit.
	 */
	delayMicroseconds(100);
	digitalWrite(PIN_RS485_DE, LOW);
}

/**
 * @brief Colecteaza un cadru, cu sincronizare pe adresa si functie.
 *
 * La alimentare senzorul emite un banner text ("APP Run..."), lung exact cat un
 * cadru Modbus. Fara filtrul de aici el umple bufferul si consuma o incercare
 * intreaga, iar raspunsul real, care vine imediat dupa, se pierde.
 *
 * @return true daca s-au adunat MODBUS_FRAME_LEN octeti in timpul alocat.
 */
static bool frame_collect(void)
{
	uint32_t t0 = millis();

	rx_buf_pos = 0;
	memset(rx_buf, 0, sizeof(rx_buf));

	while (millis() - t0 < DIST_TIMEOUT_MS)
	{
		if (sz_serial.available() <= 0)
		{
			continue;
		}

		uint8_t c = (uint8_t)sz_serial.read();

		if (rx_buf_pos == 0 && c != MODBUS_SLAVE_ADDR)
		{
			rx_ignored++;
			continue;
		}

		if (rx_buf_pos == 1 && c != MODBUS_FUNC_READ)
		{
			/* Octetul poate fi el insusi inceputul unui cadru valid: pastram
			 * sincronizarea in loc sa o pierdem.
			 */
			rx_ignored++;
			rx_buf_pos = (c == MODBUS_SLAVE_ADDR) ? 1 : 0;
			continue;
		}

		rx_buf[rx_buf_pos++] = c;

		if (rx_buf_pos == MODBUS_FRAME_LEN)
		{
			return true;
		}
	}

	return false;
}

/**
 * @brief Decodeaza cadrul primit.
 *
 * @param[out] cm Distanta in centimetri.
 * @return DIST_OK, DIST_ERR_CRC sau DIST_ERR_RANGE.
 */
static enum dist_status frame_decode(uint16_t *cm)
{
	uint16_t crc;
	uint16_t calc_crc;
	uint16_t value;

	/* Adresa si functia sunt deja garantate de sincronizare; verificarea ramane
	 * ca plasa de siguranta daca aceasta se schimba.
	 */
	if (rx_buf[0] != MODBUS_SLAVE_ADDR || rx_buf[1] != MODBUS_FUNC_READ)
	{
		return DIST_ERR_CRC;
	}

	crc = rx_buf[5] | ((uint16_t)rx_buf[6] << 8);
	calc_crc = modrtu_crc(rx_buf, MODBUS_FRAME_LEN - 2);
	if (crc != calc_crc)
	{
		Serial.print(F("  CRC gresit: primit 0x"));
		Serial.print(crc, HEX);
		Serial.print(F(", calculat 0x"));
		Serial.println(calc_crc, HEX);
		return DIST_ERR_CRC;
	}

	/* 0xFF pe octetul superior: senzorul raporteaza "in afara plajei". */
	if (rx_buf[3] == 0xFF)
	{
		return DIST_ERR_RANGE;
	}

	value = ((uint16_t)rx_buf[3] * 256 + rx_buf[4]) / 10; /* transformam in cm */

	if (value < DIST_MIN_CM || value > DIST_MAX_CM)
	{
		Serial.print(F("  Distanta implauzibila: "));
		Serial.print(value);
		Serial.println(F(" cm"));
		return DIST_ERR_RANGE;
	}

	*cm = value;
	return DIST_OK;
}

enum dist_status dist_sensor_measure(uint16_t *cm)
{
	enum dist_status status = DIST_ERR_NO_REPLY;

	rx_ignored = 0;

	for (uint8_t attempt = 0; attempt < DIST_RETRIES; attempt++)
	{
		request_send();

		if (!frame_collect())
		{
			Serial.print(F("  incercarea "));
			Serial.print(attempt + 1);
			Serial.print(F(": cadru incomplet, "));
			Serial.print(rx_buf_pos);
			Serial.print(F("/7 octeti ("));
			Serial.print(rx_ignored);
			Serial.println(F(" ignorati)"));
			status = DIST_ERR_NO_REPLY;
			continue;
		}

		status = frame_decode(cm);
		if (status == DIST_OK)
		{
			return DIST_OK;
		}
	}

	return status;
}
