#ifndef _GUARD_H_NETWORK_CONFIG_H_
#define _GUARD_H_NETWORK_CONFIG_H_

/*
实现网络配置，几种途径：
1，按钮按下
2，快速上电/掉电 N 次，需要使用 Flash
*/

void netconfig_init(void);
void netconfig_loop(void);
void setup_ap_config(void);

#endif
