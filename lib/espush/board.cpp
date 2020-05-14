#include <Arduino.h>

#include "board.h"
#include "drv.h"

#ifdef BOARD_TYPE_DC1
#include <CSE7766.h>
#endif

// 板载功能初始化
void board_setup(void)
{
  int rc = device_io_init();
  Serial.printf("device init result: %d\r\n", rc);
}

void board_loop(void)
{
  // DC1 有一些外设需要执行 loop
#ifdef BOARD_TYPE_DC1
  myCSE7766.handle();
  device_loop();
#endif
}
