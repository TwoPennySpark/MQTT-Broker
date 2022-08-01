#ifndef SERVER_H
#define SERVER_H

#include "NetCommon/net_server.h"
#include "mqtt.h"
#include "core.h"

class server: public tps::net::server_interface<mqtt_header>
{
public:
    server(uint16_t port): tps::net::server_interface<mqtt_header>(port) {}

protected:
    virtual bool on_client_connect(std::shared_ptr<tps::net::connection<mqtt_header>> client);
    virtual void on_client_disconnect(std::shared_ptr<tps::net::connection<mqtt_header>> client);
    virtual bool on_first_message(std::shared_ptr<tps::net::connection<mqtt_header>> netClient,
                                  tps::net::message<mqtt_header>& msg);
    virtual void on_message(std::shared_ptr<tps::net::connection<mqtt_header>> netClient,
                            tps::net::message<mqtt_header>& msg);
private:
    void handle_connect(std::shared_ptr<tps::net::connection<mqtt_header>>& netClient,
                        mqtt_connect& pkt);
    void handle_subscribe(std::shared_ptr<tps::net::connection<mqtt_header>>& netClient,
                        mqtt_subscribe& pkt);
    void handle_unsubscribe(std::shared_ptr<tps::net::connection<mqtt_header>>& netClient,
                        mqtt_unsubscribe& pkt);
    void handle_publish(std::shared_ptr<tps::net::connection<mqtt_header>>& netClient,
                        mqtt_publish& pkt);
    void handle_disconnect(std::shared_ptr<tps::net::connection<mqtt_header>>& netClient);
    void handle_pingreq(std::shared_ptr<tps::net::connection<mqtt_header>>& netClient);

    struct core m_core;
};

#endif // SERVER_H
