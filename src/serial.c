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

int chv[14];

void initserial(void) {
	nvic_set_priority(NVIC_USART2_IRQ, 0x40); // Enable nested IRQ
	nvic_enable_irq(NVIC_USART2_IRQ);
	nvic_enable_irq(NVIC_TIM1_UP_IRQ);

	USART1_BRR = 625; // 115200 baud @ PCLK2=72MHz
	USART1_CR1 = USART_CR1_UE | USART_CR1_TE;

	USART2_BRR = 312; // 115200 baud @ PCLK1=36MHz
	USART2_CR1 = USART_CR1_UE | USART_CR1_RE | USART_CR1_RXNEIE;

	TIM1_PSC = 71; // 1MHz
	TIM1_ARR = 3599; // 3.6ms
	TIM1_EGR = TIM_EGR_UG;
	TIM1_SR = ~TIM_SR_UIF;
	TIM1_DIER = TIM_DIER_UIE;
}

int _write(int fd, const char *buf, int len);
int _write(int fd, const char *buf, int len) { // STDOUT -> USART1_TX (blocking)
	if (fd != 1) {
		errno = EIO;
		return -1;
	}
	for (int i = 0; i < len; ++i) usart_send_blocking(USART1, buf[i]);
	return len;
}

// Single USART is used both for iBUS servo and telemetry data exchange in the following way:
// 1) Initially, USART is in full-duplex mode and is listening for servo data on the RX pin (TX is disabled).
// 2) Upon receiving a servo update, USART goes into half-duplex mode and starts listening
//    for sensor requests on the TX pin.
// 3) Upon receiving a sensor request (and if a response is required), RX is turned off
//    because it is designed to receive what is being trasmitted by TX in half-duplex mode.
// 4) When TX is turned on, i.e. TE=0->TE=1, this generates a necessary idle frame before transmission.
// 5) Upon transmitting the last byte, the TX handler waits for the transmission to complete (TC=1)
//    before disabling TX and turning RX back on.
// 6) USART reverts back to full-duplex mode after 3.6ms, and the cycle repeats.

static char tx[8];
static int txp, txq;

static void send2(char p, int x) {
	char a = x, b = x >> 8;
	int u = 0xfff9 - p - a - b;
	tx[0] = 6;
	tx[1] = p;
	tx[2] = a;
	tx[3] = b;
	tx[4] = u;
	tx[5] = u >> 8;
	txp = 0;
	txq = 6;
}

static void send4(char p, int x) {
	char a = x, b = x >> 8, c = x >> 16, d = x >> 24;
	int u = 0xfff7 - p - a - b - c - d;
	tx[0] = 8;
	tx[1] = p;
	tx[2] = a;
	tx[3] = b;
	tx[4] = c;
	tx[5] = d;
	tx[6] = u;
	tx[7] = u >> 8;
	txp = 0;
	txq = 8;
}

void usart2_isr(void) {
	int cr = USART2_CR1;
	if (cr & USART_CR1_TXEIE) {
		USART2_SR, USART2_DR = tx[txp++]; // Clear TXE+TC
		if (txp != txq) return;
		USART2_CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_TCIE;
		return;
	}
	if (cr & USART_CR1_TCIE) {
		USART2_CR1 = USART_CR1_UE | USART_CR1_RE | USART_CR1_RXNEIE;
		return;
	}
	static char a, b, c, d;
	static int u, m, n = 30;
	a = b;
	b = USART2_DR; // Clear RXNE
	if (USART2_CR3 & USART_CR3_HDSEL) { // iBUS sens
		if (m == 4 || ++m & 1) return;
		if (m == 4) { // End of chunk
			if (c != 4 || u != (a | b << 8)) return; // Sync lost
			m = 0;
			u = 0xffff;
			int i = (d & 0xf) - 1; // Sensor index
			if (i < 0) return; // Should never happen since ID=0 is for internal use
			int t = senstype(i);
			if (!t) return;
			switch (d & 0xf0) {
				case 0x80: // Probe
					tx[0] = c;
					tx[1] = d;
					tx[2] = a;
					tx[3] = b;
					txp = 0;
					txq = 4;
					break;
				case 0x90: // Type
					send2(d, t);
					break;
				case 0xa0: { // Value
					int v = sensval(i);
					switch (t >> 8) {
						case 2:
							send2(d, v);
							break;
						case 4:
							send4(d, v);
							break;
						default:
							return;
					}
					break;
				}
				default:
					return;
			}
			USART2_CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_TXEIE; // TE=0->TE=1 generates idle frame
			return;
		}
		c = a;
		d = b;
	} else { // iBUS servo
		if (a == 0x20 && b == 0x40) { // Sync
			n = 0;
			u = 0xff9f;
			return;
		}
		if (n == 30 || ++n & 1) return;
		int v = a | b << 8;
		if (n == 30) { // End of chunk
			if (u != v) return; // Sync lost
			update();
			m = 0;
			u = 0xffff;
			USART2_CR3 = USART_CR3_HDSEL;
			TIM1_CR1 = TIM_CR1_CEN | TIM_CR1_OPM;
			return;
		}
		chv[(n >> 1) - 1] = v & 0x0fff;
	}
	u -= a + b;
}

void tim1_up_isr(void) {
	TIM1_SR = ~TIM_SR_UIF;
	USART2_CR3 = 0;
}
