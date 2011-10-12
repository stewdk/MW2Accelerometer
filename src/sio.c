/*
 * This is free software distributed under the terms of the GNU General
 * Public License version 3. See the file COPYING.TXT in the parent directory.
 *
 * Andrew Sterian
 * Padnos College of Engineering and Computing
 * Grand Valley State University
 * <steriana@gvsu.edu> -- <http://claymore.engineer.gvsu.edu/~steriana>
 *
 * This is a serial driver for Atmel AVR's. It has the following features:
 *
 *  - written in C for portability and extensibility. Rewriting key parts
 *    in assembly should provide a notable speed boost, however.
 *
 *  - it can work with or without interrupts. When interrupts are enabled,
 *    buffers are used for both reception and transmission to ensure no
 *    data is lost on reception, and the application does not block (i.e.,
 *    does not have to wait) on transmission.
 *
 *  - it supports XON/XOFF flow control when interrupts are used
 *
 *  - the following devices are supported without modification:
 *          ATmega324P
 *          ATmega168
 *          ATmega162
 *          ATmega128
 *          ATmega88
 *          ATmega64
 *          ATmega48
 *          ATmega32
 *          ATmega16
 *          ATmega8
 *
 * The following functions are available:
 *      void sio_init(void)
 *              Initializes the serial port and configures (but does
 *              not enable) interrupts. This function must be called
 *              when the AVR is first initialized prior to serial 
 *              transmission or reception. You must enable interrupts
 *              manually using sei() after calling sio_init().
 *
 *              The configuration is set to 8-N-1 in this function,
 *              so modify it if you need anything else.
 *
 *      uint8_t isinput(void)
 *              Returns non-zero if data has been received, zero
 *              otherwise. This function does not block.
 *
 *      int16_t input(void)
 *              Returns a received character in the range 0x0000 to
 *              0x00FF, or -1 (0xFFFF) if no data is available. This
 *              function never blocks. It will never return -1 if
 *              isinput() was called and returned non-zero.
 *
 *      uint8_t inchar(void)
 *              Waits (blocks) for a character to be received then
 *              returns that character. If Ctrl-C handling is enabled,
 *              this function will return with garbage data if a Ctrl-C
 *              was encountered while waiting.
 *
 *      void output(uint8_t c)
 *              Writes a single character to the serial port, blocking
 *              only if the output buffer is full. This function always
 *              blocks if USE_INTERRUPTS is not defined below.
 *
 *      void outstr(const char *s)
 *              Write a C string (null-terminated) to the serial port,
 *              blocking only if the output buffer is full. This function
 *              always blocks if USE_INTERRUPTS is not defined below.
 *
 *      void outstr_P(PGM_P s)
 *              Write a C string from program space (i.e., FLASH) to the
 *              serial port. Example:
 *
 *                       outstr_P(PSTR("Hello world!\n"));
 *
 *              For constant strings like the example above, this is less
 *              wasteful of RAM but a little bit slower. Otherwise, what
 *              AVR-GCC does is put the string in the .data section, which
 *              means it resides in FLASH and gets copied to RAM when the
 *              program starts, thus occupying space in both FLASH and RAM. The
 *              example above, however, uses no RAM.
 *
 *      void sio_flush(void)
 *              This function waits for the serial transmitter to
 *              be empty. This will be the case when the entire transmit
 *              buffer is flushed.
 *
 */

/*******************************************************************
 *
 * This section contains parameters that may be modified by the
 * programmer.
 *
 *******************************************************************/

/* Define this to the value to write to the UBRR (UART Baud Rate
 * Register), effectively defining your serial baud rate. You must
 * take the AVR clock frequency into account. The USART asynchronous
 * normal mode is used so the required UBRR value is given by (cf.
 * page 172 of ATmega128 data sheet):
 *
 *          UBRR = (Fosc/(16*BaudRate)) - 1
 */
#define UBRR_VALUE (((unsigned long)F_CPU/(16UL*38400UL))-1)

/* For AVR's with more than one USART, indicate which one should be
 * used by uncommenting one of the two following lines. For AVR's with
 * only one USART, this should be left as 0.
 */
