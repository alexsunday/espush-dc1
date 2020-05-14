#ifndef  _GUARD_H_LIGHT_DRIVER_H_
#define  _GUARD_H_LIGHT_DRIVER_H_

#include <stdint.h>

/*
照明设备、开关设备驱动
1，对某个回路执行 开/关
2，查询所有回路状态
3，批量执行 开、关 操作
4，【可选】调光查询与设置
5，【可选】设置默认状态

line 从1起，如4路则可取值 1，2，3，4
*/

int device_io_init(void);

// 记住 line 自 1 起
int device_set_line_state(uint8_t line, bool state);

// 获取总回路数
int device_get_lines(void);

uint32_t device_get_lines_state(void);

// 全开全关
void device_set_batch_state(bool state);

// 部分硬件设备可能需要执行 loop
void device_loop();

#endif
