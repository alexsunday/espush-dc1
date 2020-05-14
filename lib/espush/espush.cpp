#include "espush.h"
#include <assert.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <ESP8266httpUpdate.h>
#include "drv.h"
#include "led.h"
#include "bootstrap.h"

#define BUFFER_SIZE 512
ESPush espush;

ESPush::ESPush():
#if ASYNC_TCP_SSL_ENABLED
_secure(false),
#endif
m_reconnect(true),
m_state(ecs_disconnected),
m_heart(0),
m_heart_state(0),
m_recv(NULL)
{}

ESPush::~ESPush(){}

int ESPush::bootstrap()
{
    m_state = ecs_connecting;

    this->handle_force_firmware_upgrade_frame(NULL);

    String addr;
    int port;
    bool tls;
    
    int rc = get_bootstrap_result(&addr, &port, &tls);    
    if(rc == -1) {
        Serial.println("bootstrap, but response error.");
        m_state = ecs_disconnected;
        m_reconnect = true;
        return -1;
    } else if(rc == -2) {
        Serial.println("bootstrap, failed and disable reconnect.");
        m_state = ecs_disconnected;
        m_reconnect = false;
        return -2;
    } else if(rc == 0) {
        if(addr.equals("") || port == 0) {
            Serial.println("bootstrap, but response empty.");
            m_state = ecs_disconnected;
            m_reconnect = true;
            return -1;
        }
        Serial.println("bootstrap successful.");
    } else {
        Serial.println("bootstrap, but response unknown.");
        m_state = ecs_disconnected;
        m_reconnect = true;
        return -1;
    }

    if(tls) {
#if ASYNC_TCP_SSL_ENABLED
        set_secure(true);
        const uint8_t fingerprint[] = {0x4e, 0xea, 0xcd, 0xee, 0x72, 0x1c, 0xaf, 0x16, 0x78, 0x07, 0x31, 0x56, 0xcf, 0x3c, 0x9e, 0x3a, 0xec, 0x5b, 0x3e, 0x7f};
        add_finger_print(fingerprint);
#else
        // 如果未开启 SSL，且服务器要求使用 SSL,返回错误且不再重连
        Serial.println("cannot support tls now.");
        return -2;
#endif        
    }

    action(addr, port);
    return 0;
}

static void onClientConnect(void* args, AsyncClient* client)
{
    ESPush* e = reinterpret_cast<ESPush*>(args);
    e->onConnect(args, client);
}

static void onClientDisConnect(void* args, AsyncClient* c)
{
    ESPush* e = reinterpret_cast<ESPush*>(args);
    e->onDisConnect(args, c);
}

static void onClientData(void* args, AsyncClient* c, void* data, size_t len)
{
    ESPush* e = reinterpret_cast<ESPush*>(args);
    e->onData(args, c, data, len);
}

static void onClientError(void* args, AsyncClient* c, err_t err)
{
    ESPush* e = reinterpret_cast<ESPush*>(args);
    e->onError(args, c, err);
}

static void onTimer(void* args)
{
    ESPush *e = reinterpret_cast<ESPush*>(args);
    e->onTimer();
}

static void onConnectionCheckTimer(void* args)
{
    ESPush *e = reinterpret_cast<ESPush*>(args);
    e->on_connchk_timer_cb();
}

// just for public
void ESPush::send_frame(Frame* f)
{
    if(m_state != ecs_connected) {
        Serial.println("unconnected, cannot send frame.");
        return;
    }

    write_frame(f);
}

void ESPush::write_frame(Frame* f)
{
    assert(f);
    int len = frame_length(f);

    static char buf[256];
    memset(buf, 0, sizeof(buf));
    len = serialize_frame(f, (uint8_t*)buf, 256);
    int rc = m_client.write(buf, len);
    Serial.printf("write buffer %d\r\n", rc);
    if(rc <= 0) {
        Serial.printf("write frame failed, close connection. %d\r\n", rc);
        m_client.close(true);
    }
    // free(buf);
}