#define WHICH_USART 0
//#define WHICH_USART 1

/* For AVR's with more than one USART, you may want to instantiate this module
 * twice, once for each USART. You would copy this file to sio0.c (and #define
 * WHICH_USART to 0) and copy this file to sio1.c (and #define WHICH_USART to
 * 1) to handle both USART's. If doing so, different function names need to be
 * used. In this case, uncomment the following line to modify the function
 * names in this module (e.g., input, inchar, outstr) to have the USART number
 * (0 or 1) appended. You would, then call the modified function names:
 *
 *          sio_init0();
 *          sio_init1();
 *          ...
 *          switch (inchar0()) {....
 */
//#define USE_USART_NUMBER

/* Uncomment the following to use interrupt-driven receive and transmit. 
 * This is recommended, as both buffering and software handshaking are
 * supported. With USE_INTERRUPTS undefined, USART data is received only
 * by constant polling, and is transmitted one character at a time, blocking
 * until the USART transmitter is free to transmit.
 */
#define USE_INTERRUPTS

/* Uncomment the following to use XON/XOFF software flow control. This
 * flow control is not used unless USE_INTERRUPTS is defined. Software
 * flow control adds about 300 bytes to FLASH usage. */
//#define USE_XON_XOFF

/* Define the size (in bytes) of the receive buffer. The bigger the buffer,
 * the less chance there is of losing incoming characters, since the client
 * side probably does not stop sending characters immediately after receiving
 * an XOFF.
 *
 * This constant is not used unless USE_INTERRUPTS is defined.
 */
#define SIO_RX_BUFSIZE  128

/* Define the size (in bytes) of the transmit buffer. Note that the driver
 * will block when an output function is called (e.g., outstr, output) and
 * there is no room left in this transmit buffer. If you don't want the
 * caller of these functions to block, make the transmit buffer big enough.
 *
 * This constant is not used unless USE_INTERRUPTS is defined.
 */
#define SIO_TX_BUFSIZE  128

/* Define the condition for sending an XOFF to the client to stop transmission.
 * This condition is the number of bytes in the receive buffer. Once the number
 * of bytes in the receive buffer goes above SIO_RX_XOFFLEVEL, an XOFF is sent
 * to the client. Setting this to 75% of the receive buffer size is a reasonable
 * compromise between efficient memory usage and protection from loss of data.
 *
 * This constant is not used unless USE_INTERRUPTS is defined and USE_XON_XOFF
 * is defined.
 */
#define SIO_RX_XOFFLEVEL (3*SIO_RX_BUFSIZE/4)

/* Define the condition for sending an XON to the client to restart transmission.
 * This condition is the number of bytes in the receive buffer. Once the number
 * of bytes in the receive buffer drops below SIO_RX_XONLEVEL, an XON is sent
 * to the client.
 *
 * This constant is not used unless USE_INTERRUPTS is defined and USE_XON_XOFF
 * is defined.
 */
#define SIO_RX_XONLEVEL (1*SIO_RX_BUFSIZE/4)

/* Define the characters used for XON/XOFF. ASCII 0x11 and 0x13 are pretty
 * standard so you shouldn't have to change these.
 *
 * These constants are not used unless USE_INTERRUPTS is defined and USE_XON_XOFF
 * is defined.
 */
#define XON_CHAR  0x11
#define XOFF_CHAR 0x13

/* Uncomment the following to display error messages (by transmitting
 * characters through the USART) due to overrun errors, framing errors,
 * or receive-buffer-full errors. With this commented out, the errors
 * are not reported. In either case, the input buffer is flushed.
 */
//#define SHOW_RX_ERRORS

/* Uncomment the following to enable watchdog resets during blocking
 * operations (and at other random times, too).
 */
#define FeedCOP  asm("  wdr");

