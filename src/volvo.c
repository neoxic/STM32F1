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

#define CH1_TRIM -50 // Bucket
#define CH2_TRIM 50 // Boom
#define CH3_TRIM -80 // Stick

#define VALVE_MIN 220 // Still closed
#define VALVE_MAX 280 // Fully open
#define VALVE_MUL 100 // Input multiplier (%)

#define PUMP_MIN 60 // Minimum duty
#define PUMP_MAX 340 // Maximum duty
#define PUMP_LIM 20 // Acceleration limit

#define DRIVE_MIN 50 // Minimum duty
#define DRIVE_MAX 500 // Maximum duty
#define DRIVE_LIM 10 // Acceleration limit

#define VOLT1 3381 // mV
#define VOLT2 3719 // xx.xxV = VOLT1*(R1+R2)/R2

static int input1(int t, int *u, int x, int s) {
	if (s) { // Drive mode switch
		*u = 1500 + x;
		return t - 1500;
	}
	*u = t + x;
	t = t < 1500 ? 1500 - t : t - 1500;
	if (t < VALVE_MIN) return 0;
	if (t < VALVE_MAX) return VALVE_MUL * (t - VALVE_MIN) / 200;
	return VALVE_MUL * (t - (VALVE_MIN + VALVE_MAX) / 2) / 100;
}

static int input2(int t) {
	return t - 1500;
}

static int input3(int t) {
	if (t < 1450) return 0;
	if (t > 1550) return 2;
	return 1;
}

static int output1(int t, int *s) {
	if (!(*s = !!t)) return 1500;
	if ((t += PUMP_MIN) > PUMP_MAX) return 1500 + PUMP_MAX;
	return 1500 + t;
}

static int output2(int t) {
	if (t > -DRIVE_MIN && t < DRIVE_MIN) return 1500;
	if (t < -DRIVE_MAX) return 1500 - DRIVE_MAX;
	if (t > DRIVE_MAX) return 1500 + DRIVE_MAX;
	return 1500 + t;
}

static int ramp(int t, int u, int x) {
	if (!u || !x) return t;
	if (t < 1500) {
		u -= x;
		if (u > 1450) u = 1450;
		if (t < u) return u;
	} else {
		u += x;
		if (u < 1550) u = 1550;
		if (t > u) return u;
	}
	return t;
}

static int u1, u2, u3, u4, u5, u6, u7;
static int i1, i2, i3, i4, i5, i6;
static int s1, s2;

void update(void) {
	s1 = input3(chv[6]);
	s2 = input3(chv[7]);

	i1 = input1(chv[0], &u1, CH1_TRIM, s2);
	i2 = input1(chv[1], &u2, CH2_TRIM, s2 == 2);
	i3 = input1(chv[2], &u3, CH3_TRIM, s2 == 1);
	i4 = input2(chv[3]);
	i5 = input2(chv[4]);
	i6 = input2(chv[5]);

	int sl;
	switch (s2) { // Drive mode
		case 0:
			u4 = ramp(output1(i1 + i2 + i3, &sl), u4, PUMP_LIM);
			u5 = ramp(output2(i4), u5, DRIVE_LIM);
			u6 = ramp(output2(i5), u6, DRIVE_LIM);
			u7 = ramp(output2(i6), u7, DRIVE_LIM);
			break;
		case 1:
			u4 = ramp(output1(i2, &sl), u4, PUMP_LIM);
			u5 = ramp(output2(i1), u5, DRIVE_LIM);
			u6 = ramp(output2(i3 + i4), u6, DRIVE_LIM);
			u7 = ramp(output2(i3 - i4), u7, DRIVE_LIM);
			break;
		case 2:
			u4 = ramp(output1(i3, &sl), u4, PUMP_LIM);
			u5 = ramp(output2(i4), u5, DRIVE_LIM);
			u6 = ramp(output2(i2 + i1), u6, DRIVE_LIM);
			u7 = ramp(output2(i2 - i1), u7, DRIVE_LIM);
			break;
	}

	TIM3_CCR1 = u1;
	TIM3_CCR2 = u2;
	TIM3_CCR3 = u3;
	TIM3_CCR4 = u4;
	TIM2_CCR2 = u5;
	TIM2_CCR3 = u6;
	TIM2_CCR4 = u7;

	GPIOA_BSRR = s1 ? 0x0001 : 0x00010000; // A0
	GPIOC_BSRR = sl ? 0x20000000 : 0x2000; // C13

	WWDG_CR = 0xff; // Reset watchdog
#ifdef DEBUG
	static int n;
	if (++n < 26) return; // 130Hz -> 5Hz
	SCB_SCR = 0; // Resume main loop
	n = 0;
#endif
}

