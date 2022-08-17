#ifndef SERVER_H
#define SERVER_H

#include "NetCommon/net_server.h"
#include "mqtt.h"
#include "core.h"

class server: public tps::net::server_interface<mqtt_header>
{
public:
    server(uint16_t port): tps::net::server_interface<mqtt_header>(port) {}
    virtual ~server() override {}

protected:
    virtual bool on_client_connect    (pConnection client) override;
    virtual void on_client_disconnect (pConnection client) override;
    virtual bool on_first_message     (pConnection netClient,
                                       tps::net::message<mqtt_header>& msg) override;

    virtual void on_message(pConnection netClient,
                            tps::net::message<mqtt_header>& msg) override;

private:
    void handle_connect     (pConnection& netClient, mqtt_connect& pkt);

    void handle_subscribe   (pClient& client, mqtt_subscribe& pkt);
    void handle_unsubscribe (pClient& client, mqtt_unsubscribe& pkt);
    void handle_publish     (pClient& client, mqtt_publish& pkt);
    void publish_msg        (mqtt_publish& pkt);

    void handle_puback (pClient& client, mqtt_puback& pkt);
    void handle_pubrec (pClient& client, mqtt_pubrec& pkt);
    void handle_pubrel (pClient& client, mqtt_pubrel& pkt);
    void handle_pubcomp(pClient& client, mqtt_pubcomp& pkt);
    void handle_pingreq(pClient& client);

    // deletes client data from core based on client's clean session parameter
    // also, depending on the state of the flags, performs will publishing
    // or/and network disconnection of the client
    enum disconnect_flags
    {
        NONE         = 0,
        DISCONNECT   = 1 << 0,
        PUBLISH_WILL = 1 << 1,
    };
    void disconnect(pClient& client, uint8_t flags,
                    uint8_t manualControl = core_t::BASED_ON_CS_PARAM);

    struct core m_core;
};

#endif // SERVER_H
