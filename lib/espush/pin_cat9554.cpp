
#include "board.h"
#include "drv.h"
#include <FS.h>

#ifdef BOARD_TYPE_DC1

#include "CSE7766.h"
#include "espush.h"
#include "CAT9554.h"
#include "smemory.h"
#include "frame.h"


// 看你的继电器是连接那个io，默认gpio0
const int logLed = 14;
const int wifiLed = 0;

// cat9554 连接的 3 个继电器地址分别为如下
const static uint8_t gl_lines[] = {6, 5, 4, 7};
const static uint8_t gl_buttons[] = {2, 1, 0};
static int8_t states[] = {1, 1, 1, 1};

void electric_report(void* args)
{
  String message;
  message = "{\"vol\":\""+String(myCSE7766.getVoltage())+
  "\",\"cur\":\""+String(myCSE7766.getCurrent())+
  "\",\"dt\":\"" + String("dc1") +
  "\",\"typ\":\"" + String("e01") +
  "\",\"p1\":\""+String(myCSE7766.getActivePower())+
  "\",\"p2\":\""+String(myCSE7766.getApparentPower())+
  "\",\"p3\":\""+String(myCSE7766.getReactivePower())+
  "\",\"pf\":\""+String(myCSE7766.getPowerFactor())+
  "\",\"e\":\""+String(myCSE7766.getEnergy())+
  "\"}";

  myCSE7766.resetEnergyValue();
  Frame* frame = new_electric_frame(message);
  if(!frame) {
    return;
  }

  espush.send_frame(frame);
  free_frame(frame);
}

void electric_report_init(void)
{
  static os_timer_t electric_timer;
  os_timer_disarm(&electric_timer);
  os_timer_setfn(&electric_timer, electric_report, NULL);
  os_timer_arm(&electric_timer, 1000 * 60 * 10, true);
}

int device_io_init(void)
{
  myCSE7766.setRX(13);
  myCSE7766.begin();

  cat9554.setup();
  cat9554.pinMode(0, INPUT);
  cat9554.pinMode(1, INPUT);
  cat9554.pinMode(2, INPUT);

  // 继电器
  cat9554.pinMode(4, OUTPUT);
  cat9554.pinMode(5, OUTPUT);
  cat9554.pinMode(6, OUTPUT);
  cat9554.pinMode(7, OUTPUT);

  // 根据 flash 加载的状态，修正继电器
  init_states(load_states_via_flash());

  // 打开 LED 灯
  pinMode(wifiLed, OUTPUT);
  pinMode(logLed, OUTPUT);
  digitalWrite(logLed, LOW);

  // 定时上报电气参数
  electric_report_init();

  return 0;
}

// idx 从 1 开始
void line_pin(int idx, bool mode)
{
  int8_t state;
  int size = sizeof(gl_lines) / sizeof(uint8_t);
  if(idx > size) {
    Serial.println("too large line number");
    return;
  }

  int pin = gl_lines[idx - 1];
  cat9554.digitalWrite(pin, mode);

  if(mode) {
    state = 1;
  } else {
    state = 0;
  }
  states[idx - 1] = state;

  if(pin == 7) {
    // 总开关，应顺带把 LED 也设置下
    if(mode) {
      digitalWrite(wifiLed, LOW);
    } else {
      digitalWrite(wifiLed, HIGH);
    }
  }
}

/*
总共有4个回路，line 从 1 起，分别为 1，2，3，4
*/
int device_set_line_state(uint8_t line, bool state)
{
  line_pin(line, state);

  // 如果是 打开前三路，且总开关被关闭，则需要顺带把总开关也打开
  if(line <= 3 && state && states[3] == 0) {
    line_pin(4, HIGH);
  }

  save_states_to_flash();
  return 0;
}

uint32_t get_lines_state_by_sm(void)
{
  uint32_t res = 0;
  size_t len = sizeof(gl_lines) / sizeof(uint8_t);

  for(size_t i=0; i!=len; ++i) {
    res |= states[i] << i;
  }

  return res;
}

uint32_t device_get_lines_state(void)
{
    // return get_lines_state_by_i2c();
    return get_lines_state_by_sm();
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

void device_batch_toggle()
{
  int8_t all_state = 0;
  int lines = device_get_lines();
  
  for(int i=0; i!=lines; ++i) {
    if(states[i]) {
      all_state = 1;
    }
  }

  if(all_state == 1) {
    device_set_batch_state(false);
    all_state = 0;
  } else if(all_state == 0) {
    device_set_batch_state(true);
    all_state = 1;
  }

    // 触发上送
  espush.send_states();
}

// 获取总回路数, D1 有4个独立可控制回路
int device_get_lines(void)
{
    return sizeof(gl_lines) / sizeof(uint8_t);
}

/*
0 => 6
1 => 5
2 => 4
*/
void switchRelay(int ch)
{
  int8_t state = !states[ch];
  device_set_line_state(ch + 1, state == 1);

  states[ch] = state;

  // 触发上送
  espush.send_states();
}

uint8_t btn_short_time = 50;
uint16_t btn_long_time = 2000; // 2000 = 2s
uint8_t btn_timing = 0;
unsigned long btn_timing_start[4] = {0, 0, 0, 0};
uint8_t btn_actions[4] = {0, 0, 0, 0}; // 0 = 没有要执行的动作, 1 = 执行短按动作, 2 = 执行长按动作
void btn_check(int ch)
{
  bool state = cat9554.digitalRead(ch);
  if(state == 0) {
    // begin press.
    if(!bitRead(btn_timing, ch)) {
      bitSet(btn_timing, ch);
      btn_timing_start[ch] = millis();
    } else {
      if(millis() >= (btn_timing_start[ch] + btn_short_time)) {
        btn_actions[ch] = 1;
      }
      if(millis() >= (btn_timing_start[ch] + btn_long_time)) {
        btn_actions[ch] = 2;
      }
    }
  } else {
    // end press.
    bitClear(btn_timing, ch);
    if(btn_actions[ch] != 0) {
      if(btn_actions[ch] == 1) {
        // short press.
        switchRelay(ch);
      }
      btn_actions[ch] = 0;
    }
  }
}

void device_loop()
{
  btn_check(0);
  btn_check(1);
  btn_check(2);
}

#endif
