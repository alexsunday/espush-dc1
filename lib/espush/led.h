#ifndef _GUARD_H_LED_H_
#define _GUARD_H_LED_H_

#include <Arduino.h>
#include "board.h"

#if USE_LED
void led_init(void);
void set_led_off(void);
void set_led_on(void);
int get_led_status(void);
void set_led_toggle(void);
void set_led_blink_fast(void);
void set_led_blink_slow(void);
#endif  //USE_LED == 1

#endif