/* Uncomment the following to enable Ctrl-C handling. If you do so, you also
 * have to define SET_ABORT and CHECK_ABORT below. SET_ABORT is the name of a
 * function to call when a Ctrl-C (i.e., ASCII code 0x03) is received. This
 * Ctrl-C mechanism allows immediate (relatively speaking) termination of a
 * long-duration code loop from within this serial driver, without having to
 * constantly check for serial data (and do something with it) while in this
 * loop. CHECK_ABORT is the name of a function that returns non-zero if an
 * abort (i.e., Ctrl-C) has been signalled.
 *
 * For example:
 *      #define SIO_CTRL_C_ABORTS
 *      #define SET_ABORT gQuitNow=1
 *      #define CHECK_ABORT gQuitNow
 *      ...
 *         gQuitNow = 0;
 *         while (! gQuitNow) {
 *           doSomeHardWork();
 *           if (hardWorkDone()) break;
 *         }
 */
#define SIO_CTRL_C_ABORTS
#define SET_ABORT asm volatile ("jmp 0x0000"); //(what to do if Ctrl-C is received)
#define CHECK_ABORT (0) //(returns non-zero if Ctrl-C was received)

/*******************************************************************
 *
 * This is the end of the configuration parameters. You shouldn't
 * have to change anything below this point.
 *
 *******************************************************************/

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include "sio.h"

/* Some AVR's require bit 7 of the UCSRC to be set when writing to
 * the UCSRC, as it shares the same memory location as the UBRR, which
 * is selected by leaving the bit clear.
 */
#if defined(__AVR_ATmega32__) || defined(__AVR_ATmega16__) || defined(__AVR_ATmega8__) || defined(__AVR_ATmega162__)
#  if defined(__AVR_ATmega162__)
#    define URSEL_BIT (_BV(URSEL0))
#  else
#    define URSEL_BIT (_BV(URSEL))
#  endif
#else
#  define URSEL_BIT (0x00)
#endif

/* These chips use register names like UDR0 and UCSR0A even though they have only 1 USART */
#if defined(__AVR_ATmega48__) || defined(__AVR_ATmega88__) || defined(__AVR_ATmega168__)
#  define UCSRA UCSR0A
#  define UCSRB UCSR0B
#  define UCSRC UCSR0C
#  define UBRRH UBRR0H
#  define UBRRL UBRR0L
#  define UDR   UDR0
#  define SIG_UART_RECV SIG_USART_RECV
#  define SIG_UART_DATA SIG_USART_DATA
#endif

/* These chips use bit names like DOR0 and TXC0 even though they have only 1 USART */
#if defined(__AVR_ATmega48__) || defined(__AVR_ATmega88__) || defined(__AVR_ATmega168__) || defined(__AVR_ATmega162__)
#  define DOR    DOR0
#  define FE     FE0
#  define UDRIE  UDRIE0
#  define TXC    TXC0
#  define RXEN   RXEN0
#  define TXEN   TXEN0
#  define RXCIE  RXCIE0
#endif

/* These chips have 2 USART's so we select among the available functions */
#if defined(__AVR_ATmega128__) || defined(__AVR_ATmega64__) || defined(__AVR_ATmega162__) \
    || defined(__AVR_ATmega324P__)
