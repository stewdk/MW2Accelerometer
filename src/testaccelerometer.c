#include <stdio.h>
#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#include "printf.h"
#include "sio.h"

//how many ADC samples to sum together
#define AVG_COUNT 24

//global variable to let the main program know that an A/D conversion finished
static volatile int8_t gADC = 0;
static volatile int8_t gLastADCChannel = 0;
static volatile uint16_t gXADCValue = 0;
static volatile uint16_t gYADCValue = 0;
static volatile uint16_t gZADCValue = 0;

/* Set data direction registers to all inputs. Also sets PA7 to be an output
 * and drives it high to keep CC2500 off. Also sets PC6 to be an output and
 * drives it high to keep MW2 LED off.
 */
static void set_DDR(void) {
	//All inputs
	DDRA = DDRB = DDRC = DDRD = 0;
	//All pull-up resistors activated
	PORTB = PORTC = PORTD = 0xFF;
	//except for PA0, PA1, and PA2 (we don't want pull-ups on accelerometer inputs)
	PORTA = ~_BV(PA2) & ~_BV(PA1) & ~_BV(PA0);

	// Keep CC2500 off
	DDRA |= _BV(PA7);
	PORTA |= _BV(PA7);

	// Enable onboard LED as output but keep it dark
	DDRC |= _BV(PC6);
	PORTC |= _BV(PC6);

	// Enable PB7-PB3 as output for LEDs
	DDRB |= _BV(PB7) | _BV(PB6) | _BV(PB5) | _BV(PB4) | _BV(PB3);
	PORTB &= ~_BV(PB7) & ~_BV(PB6) & ~_BV(PB5) & ~_BV(PB4) & ~_BV(PB3);
}

void setLEDs(uint16_t accelValue)
{
	if (accelValue < 1375UL)
	{
		PORTB &= ~_BV(PB7) & ~_BV(PB6) & ~_BV(PB5) & ~_BV(PB4) & ~_BV(PB3);
	}
	else if (accelValue < 1675UL)
	{
		PORTB |= _BV(PB4);
		PORTB &= ~_BV(PB7) & ~_BV(PB6) & ~_BV(PB5) & ~_BV(PB3);
	}
	else if (accelValue < 1975UL)
	{
		PORTB |= _BV(PB3) | _BV(PB5) | _BV(PB4);
		PORTB &= ~_BV(PB6) & ~_BV(PB7);
	}
	else
	{
		PORTB |= _BV(PB7) | _BV(PB6) | _BV(PB5) | _BV(PB4) | _BV(PB3);
	}
}

void setupADC(void) {
	//AVCC voltage reference, channel 0
	ADMUX = _BV(REFS0);
	//to left adjust result, use ADLAR bit in ADMUX
	//Enable, auto trigger, clear flag, interrupt enable, 64 prescaler for 125kHz operation
	ADCSRA = _BV(ADEN) | _BV(ADATE) | _BV(ADIF) | _BV(ADIE) | _BV(ADPS2) | _BV(ADPS1);
	//Timer/Counter0 Compare Match
	ADCSRB = _BV(ADTS1) | _BV(ADTS0);
}

void setupTimer0(void) {
	//OC0A disconnected, clear timer on compare match
	TCCR0A = _BV(WGM01);
	//divide clk by 64
	TCCR0B = _BV(CS01) | _BV(CS00);
	//compare match every 32 cycles of 125kHz
	OCR0A = 32;
}

void setupTimer1(void) {
	TCCR1A = 0;
	// Sets a prescaler of 64
	TCCR1B = _BV(CS11) | _BV(CS10);
}

static void setADCChannel(uint8_t ch) {
	ADMUX = (ADMUX & ~(_BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1) | _BV(MUX0))) | ch;
}

static uint8_t getADCChannel(void) {
	return ADMUX & (_BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1) | _BV(MUX0));
}

ISR(ADC_vect) {
	//notify the main program that the A/D conversion is finished
	gADC = 1;

	gLastADCChannel = getADCChannel();

	//store the ADC result
	switch(gLastADCChannel) {
		case 0:
			gXADCValue = ADC;
			break;
		case 1:
			gYADCValue = ADC;
			break;
		case 2:
			gZADCValue = ADC;
			break;
		default:
			gADC = -1;
			break;
	}

	//for the next A/D conversion, check the next channel
	setADCChannel((gLastADCChannel + 1) % 3);
	//clear the timer0 flag
	TIFR0 = _BV(OCF0A);
}

//integer square root routine (http://www.finesse.demon.co.uk/steven/sqrt.html)
#define iter1(N) \
    try = root + (1 << (N)); \
    if (n >= try << (N))   \
    {   n -= try << (N);   \
        root |= 2 << (N); \
    }

uint32_t isqrt(uint32_t n)
{
    uint32_t root = 0, try;
    iter1 (15);    iter1 (14);    iter1 (13);    iter1 (12);
    iter1 (11);    iter1 (10);    iter1 ( 9);    iter1 ( 8);
    iter1 ( 7);    iter1 ( 6);    iter1 ( 5);    iter1 ( 4);
    iter1 ( 3);    iter1 ( 2);    iter1 ( 1);    iter1 ( 0);
    return root >> 1;
}

int main(void) {
	uint16_t xSum = 0, ySum = 0, zSum = 0;
	uint8_t position = 0;

	/* experimental results with with AVG_COUNT = 24
	x: 10000 - 12790
	range: 2790
	mid: 11395

	y: 10610 - 13450
	range: 2840
	mid: 12030

	z: 11080 - 13780
	range: 2700
	mid: 12430
	*/

	const uint16_t xZero = 475 * AVG_COUNT; //500;
	const uint16_t yZero = 501 * AVG_COUNT; //500;
	const uint16_t zZero = 518 * AVG_COUNT; //535;

	int16_t x, y, z;
	uint32_t magnitude;

	MCUCR |= _BV(JTD);		// Turn off JTAG interface to allow control of upper Port C pins
	MCUCR |= _BV(JTD);		// Must be done twice within 4 cycles to take effect

	// Set all Port I/O pins and directions
	set_DDR();

	setupADC();

	setupTimer0();
	setupTimer1();

	// Enable interrupts
	sei();

	setupPrintf();

	while (1) {
		if (gADC) {
			if (gADC < 0) {
				printf("\r\nError: bad ADC channel\r\n");
				break;
			} else {
				cli();
				gADC = 0;
				sei();

				if (gLastADCChannel == 0) { //X
					xSum += gXADCValue;
				} else if (gLastADCChannel == 1) { //Y
					ySum += gYADCValue;
				} else if (gLastADCChannel == 2) { //Z
					zSum += gZADCValue;

					position = (position + 1) % AVG_COUNT;


					if (position == 0) {
						x = xSum - xZero;
						y = ySum - yZero;
						z = zSum - zZero;
						
						//absolute value
						x = x < 0 ? -x : x;
						y = y < 0 ? -y : y;
						z = z < 0 ? -z : z;

						//Pythagorean theorem
						magnitude = isqrt(
							(uint32_t)x * (uint32_t)x +
							(uint32_t)y * (uint32_t)y +
							(uint32_t)z * (uint32_t)z);

						cli();
						//Apparently, printf doesn't like to be interrupted. Just try it.
						//printf("%lu,   %d,%d,%d,    %u          \r", magnitude, x, y, z, TCNT1);
						setLEDs(magnitude);
						sei();
						xSum = ySum = zSum = 0;
					}
				} else {
					break;
				}
			}
		}
	}

	return 0;
}