int sensors[3] = {0x040201, 0x050203};

int sensor(int i, int v) {
	switch (i) {
		case 0: // TMP36 sensor
			return ((v * VOLT1) >> 12) - 100;
		case 1: // Voltage divider
			return (v * VOLT2) >> 12;
	}
	return 0;
}

void main(void) {
	rcc_clock_setup_pll(&rcc_hse_configs[RCC_CLOCK_HSE8_72MHZ]); // PCLK1=36MHz, PCLK2=72MHz

	RCC_APB2ENR = RCC_APB2ENR_AFIOEN | RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN | RCC_APB2ENR_IOPCEN | RCC_APB2ENR_ADC1EN | RCC_APB2ENR_TIM1EN | RCC_APB2ENR_USART1EN;
	RCC_APB1ENR = RCC_APB1ENR_TIM2EN | RCC_APB1ENR_TIM3EN | RCC_APB1ENR_WWDGEN | RCC_APB1ENR_USART2EN;

	// Default GPIO state - output low
	AFIO_MAPR = 0x00000200; // TIM2_REMAP=10
	GPIOA_ODR = 0x0000a408; // A3,A10,A13,A15 (pull-up)
	GPIOC_ODR = 0x00002000; // C13 (high)
	GPIOA_CRL = 0xaa008aa2; // A1 (TIM2_CH2), A2 (USART2_TX), A3 (USART2_RX), A4,A5 (analog), A6 (TIM3_CH1), A7 (TIM3_CH2)
	GPIOA_CRH = 0x888228a2; // A9 (USART1_TX), A10 (USART1_RX), A13,A14,A15 (SWD)
	GPIOB_CRL = 0x222222aa; // B0 (TIM3_CH3), B1 (TIM3_CH4)
	GPIOB_CRH = 0x2222aa22; // B10 (TIM2_CH3), B11 (TIM2_CH4)
	GPIOC_CRL = 0x22222222;
	GPIOC_CRH = 0x22222222;

	WWDG_CFR = 0x1ff; // Watchdog timeout 4096*8*64/PCLK1=~58ms
	ADC1_CR2 = ADC_CR2_ADON; // Early ADC power-on

	TIM2_PSC = 71; // 1MHz
	TIM2_ARR = 3999; // 250Hz
	TIM2_EGR = TIM_EGR_UG;
	TIM2_CR1 = TIM_CR1_CEN;
	TIM2_CCMR1 = TIM_CCMR1_OC2PE | TIM_CCMR1_OC2M_PWM1;
	TIM2_CCMR2 = TIM_CCMR2_OC3PE | TIM_CCMR2_OC3M_PWM1 | TIM_CCMR2_OC4PE | TIM_CCMR2_OC4M_PWM1;
	TIM2_CCER = TIM_CCER_CC2E | TIM_CCER_CC3E | TIM_CCER_CC4E;

	TIM3_PSC = 71; // 1MHz
	TIM3_ARR = 3999; // 250Hz
	TIM3_EGR = TIM_EGR_UG;
	TIM3_CR1 = TIM_CR1_CEN;
	TIM3_CCMR1 = TIM_CCMR1_OC1PE | TIM_CCMR1_OC1M_PWM1 | TIM_CCMR1_OC2PE | TIM_CCMR1_OC2M_PWM1;
	TIM3_CCMR2 = TIM_CCMR2_OC3PE | TIM_CCMR2_OC3M_PWM1 | TIM_CCMR2_OC4PE | TIM_CCMR2_OC4M_PWM1;
	TIM3_CCER = TIM_CCER_CC1E | TIM_CCER_CC2E | TIM_CCER_CC3E | TIM_CCER_CC4E;

	initsensor();
	initserial();
#ifdef DEBUG
	printf("\n");
	printf("  U1   U2   U3   U4   U5   U6   U7      I1   I2   I3   I4   I5   I6    SW\n");
#endif
	for (;;) {
		SCB_SCR = SCB_SCR_SLEEPONEXIT; // Suspend main loop
		__WFI();
#ifdef DEBUG
		printf("%4d %4d %4d %4d %4d %4d %4d    %4d %4d %4d %4d %4d %4d    %d %d\n",
			u1, u2, u3, u4, u5, u6, u7, i1, i2, i3, i4, i5, i6, s1, s2);
#endif
	}
}
