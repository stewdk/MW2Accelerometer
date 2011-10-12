/*
 * This is free software distributed under the terms of the GNU General
 * Public License version 3. See the file COPYING.TXT in the parent directory.
 *
 * Andrew Sterian
 * Padnos College of Engineering and Computing
 * Grand Valley State University
 * <steriana@gvsu.edu> -- <http://claymore.engineer.gvsu.edu/~steriana>
 * 
 * This program is intended to reside on a MW2 board. The target is
 * an ATmega324P clocked with a 8 MHz internal clock (i.e., F_CPU should be
 * defined as 8000000).
 *
 * The fuses should be set to:
 *
 *   - Internal 8 MHz clock
 *   - EEPROM saved across erasures
 *   - Brownout detection enabled at 2.7V
 *
 * This program demonstrates the use of printf() using AVR-LIBC. It communicates
 * with a terminal program at 38400 bps, 8 data bits, no parity, 1 stop bit, no
 * flow control.
 */

#include <stdio.h>
#include <stdlib.h>

#include "sio.h"

/*
 * This function is the ultimate output function of printf(). It takes a
 * character to print and a file stream. We can ignore the file stream if
 * there is only place for characters to go. This function simply calls
 * the "output()" function in the SIO module.
 */
static int stdio_put(char c, FILE *stream)
{
	(void)stream;	// Tell C compiler that "stream" is unused so we
					// don't get any warnings.

	output(c);

	return 0;      // 0 return value indicates success
}

/*
 * This function is the ultimate input function of scanf(). It takes a
 * file stream to read from, which we ignore since we will always read
 * characters from the SIO module. The function simply calls the "inchar()"
 * function in the SIO module and returns the character read.
 */
static int stdio_get(FILE *stream)
{
	(void)stream;
	return inchar();
}

/*
 * The variable "mystream" serves as the Standard I/O FILE structure for both
 * reading and writing. It will be used for stdio, stdout, and stderr. Note
 * that it is passed the address of the "stdio_put()" and "stdio_get()" functions
 * defined above.
 */
static FILE mystream = FDEV_SETUP_STREAM(stdio_put, stdio_get, _FDEV_SETUP_RW);

void setupPrintf(void)
{
	// Initialize the serial port communications
	sio_init();
	// Set the Standard I/O file streams to the address of our FILE structure
	stdin = stdout = stderr = &mystream;
}
