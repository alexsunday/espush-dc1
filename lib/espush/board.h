#ifndef _GUARD_H_BOARD_FUTURE_H_
#define _GUARD_H_BOARD_FUTURE_H_

// #define BOARD_TYPE_DC1
#define BOARD_TYPE_GPIO
// #define BOARD_TYPE_CD4067

#ifdef BOARD_TYPE_GPIO
// #define ESP_BOARD_DIFI
// #define ESP_BOARD_ESP01
// #define ESP_BOARD_12F_USER1
#define ESP_BOARD_NODEMCU
#endif

// LED
#define USE_LED 1
#if USE_LED
// LED ，如果是01，则只能取 GPIO5
#ifdef ESP_BOARD_ESP01
#define LED_PIN 2
#endif

// 开发板，取12 引脚
#ifdef ESP_BOARD_DIFI
#define LED_PIN 12
#endif

#ifdef ESP_BOARD_12F_USER1
#define LED_PIN 5
#endif

#ifdef BOARD_TYPE_DC1
// 使用 logo 灯
#define LED_PIN 14
#endif

#ifdef ESP_BOARD_NODEMCU
// 使用 logo 灯
#define LED_PIN 2
#endif

#define LED_ON_STATE LOW
#define LED_OFF_STATE HIGH
#endif // USE_LED

// #define NETCONFIG_USE_FLASH

#define NETCONFIG_USE_BUTTON

// #define NETCONFIG_USE_SMARTCONFIG
#define NETCONFIG_USE_WEB_AP

#ifdef NETCONFIG_USE_BUTTON

#ifdef BOARD_TYPE_DC1
#define NETCONFIG_BUTTON_GPIO 16
#endif

#ifdef ESP_BOARD_NODEMCU
#define NETCONFIG_BUTTON_GPIO 0
#endif

#endif


void board_setup(void);
void board_loop(void);
void show_imei(void);

#endif
