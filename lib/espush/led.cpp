#include <Arduino.h>
#include <Ticker.h>

#include "led.h"

#if USE_LED
static Ticker ticker;

void led_init(void)
{
  pinMode(LED_PIN, OUTPUT);
}

void set_led_off(void)
{
  ticker.detach();
  digitalWrite(LED_PIN, LED_OFF_STATE);
}

void set_led_on(void)
{
  ticker.detach();
  digitalWrite(LED_PIN, LED_ON_STATE);
}

int get_led_status(void)
{
  return digitalRead(LED_PIN);
}

void set_led_toggle(void)
{
  int state = digitalRead(LED_PIN);  // get the current state of GPIO1 pin
  digitalWrite(LED_PIN, !state);
}

void set_led_blink_fast(void)
{
  ticker.detach();
  ticker.attach(0.3, set_led_toggle);
}

void set_led_blink_slow(void)
{
  ticker.detach();
  ticker.attach(1, set_led_toggle);
}

#endif   // USE_LED===1