#if ASYNC_TCP_SSL_ENABLED
void ESPush::set_secure(bool secure)
{
    _secure = secure;
}

void ESPush::add_finger_print(const uint8_t* fingerprint)
{
  std::array<uint8_t, SHA1_SIZE> newFingerprint;
  memcpy(newFingerprint.data(), fingerprint, SHA1_SIZE);
  _secure_finger_prints.push_back(newFingerprint);
}

// 证书指纹检查，成功返回0否则失败
int ESPush::finger_check(void)
{
    SSL* client_ssl = m_client.getSSL();
    if(_secure && _secure_finger_prints.size() > 0) {
        for (std::array<uint8_t, SHA1_SIZE> fingerprint : _secure_finger_prints) {
            if(ssl_match_fingerprint(client_ssl, fingerprint.data()) == SSL_OK) {
                return 0;
            }
        }
    }

    return -1;
}
#endif

/*
连接成功，直接发送注册包
*/
void ESPush::onConnect(void* args, AsyncClient* client)
{
    Serial.println("onConnect");

#if ASYNC_TCP_SSL_ENABLED
    if(_secure) {
        int _ssl_check = finger_check();
        if(_ssl_check < 0) {
            Serial.println("ssl check failed.");
            m_client.close(true);
            m_reconnect = false;
            return;
        }
    }
#endif

    // 连接成功时，状态机调整 只有在收到成功后，才能认为已连接到服务器
    m_state = ecs_connecting;
    m_heart = 0;
    m_reconnect = true;
    buffer_reset(m_recv);

    Frame* login = new_login_frame();
    if(!login) {
        Serial.println("new login frame failed.");
        return;
    }
    write_frame(login);
    free_frame(login);
}

/*
设置重连标识符
*/
void ESPush::onDisConnect(void* args, AsyncClient* c)
{
    Serial.println("onDisConnected.");

    m_state = ecs_disconnected;
    m_heart = 0;
    buffer_reset(m_recv);

#if USE_LED
    // 断开连接后，指示灯恢复闪烁
    if(WiFi.isConnected()) {
        set_led_on();
    } else {
        set_led_off();
    }
#endif
    // 定时器会选择是否重连
}

/*
收到数据，加入到环状缓冲区
处理缓冲区数据
*/
void ESPush::onData(void* args, AsyncClient* c, void* data, size_t len)
{
    assert(data);
    assert(len);
    assert(m_recv);
    Serial.println("onData");
    
    int rc = buffer_add_data(m_recv, (uint8_t*)data, len);
    if(rc < 0) {
        Serial.println("buffer full, will close connection");
        m_client.close();
        return;
    }
    
    rc = handle_buffer();
    if(rc < 0) {
        Serial.println("handle buffer failed.");
        // 若服务器向终端发送了未知包，导致客户端处理失败，此处会断开
        m_client.close(true);
        return;
    }
}

int ESPush::handle_buffer()
{
    Serial.println("Handle buffer.");
	Frame* f;
	int rc, length;
	if(buffer_is_full(m_recv)) {
        Serial.println("buffer is Full.");
		return -1;
	}
	
	rc = buffer_try_extract(m_recv);
	if(rc == 0) {
        Serial.println("serial too small, cannot extract frame.");
		return 0;
	}
	length = rc;
	Serial.printf("prepare deserialize frame, left: %d\r\n", length);
	
    // 此处新建 frame，method 指定为0，不会使用，用于反序列化
	f = get_empty_frame(0);
	if(!f) {
		Serial.printf("alloc empty frame on handle buffer failed.\r\n");
		return -1;
	}
	
	// length + 1 ，反序列化时，会在 data 最后一个字节 置 0，所以多传一个字节
	rc = deserialize_frame(m_recv->buffer, length + 1, f);
    // 不管反序列化是否成功，均首先收缩缓冲区
	buffer_shrink(m_recv, length);
	if(rc < 0) {
		Serial.println("deserialize frame failed.");
		free_frame(f);
		return -1;
	}
	
	rc = handle_frame(f);
	if(rc < 0) {
		Serial.println("handle frame failed.");
		free_frame(f);
		return -1;
	}

    free_frame(f);
    return 0;
}

