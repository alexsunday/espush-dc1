#ifndef _GUARD_H_PROTO_FRAME_H_
#define _GUARD_H_PROTO_FRAME_H_

#include <stdint.h>
#include <Arduino.h>

#define FRAME_STATIC_DATA 0
#define FRAME_DYN_DATA 1

#define method_login 0x01
#define method_devinfo 0x02
#define method_uart  0x03
#define cs_connected 1
#define cs_unconnected 0
#define cs_connecting 2

#define MAX_RECV_BUFFER_LENGTH 256

#define LIGHT_QUERY 0x01
#define LIGHT_SET_LINE 0x02
#define LIGHT_SET_BATCH 0x03

/*
data_flag special data field which is static or alloc data.
*/
typedef struct {
	uint8_t method;
	uint8_t txid;
    int8_t data_flag;
	uint8_t* data;
	size_t length;
} Frame ;

Frame* get_empty_frame(uint8_t method);
Frame* new_login_frame();
Frame* new_devinfo_frame(void);
Frame* new_states_report_frame(void);
Frame* new_electric_frame(const String &body);

int frame_length(Frame *f);

void set_dyn_data(Frame *f, uint8_t* data, size_t d_length);
void set_static_data(Frame *f, uint8_t* data, size_t d_length);

void free_frame(Frame *f);

int write_frame(int fd, Frame* f);
int recv_frame(int fd, Frame *f);

int serialize_frame(Frame *f, uint8_t* out, size_t out_max_length);
void serialize_f(Frame *f, uint8_t* out);

int deserialize_frame(uint8_t* in_data, size_t in_length, Frame *out);

typedef struct {
	uint8_t* buffer;
	size_t len;
	size_t cap;
} Buffer;


union uint16_writer {
	uint16_t val;
	uint8_t  data[2];
};

union uint32_writer {
	uint32_t val;
	uint8_t  data[4];
};

Buffer* new_buffer(Buffer* out);
void free_buffer(Buffer* buf);

int buffer_is_full(Buffer* buf);

// 返回 -1 代表已满，出错，该重置，返回 > 0 为当前 buffer 的length
int buffer_add_data(Buffer* buf, uint8_t* ptr, size_t len);

// 返回 0 代表无法取出，返回 > 0 代表 若干字节可以组装一个 frame
int buffer_try_extract(Buffer* buf);

// 将 前 len 个字节收缩掉
void buffer_shrink(Buffer* buf, size_t len);

// 重置 buffer 状态机
void buffer_reset(Buffer* buf);

/*
0x80	服务器响应心跳数据包，回复限定5 秒内
0x81	服务器回应 认证报文
0x82	服务器发送 执行升级 指令
0x83	服务器发送 透传数据
0x84	服务器发送 强制重启 指令
0x85    服务器发送 主动探测 指令
*/
enum method_list_enum {
	mle_heart_response = 0x80,
	mle_authorization = 0x81,
	mle_firmware_upgrade = 0x82,
	mle_uart_transport = 0x83,
	mle_force_reboot = 0x84,
	mle_server_probe = 0x85,
};

struct lightproto {
    uint16_t txid;
    uint16_t cmd;
    char data[4];
};

void deserialize_lightproto_frame(uint8_t* data, struct lightproto* out);
void serialize_lightproto_frame(struct lightproto* in, uint8_t* out);

#endif
