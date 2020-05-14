#ifndef _GUARD_H_STATES_MEMORY_H_
#define _GUARD_H_STATES_MEMORY_H_

#include <Arduino.h>
/*
开关状态记忆功能，会把状态记住到flash里
存4字节，即 32 位，足够记32个回路了
*/

uint32_t load_states_via_flash(void);
void init_states(uint32_t res);
void save_states_to_flash();

#endif