int ESPush::handle_device_auth_response_frame(Frame* f)
{
    Serial.println("device cloud auth response, ignore.");
    if(f->length == 0 || f->data == NULL) {
        Serial.println("device auth resp frame, length empty.");
        return -1;
    }

    uint8_t res = f->data[0];
    if(res != 0) {
        Serial.println("device not in cloud, please regist first.");
        m_reconnect = false;
        m_client.close(true);
        return -1;
    }

    // 此时可认为已连接到服务器
    m_state = ecs_connected;
#if USE_LED
    // LED 指示灯状态变更
    set_led_on();
#endif    
    // 发送设备回路状态
    res = send_states();
    Serial.printf("send states code %d\r\n", res);
    // 发送 dev_info，rssi 等
    return send_dev_info();
}

int ESPush::send_states()
{
    if(m_state != ecs_connected) {
        Serial.println("unconnected, cannot report state.");
        return -1;
    }
    
    Frame* info = new_states_report_frame();
    if(!info) {
        Serial.println("new states report frame failed.");
        return -1;
    }
    write_frame(info);
    free_frame(info);
    return 0;
}

int ESPush::send_dev_info()
{
    Frame* info = new_devinfo_frame();
    if(!info) {
        Serial.println("new devInfo frame failed.");
        return -1;
    }
    write_frame(info);
    free_frame(info);
    return 0;
}

int ESPush::handle_heart_response_frame(Frame* f)
{
    Serial.println("heart response, ignore.");
    on_recv_heart_rsp();
    return 0;
}

int ESPush::handle_proto_query(struct lightproto* in)
{
    assert(in);

    uint32_t result = device_get_lines_state();
    struct lightproto out;
    out.txid = in->txid;
    out.cmd = in->cmd;
    
    union uint32_writer w;
    w.val = htonl(result);
    memcpy(out.data, w.data, sizeof(w));
    return write_light_frame(&out);
}

int ESPush::handle_proto_set_line(struct lightproto* in)
{
    assert(in);
    union uint16_writer w;

    // line
    memcpy(w.data, in->data, 2);
    uint16_t line = ntohs(w.val);

    // state
    memcpy(w.data, in->data + 2, 2);
    uint16_t v = ntohs(w.val);

    bool state = false;
    if(v) {
        state = true;
    }

    int rc = device_set_line_state(line, state);
    if(rc < 0) {
        Serial.printf("device set line state failed. %d\r\n", rc);
        return -1;
    }

    return write_light_frame(in);
}

int ESPush::handle_proto_set_batch(struct lightproto* in)
{
    assert(in);
    // state
    union uint16_writer w;
    memcpy(w.data, in->data, 2);
    uint16_t state = ntohs(w.val);

    bool v = false;
    if(state) {
        v = true;
    }

    device_set_batch_state(v);
    return write_light_frame(in);
}

int ESPush::write_light_frame(struct lightproto* in)
{
    assert(in);
    static uint8_t uart_buf[8];
    serialize_lightproto_frame(in, uart_buf);

    Frame *f = get_empty_frame(method_uart);
    if(!f) {
        Serial.println("alloc empty frame failed");
        return -1;
    }
        
    set_static_data(f, uart_buf, sizeof(uart_buf));
    write_frame(f);
    free_frame(f);
    return 0;
}

