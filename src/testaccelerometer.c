#include <stdio.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#include "printf.h"
#include "sio.h"

//how many ADC samples to sum together
#define AVG_COUNT 24U

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

	// Enable PB7-PB2 as output for LEDs
	DDRB |= _BV(PB7) | _BV(PB6) | _BV(PB5) | _BV(PB4) | _BV(PB3) | _BV(PB2);
	PORTB |= _BV(PB7) | _BV(PB5) | _BV(PB3);
	PORTB &= ~_BV(PB6) & ~_BV(PB4) & ~_BV(PB2);
}

void setLEDs(uint16_t accelValue)
{
	if (accelValue < 11000UL)
	{
		PORTB &= ~_BV(PB7) & ~_BV(PB5) & ~_BV(PB3);
	}
	else if (accelValue < 11500UL)
	{
		PORTB |= _BV(PB7);
		PORTB &= ~_BV(PB5) & ~_BV(PB3);
	}
	else if (accelValue < 12000UL)
	{
		PORTB |= _BV(PB7) | _BV(PB5);
		PORTB &= ~_BV(PB3);
	}
	else
	{
		PORTB |= _BV(PB7) | _BV(PB5) | _BV(PB3);
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

int main(void) {
	uint16_t xsum = 0, ysum = 0, zsum = 0;
	uint8_t position = 0;

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
					xsum += gXADCValue;
				} else if (gLastADCChannel == 1) { //Y
					ysum += gYADCValue;
				} else if (gLastADCChannel == 2) { //Z
					zsum += gZADCValue;

					position = (position + 1) % AVG_COUNT;


					if (position == 0) {
						cli();
						//Apparently, printf doesn't like to be interrupted. Just try it.
						//printf("%u,%u,%u,%u          \r", xsum, ysum, zsum, TCNT1);
						setLEDs(ysum);
						sei();
						xsum = ysum = zsum = 0;
					}
				} else {
					break;
				}
			}
		}
	}

	return 0;
}