#  if WHICH_USART==0  // USART0
#    define UCSRA UCSR0A
#    define UCSRB UCSR0B
#    define UCSRC UCSR0C
#    define UBRRH UBRR0H
#    define UBRRL UBRR0L
#    define UDR   UDR0
#    if defined(__AVR_ATmega128__) || defined(__AVR_ATmega64__)
#      define SIG_UART_RECV SIG_UART0_RECV
#      define SIG_UART_DATA SIG_UART0_DATA
#    elif defined(__AVR_ATmega324P__)
#      define SIG_UART_RECV SIG_USART_RECV
#      define SIG_UART_DATA SIG_USART_DATA
#      define DOR   DOR0
#      define FE    FE0
#      define TXC   TXC0
#      define UDRIE UDRIE0
#      define RXCIE RXCIE0
#      define RXEN  RXEN0
#      define TXEN  TXEN0
#    else
#      define SIG_UART_RECV SIG_USART0_RECV
#      define SIG_UART_DATA SIG_USART0_DATA
#    endif
#    ifdef USE_USART_NUMBER
#       define sio_init  sio_init0
#       define isinput   isinput0
#       define input     input0
#       define inchar    inchar0
#       define output    output0
#       define outstr    outstr0
#       define outstr_P  outstr_P0
#       define sio_flush sio_flush0
#    endif
#  elif WHICH_USART==1  // USART1
#    define UCSRA UCSR1A
#    define UCSRB UCSR1B
#    define UCSRC UCSR1C
#    define UBRRH UBRR1H
#    define UBRRL UBRR1L
#    define UDR   UDR1
#    if defined(__AVR_ATmega128__) || defined(__AVR_ATmega64__)
#      define SIG_UART_RECV SIG_UART1_RECV
#      define SIG_UART_DATA SIG_UART1_DATA
#    elif defined(__AVR_ATmega324P__)
#      define SIG_UART_RECV SIG_USART1_RECV
#      define SIG_UART_DATA SIG_USART1_DATA
#      define DOR   DOR1
#      define FE    FE1
#      define TXC   TXC1
#      define UDRIE UDRIE1
#      define RXCIE RXCIE1
#      define RXEN  RXEN1
#      define TXEN  TXEN1
#    else
#      define SIG_UART_RECV SIG_USART1_RECV
#      define SIG_UART_DATA SIG_USART1_DATA
#    endif
#    ifdef USE_USART_NUMBER
#       define sio_init  sio_init1
#       define isinput   isinput1
#       define input     input1
#       define inchar    inchar1
#       define output    output1
#       define outstr    outstr1
#       define outstr_P  outstr_P1
#       define sio_flush sio_flush1
#    endif
#  else
#    error "Unknown value of WHICH_USART"
#  endif
#else
#  if WHICH_USART != 0
#    error "This device has only one USART. You must leave WHICH_USART set to 0."
#  endif
#endif // ATmega128 or ATmega64

#ifdef USE_INTERRUPTS

#ifndef interruptsOFF
#  define interruptsOFF cli
#endif
#ifndef interruptsON
#  define interruptsON  sei
#endif

static uint8_t *_rx_tail ;
static uint8_t * volatile _rx_head ;
static uint8_t _rx_buf_start[SIO_RX_BUFSIZE];
static uint8_t volatile _rx_flags ;
static uint16_t volatile _rx_chars ;
#define _rx_buf_limit (_rx_buf_start+sizeof(_rx_buf_start))

static uint8_t * volatile _tx_tail ;
static uint8_t *_tx_head ;
static uint8_t _tx_buf_start[SIO_TX_BUFSIZE];
static uint8_t volatile _tx_flags ;
#define _tx_buf_limit (_tx_buf_start+sizeof(_tx_buf_start))

// Flags for _tx_flags
#define TX_FLAG_SEND_XOFF  0x01 // transmitter should send XOFF instead of next character
#define TX_FLAG_SEND_XON   0x02 // transmitter should send XON instead of next character
#define TX_FLAG_SUSPENDED  0x04 // Host has requested XOFF

// Flags for _rx_flags
#define RX_FLAG_SUSPENDED  0x01  // XOFF has been sent to host
#define RX_FLAG_OVERRUN    0x02  // Software overrun (rather than USART overrun)
#define RX_FLAG_FE         0x10  // framing error, same as _BV(FE) bit in AVR
#define RX_FLAG_OR         0x08  // overrun, same as _BV(DOR) bit in AVR