int ESPush::handle_uart_transport_frame(Frame* f)
{
    Serial.println("uart data.");

    int rc = 0;
    size_t length = f->length;
    if(length != 8) {
        Serial.println("light proto frame length must equal to 8.");
        return -1;
    }
    assert(f->data);

    struct lightproto in;
    deserialize_lightproto_frame(f->data, &in);
    if(in.cmd == LIGHT_QUERY) {
        rc = handle_proto_query(&in);
    } else if(in.cmd == LIGHT_SET_LINE) {
        rc = handle_proto_set_line(&in);
    } else if(in.cmd == LIGHT_SET_BATCH) {
        rc = handle_proto_set_batch(&in);
    } else {
        Serial.println("unknown uart cmd. ");
        rc = -1;
    }

    return rc;
}

int ESPush::handle_force_reboot_frame(Frame* f)
{
    Serial.println("force reboot, ignore.");
    ESP.restart();
    return 0;
}

int ESPush::handle_server_probe_frame(Frame* f)
{
    Serial.println("server probe.");
    return send_heart_frame();
}

void ESPush::handle_ota()
{
    String addr;
    int port;
    bool tls;
    int rc = get_bootstrap_result(&addr, &port, &tls);
    Serial.printf("rc: %d\r\n", rc);
}

int ESPush::handle_force_firmware_upgrade_frame(Frame* f)
{
    Serial.println("force firmware upgrade.");

    /*
    curl  -X POST 'https://light.espush.cn/api/portal/light/proto/devices/116/upgrade' -H 'authorization: USER 13726734050 e353f93ecdba979ecafff1b97b884dc5' -H 'accept-encoding: gzip, deflate, br'  -H 'user-agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/74.0.3729.131 Safari/537.36' -H 'accept: application/json, text/plain'
    */
    WiFiClient client;
    HTTPUpdateResult result = ESPhttpUpdate.update(client, "http://light.espush.cn/api/portal/esp8266/firmware/upgrade", ESPUSH_FIRMWARE_VERSION);
    if(result != HTTP_UPDATE_OK) {
        Serial.printf("firmware upgrade result: %d\r\n", result);
    }
    return 0;
}

int ESPush::handle_frame(Frame* f)
{
	int rc = 0;

	Serial.println("handle frame.");
	switch (f->method)
	{
    case mle_authorization:
        rc = handle_device_auth_response_frame(f);
        break;
	case mle_heart_response:
		rc = handle_heart_response_frame(f);
		break;
	case mle_firmware_upgrade:
		rc = handle_force_firmware_upgrade_frame(f);
		break;
	case mle_uart_transport:
		rc = handle_uart_transport_frame(f);
		break;
	case mle_force_reboot:
		rc = handle_force_reboot_frame(f);
		break;
	case mle_server_probe:
		rc = handle_server_probe_frame(f);
		break;
	default:
        rc = -1;
		Serial.println("unknown method, ignore.");
		break;
	}

	return rc;
}

/*
设置重连标识符
close?
*/
void ESPush::onError(void* args, AsyncClient* c, err_t e)
{
    Serial.println("onError.");
    m_client.close(true);
}

/*
init 执行手动初始化，系统启动时未必能执行，必须放在 init 里的动作
timer 初始化
环状缓冲区初始化
*/
void ESPush::init()
{
    os_timer_disarm(&m_timer);
    os_timer_setfn(&m_timer, ::onTimer, this);
    os_timer_arm(&m_timer, 1000, true);

    WiFi.hostname("ZhiYun-DC1");
    static Buffer buffer;
    m_recv = new_buffer(&buffer);
}

/*
action => 开始
1，初始化成员变量
2，设置回调函数
*/
void ESPush::action(String &host, int port)
{
    Serial.println("action ing...");
    m_client.onConnect(onClientConnect, this);
    m_client.onData(onClientData, this);
    m_client.onDisconnect(onClientDisConnect, this);
    m_client.onError(onClientError, this);
    
#if ASYNC_TCP_SSL_ENABLED
    m_client.connect(host.c_str(), port, _secure);
#else
    m_client.connect(host.c_str(), port);
#endif
    m_reconnect = true;
    m_state = ecs_connecting;
}

