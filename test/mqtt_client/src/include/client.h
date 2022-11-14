#ifndef CLIENT_H
#define CLIENT_H

#include "NetCommon/net_client.h"
#include "mqtt.h"
#include "core.h"

#define CONNECT_BYTE 0x10
#define SUBREQ_BYTE  0x80
#define UNSUB_BYTE   0xA0
#define PINGREQ_BYTE 0xC0
#define DISC_BYTE    0xE0

enum {CLEAN_SESSION_TRUE, CLEAN_SESSION_FALSE};

class MQTTClient: public tps::net::client_interface<mqtt_header>
{
public:
    ~MQTTClient() {if (is_connected()){ disconnect();}}

    tps::net::message<mqtt_header> pack_connect(mqtt_connect& con);
    void pack_subscribe(mqtt_subscribe& sub, tps::net::message<mqtt_header>& msg);
    void pack_unsubscribe(mqtt_unsubscribe& unsub, tps::net::message<mqtt_header>& msg);

    mqtt_publish unpack_publish(tps::net::message<mqtt_header> &msg);
    mqtt_connack unpack_connack(tps::net::message<mqtt_header> &msg);
    mqtt_suback unpack_suback(tps::net::message<mqtt_header>& msg);
    mqtt_ack unpack_ack(tps::net::message<mqtt_header>& msg);

    void connect_server(const std::string& clientID, uint8_t cleanSession = 1);
    uint16_t publish(std::string& topic, std::string& msg, uint8_t qos, uint8_t retain=0);
    uint16_t subscribe(std::vector<std::pair<std::string, uint8_t> > &topics);
    void unsubscribe();
    void disconnect();
    void send_ack(packet_type type, uint16_t pktID);
    void pingreq();
};


#endif // CLIENT_H
