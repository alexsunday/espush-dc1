#include <Arduino.h>
#include "board.h"
#include "drv.h"

#ifdef BOARD_TYPE_CD4067

#define LINES_COUNT 16
// D5, D6, D7, D8  D4, D3, D2, D1
// uint8_t internal_drv[] = {14, 12, 13, 15, 2, 0, 4, 5};
// uint8_t internal_drv[] = {14, 12, 13, 15};
uint8_t internal_drv[] = {15, 13, 12, 14};
int device_io_init(void)
{
  // init D1 ~ D8 as output
  int size = sizeof(internal_drv) / sizeof(uint8_t);
  for(int i=0; i!=size; ++i) {
    pinMode(internal_drv[i], OUTPUT);
  }
  return 0;
}

// line 从 1 起，此处 state 无意义
int device_set_line_state(uint8_t line, bool state)
{
  uint8_t pinmap = line - 1;
  int size = sizeof(internal_drv) / sizeof(uint8_t);
  for(int i=0; i!=size; ++i) {
    digitalWrite(internal_drv[i], (pinmap & (1 << i)) >> i);
  }
  return 0;
}

// 获取总回路数
int device_get_lines(void)
{
  return LINES_COUNT;
}

uint32_t device_get_lines_state(void)
{
  return 0;
}

// 全开全关
void device_set_batch_state(bool state)
{
  //
}

void device_loop()
{
  
}
#endif