/*
    1，关闭心跳定时器
    2，发送心跳请求，开启心跳检查定时器，不重复
    3，置 m_heart_state 为1
*/
int ESPush::send_heart_frame()
{
    os_timer_disarm(&m_connection_check_timer);

	static char heart_buf[] = {0, 3, 0, 0, 0xFF};
    int rc = m_client.write(heart_buf, sizeof(heart_buf));
    if(rc <= 0) {
        Serial.println("write failed, need to close.");
        m_client.close(true);
        return rc;
    }

    m_heart_state = 1;
    os_timer_setfn(&m_connection_check_timer, onConnectionCheckTimer, this);
    os_timer_arm(&m_connection_check_timer, 10 * 1000, 0);

    return 0;
}

void ESPush::set_reconnect(bool r)
{
    m_reconnect = r;
}

void ESPush::close()
{
    m_client.close(true);
}

// 10秒后，检查若 m_heart_state，若为1，则认为服务器未回复
void ESPush::on_connchk_timer_cb(void)
{
    if(m_heart_state == 1) {
        Serial.println("connection recv heart response timeout, closed it!");
        m_client.close(true);
    }
}

// 若收到心跳回复，置 m_heart_state 为0
void ESPush::on_recv_heart_rsp(void)
{
    m_heart_state = 0;
}

void show_free_mem()
{
    uint32_t left_stack = ESP.getFreeContStack();
    uint32_t free_heap = ESP.getFreeHeap();
    uint32_t free_space = ESP.getFreeSketchSpace();
    Serial.printf("stack: %d, heap: %d, space: %d\r\n", left_stack, free_heap, free_space);
}

/*
每秒钟被执行，检查状态，执行重连等，由 m_timer 驱动
检查是否要发送心跳
*/
void ESPush::onTimer()
{
    // 如果已连接到服务器，则心跳计数器++
    if(m_state == ecs_connected) {
        m_heart ++;
        // 如果心跳计数器 > 30 则发送心跳，计数器置0
        if(m_heart >= 30) {
            m_heart = 0;
            send_heart_frame();
            show_free_mem();
        }
    }
}

/*
 loop 函数，已比较快的频率，运行在 arduino 架构中
检查 wifi 连接状态
检查 cloud 连接状态
*/
void ESPush::loop()
{
    int rc = 0;
    // Serial.println("loop timer!");
    // 仅在 m_client tcp 状态机为断开，且 本地状态机不是 连接中，且 允许重连时，重连
    // 还需要加一个条件，需要为 STA 模式，且已连接 时，才进行
    if((WiFi.getMode() & WIFI_STA) && WiFi.status() == WL_CONNECTED) {
        // need check
    } else {
        return;
    }
    
    if(m_client.disconnected() && m_state != ecs_connecting && m_reconnect == true) {
        Serial.println("prepare reconnect to server...");
        rc = bootstrap();
        if(rc < 0) {
            // delay(1000);
        }
        return;
    }

    // 如果已断链，且不重连，输出日志
    if(m_client.disconnected()) {
        // Serial.println("espush cloud connection disconnected");
        // delay(100);
    }
}

/*
断链bug排查方式
1，定时输出 剩余内存 信息，观察内存是否有变动
2，使用工具长期插着设备，观察并分析串口日志
3，

配网：
1，解决启动时由于连不上AP，处于AP状态下，超时未生效的问题
2，portal 界面要自定义，接口要自定义

固件TODO:
1，解决配网可用性，以及配网安全性
7，透传固件

以下问题可能已解决，测试中
6，必须解决掉线问题

以下问题可能已解决，待测：
2，系统可最多记录 5 组 AP 信息，需要测试换AP时系统的表现
8，工作指示灯
5，服务器更新 FOTA，需要生产可用
4，局域网本地模式
*/