/* SIGNAL(xxx) runs with interrupts disabled */
/* Interrupt handler for data reception. */
SIGNAL(SIG_UART_RECV)
{
  uint8_t scsr = UCSRA & (_BV(DOR)|_BV(FE));

  _rx_flags |= scsr;
  *_rx_head = UDR;

  // If no errors have been flagged for this character, check for XON/XOFF reception
  // then check for need to send XOFF if high watermark reached.

  if (scsr) {
    // Ignore data if we have overrun or framing error.
    return;
  }

#ifdef USE_XON_XOFF
  if (*_rx_head == XOFF_CHAR) {
    _tx_flags |= TX_FLAG_SUSPENDED;
    return;

  } else if (*_rx_head == XON_CHAR) {
    _tx_flags &= ~TX_FLAG_SUSPENDED;

    /* Enable transmit data register empty interrupt */
    UCSRB |= _BV(UDRIE);
    return;
  }
#endif

#ifdef SIO_CTRL_C_ABORTS
  if (*_rx_head == 0x03) {
    SET_ABORT;
    return;
  }
#endif

  // It's not a special character, so it's a data character.
#ifdef USE_XON_XOFF
  if ((++_rx_chars >= SIO_RX_XOFFLEVEL) && ((_rx_flags & RX_FLAG_SUSPENDED) == 0)) {
    // We've gone above XOFF level, send XOFF and enable transmitter
    _tx_flags |= TX_FLAG_SEND_XOFF;
    _rx_flags |= RX_FLAG_SUSPENDED;

    /* Enable transmit data register empty interrupt */
    UCSRB |= _BV(UDRIE);
  }
#else
  ++_rx_chars;
#endif
      
  // Finally, update head pointer to indicate received character
  if (++_rx_head >= _rx_buf_limit) {
    _rx_head = _rx_buf_start;
  }
  if (_rx_head == _rx_tail) {
    _rx_flags |= RX_FLAG_OVERRUN;
  }
}
      
/* SIGNAL(xxx) runs with interrupts disabled */
/* Interrupt handler for data transmission. */
SIGNAL(SIG_UART_DATA)
{
#ifdef USE_XON_XOFF
  // If request to send XON/XOFF is pending, do it instead of next buffer character
  if (_tx_flags & TX_FLAG_SEND_XON) {
      UDR = XON_CHAR; UCSRA = _BV(TXC); // Clear TXC bit...just in case? (probably not necessary)
      _tx_flags &= ~TX_FLAG_SEND_XON;

  } else if (_tx_flags & TX_FLAG_SEND_XOFF) {
      UDR = XOFF_CHAR; UCSRA = _BV(TXC); // Clear TXC bit...just in case? (probably not necessary)
      _tx_flags &= ~TX_FLAG_SEND_XOFF;

  } else 
#endif
    if ((_tx_tail != _tx_head) 
#ifdef USE_XON_XOFF
        && ((_tx_flags & TX_FLAG_SUSPENDED) == 0)
#endif
        ) {
      UDR = *_tx_tail++; UCSRA = _BV(TXC); // Clear TXC bit...just in case? (probably not necessary)
      if (_tx_tail >= _tx_buf_limit) {
          _tx_tail = _tx_buf_start;
      }
  } else {
    /* No more characters to transmit or we've been requested to XOFF. Disable interrupt. */
      UCSRB &= ~_BV(UDRIE);
  }
}

uint8_t isinput(void)
{
  return ! (_rx_tail == _rx_head);
}

int16_t input(void)
{
  uint8_t c;

  /* Detect and clear overrun condition */
  if (_rx_flags & (_BV(DOR)|_BV(FE)|RX_FLAG_OVERRUN)) { 
#ifdef SHOW_RX_ERRORS
    outstr_P(PSTR("\n\nsio_rx error:"));
    if (_rx_flags & _BV(DOR)) {
      outstr_P(PSTR(" OR"));
    }
    if (_rx_flags & _BV(FE)) {
      outstr_P(PSTR(" FE"));
    }
    if (_rx_flags & RX_FLAG_OVERRUN) {
      outstr_P(PSTR(" OV"));
    }
    output('\n');
#endif
    interruptsOFF();
    _rx_flags &= ~(_BV(DOR)|_BV(FE)|RX_FLAG_OVERRUN);
    _rx_tail = _rx_head;
    _rx_chars = 0;
    interruptsON();
  }

  FeedCOP; // Reset computer-operating-properly watchdog

  if (_rx_tail == _rx_head) return -1;

  c = *_rx_tail++;

  interruptsOFF();
  _rx_chars--;
#ifdef USE_XON_XOFF
  if ((_rx_flags & RX_FLAG_SUSPENDED) && (_rx_chars <= SIO_RX_XONLEVEL)) {
    _tx_flags |= TX_FLAG_SEND_XON;
    _rx_flags &= ~RX_FLAG_SUSPENDED;

    /* Enable transmit interrupt */
    UCSRB |= _BV(UDRIE);
  }
#endif
  interruptsON();

  if (_rx_tail >= _rx_buf_limit) {
    _rx_tail = _rx_buf_start;
  }

  return (int16_t)((uint16_t)c);
}

