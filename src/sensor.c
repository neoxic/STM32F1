/*
** Copyright (C) Arseny Vakhrushev <arseny.vakhrushev@me.com>
**
** This firmware is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
**
** This firmware is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this firmware. If not, see <http://www.gnu.org/licenses/>.
*/

#include "common.h"

void initsensor(void) {
	ADC1_CR2 = ADC_CR2_ADON | ADC_CR2_CAL; // Start calibration
	while (ADC1_CR2 & ADC_CR2_CAL); // Calibration in progress
	ADC1_SMPR1 = -1; // Maximum sampling time
	ADC1_SMPR2 = -1;
}

int senstype(int i) {
	return i < 3 ? sensors[i] & 0xffff : 0;
}

int sensval(int i) {
	static int s[3];
	if (i >= 3) return 0;
	int q = sensors[i];
	if (!q) return 0;
	ADC1_SQR3 = q >> 16; // Set channel
	ADC1_CR2 = ADC_CR2_ADON; // Start conversion
	while (!(ADC1_SR & ADC_SR_EOC)); // Conversion in progress
	int x = ADC1_DR; // Clear EOC
	if (!(q = s[i])) s[i] = q = x << 7;
	return sensor(i, (s[i] = x + q - (q >> 7)) >> 7);
}
