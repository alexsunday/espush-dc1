#ifndef _GUARD_H_ESPUSH_H_
#define _GUARD_H_ESPUSH_H_

#include <Arduino.h>
#include <WString.h>
#include <ESPAsyncTCP.h>

extern "C" {
#include <osapi.h>
#include <os_type.h>
}

#include "frame.h"

#define ESPUSH_FIRMWARE_VERSION "1.1.2"

#if ASYNC_TCP_SSL_ENABLED
#include <tcp_axtls.h>
#define SHA1_SIZE 20
#endif


enum connection_state {
    ecs_disconnected,
    ecs_connecting,
    ecs_bootstrap,
    ecs_connected,
};

class ESPush {
public:
    ESPush();
    ~ESPush();
    int bootstrap();
    void init();
    void loop();
    void onTimer();
    void action(String &host, int port);
    void handle_ota();
    int send_states();

    // for public
    void send_frame(Frame* f);

#if ASYNC_TCP_SSL_ENABLED
    bool _secure;
    std::vector<std::array<uint8_t, SHA1_SIZE>> _secure_finger_prints;
    int finger_check(void);
    void add_finger_print(const uint8_t* fingerprint);
    void set_secure(bool secure);
#endif

protected:
    int write_light_frame(struct lightproto* f);
    void write_frame(Frame* f);
    int handle_buffer();
    int handle_frame(Frame* f);

    int send_dev_info();
private:
    // handle espush v3 protocol.
    int handle_heart_response_frame(Frame* f);
    int handle_device_auth_response_frame(Frame* f);
    int handle_uart_transport_frame(Frame* f);
    int handle_force_reboot_frame(Frame* f);
    int handle_server_probe_frame(Frame* f);
    int handle_force_firmware_upgrade_frame(Frame* f);
    // handle light protocol.
    int handle_proto_query(struct lightproto* in);
    int handle_proto_set_line(struct lightproto* in);
    int handle_proto_set_batch(struct lightproto* in);

    // 心跳数据包处理逻辑
    /*
    1，关闭心跳定时器
    2，发送心跳请求，开启心跳检查定时器，不重复
    3，置 m_heart_state 为1，启动10秒定时器

    若收到心跳回复，置 m_heart_state 为0
    10秒后，检查若 m_heart_state，若为1，则认为服务器未回复
    */
    int send_heart_frame();
    void on_recv_heart_rsp(void);
    os_timer_t m_connection_check_timer;
public:
    void on_connchk_timer_cb(void);
    void set_reconnect(bool r);
    void close();

public:
    void onConnect(void* args, AsyncClient* client);
    void onDisConnect(void* args, AsyncClient* c);
    void onData(void* args, AsyncClient* c, void* data, size_t len);
    void onError(void* args, AsyncClient* c, err_t e);
private:
    AsyncClient m_client;
    bool m_reconnect;
    enum connection_state m_state;

    // 此定时器固定每秒触发一次，用于重连，同时也用于心跳
    os_timer_t m_timer;
    // heart counter
    uint8_t m_heart;
    // 心跳状态机，发送心跳则+1
    // 收到心跳回复则-1，若为0则维持不变
    uint8_t m_heart_state;

    // recv buffer
    Buffer* m_recv;
};

extern ESPush espush;
#endif