#else // USE_INTERRUPTS

uint8_t isinput(void)
{
    return bit_is_set(UCSRA, RXC);
}

/*
 * This function checks for available serial input. If none is available, it
 * returns -1, otherwise it returns the character found in the lower byte
 * with the upper byte set to 0.
 */
int16_t input(void)
{
  FeedCOP; // Reset computer-operating-properly watchdog

  if (bit_is_set(UCSRA, RXC)) {
    uint16_t scdr = (uint16_t)UDR;
#ifdef SIO_CTRL_C_ABORTS
    if (scdr == 0x03) {
      SET_ABORT;
      return -1;
    } else {
      return scdr;
    }
#else
    return scdr;
#endif
  } else {
    return -1;
  }
}

#endif // ! USE_INTERRUPTS

/*
 * This function waits until a character is received, then returns that
 * character.
 */
uint8_t inchar(void)
{
  int16_t c;

  while (((c=input()) == -1) 
#ifdef SIO_CTRL_C_ABORTS
      && ! CHECK_ABORT
#endif
      ) /* NULL */ ;

  return (uint8_t)c;
}

/*
 * This function writes a single character to the serial port.
 */
#ifdef USE_INTERRUPTS
void output(uint8_t c)
{
  uint8_t *newHead = _tx_head+1;

  if (newHead >= _tx_buf_limit) {
    newHead = _tx_buf_start;
  }

  // Block if buffer is now full
  *_tx_head = c;
  do {
    FeedCOP; // Reset the computer-operating-properly watchdog
  } while (newHead == _tx_tail);

  _tx_head = newHead;

  UCSRB |= _BV(UDRIE); /* Enable tx interrupt */
}
#else

void output(uint8_t c)
{
  while (bit_is_clear(UCSRA, UDRE)) {
    FeedCOP; // Reset the computer-operating-properly watchdog
  }

  UDR = c;
}
#endif

/*
 * This function writes a string to the serial port.
 */
void outstr(const char *s)
{
  while (*s) output(*s++);
}

/*
 * This function writes a constant string from the TEXT section to
 * the serial port.
 */
void outstr_P(PGM_P s)
{
  uint8_t c;

  while ((c=pgm_read_byte(s)) != 0) {
    output(c);
    s++;
  }
}
/*
 * This function initializes the serial port.
 */

void sio_init(void)
{
	UBRRH = ((uint16_t)UBRR_VALUE) / 256U;
	UBRRL = ((uint16_t)UBRR_VALUE) % 256U;
    
    // Set single-speed operation, no multi-processor communication mode
    UCSRA = 0;

	// Set frame format: 8 data bits, 1 stop bit, no parity,
    // asynchronous.
	UCSRC = URSEL_BIT | 6;

    /* Clear any reception by turning USART off/on */
    UCSRB = 0;
    
	// Enable receiver and transmitter
	UCSRB = _BV(RXEN)|_BV(TXEN);
    
#ifdef USE_INTERRUPTS
  _rx_head = _rx_tail = _rx_buf_start;
  _rx_flags = 0;

  _tx_head = _tx_tail = _tx_buf_start;
  _tx_flags = 0;

  _rx_chars = 0; // Number of characters in receive buffer

  UCSRB |= _BV(RXCIE); /* Don't enable TIE until characters are written */
#endif
}

/*
 * This function waits for the transmitter to be empty before quitting.
 */

void sio_flush(void)
{
    while (bit_is_clear(UCSRA, TXC)) /* NULL */ ; // Wait for transmit complete
}
// vim: expandtab ts=4
