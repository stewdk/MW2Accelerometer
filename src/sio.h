#ifndef _SIO_H_
#define _SIO_H_

#include <avr/pgmspace.h>

extern void sio_init(void);
extern uint8_t isinput(void);
extern int16_t input(void);
extern uint8_t inchar(void);
extern void output(uint8_t c);
extern void outstr(const char *s);
extern void outstr_P(PGM_P s);
extern void sio_flush(void);

/* We include these in case we have a chip with two USART's and two
 * instantiations of the serial driver are created, one for each USART (see the
 * documentation for USE_USART_NUMBER in the sio.c file). There's no harm in
 * declaring these external functions and not instantiating them. It does
 * pollute the namespace a bit, but it's an easy thing to do.
 */
extern void sio_init0(void);
extern uint8_t isinput0(void);
extern int16_t input0(void);
extern uint8_t inchar0(void);
extern void output0(uint8_t c);
extern void outstr0(const char *s);
extern void outstr_P0(PGM_P s);
extern void sio_flush0(void);

extern void sio_init1(void);
extern uint8_t isinput1(void);
extern int16_t input1(void);
extern uint8_t inchar1(void);
extern void output1(uint8_t c);
extern void outstr1(const char *s);
extern void outstr_P1(PGM_P s);
extern void sio_flush1(void);

#endif /* _SIO_H_ */
// vim: expandtab
