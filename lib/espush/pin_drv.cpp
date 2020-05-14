#include <Arduino.h>
#include "drv.h"
#include "board.h"
#include "smemory.h"

#ifdef BOARD_TYPE_GPIO
// 使用 GPIO 直接控制 开关继电器

// 开发板只做两路
#ifdef ESP_BOARD_DIFI
int gl_lines[] = {13, 14};
#endif

#ifdef ESP_BOARD_ESP01
int gl_lines[] = {0};
#endif

#ifdef ESP_BOARD_12F_USER1
int gl_lines[] = {5, 4, 14, 12};
#endif

#ifdef ESP_BOARD_NODEMCU
// int gl_lines[] = {0, 2, 4, 5, 10, 12, 13, 14, 15, 16};
int gl_lines[] = {5, 4, 14, 12, 13, 15};
#endif

// 默认高电平触发 可通过宏定义切换为低电平触发
#define PIN_ON_EDGE LOW


void pin_line_set_on(int pin)
{
  digitalWrite(pin, PIN_ON_EDGE);
}

void pin_line_set_off(int pin)
{
  digitalWrite(pin, ! PIN_ON_EDGE);
}

int device_io_init(void)
{
  int size = device_get_lines();
  for(int i=0; i!=size; ++i) {
    pinMode(gl_lines[i], OUTPUT);
  }

    // 根据 flash 加载的状态，修正继电器
  init_states(load_states_via_flash());

  return 0;
}

int device_set_line_state(uint8_t line, bool state)
{
  int pin = gl_lines[line-1];
  if(state) {
    pin_line_set_on(pin);
  } else {
    pin_line_set_off(pin);
  }

  //  save_states_to_flash();
  return 0;
}

uint32_t device_get_lines_state(void)
{
  uint32_t res = 0;
  
  size_t len = sizeof(gl_lines) / sizeof(int);
  for(size_t i=0; i!= len; ++i) {
    int cur = 0;
    int edge = digitalRead(gl_lines[i]);
    if(edge==PIN_ON_EDGE) {
      cur = 1;
    }

    res |= cur << i;
  }
  
  return res;
}

// batch set state
void device_set_batch_state(bool state)
{
  int i;
  int lines = device_get_lines();
  for(i=0; i!=lines; ++i) {
    device_set_line_state(i + 1, state);
  }
}

int device_get_lines(void)
{
    return sizeof(gl_lines) / sizeof(int);
}

void device_loop()
{
  
}

#endif
