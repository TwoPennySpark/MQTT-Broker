#ifndef SERVER_H
#define SERVER_H

#include "NetCommon/net_server.h"
#include "mqtt.h"
#include "core.h"

template <typename T>
class server: public tps::net::server_interface<T>
{
public:
    server(uint16_t port): tps::net::server_interface<T>(port) {}

    virtual bool on_client_connect(std::shared_ptr<tps::net::connection<T>> client)
    {
        return true;
    }

    virtual void on_client_disconnect(std::shared_ptr<tps::net::connection<T>> client)
    {

    }

    virtual void on_message(std::shared_ptr<tps::net::connection<T>> client, tps::net::message<T>& msg)
    {
        auto pkt = mqtt_packet::create(msg);

        switch(packet_type(pkt->header.bits.type))
        {
            case packet_type::CONNECT:
                break;
            case packet_type::CONNACK:
                break;
            case packet_type::SUBSCRIBE:
                break;
            case packet_type::UNSUBSCRIBE:
                break;
            case packet_type::SUBACK:
                break;
            case packet_type::PUBLISH:
                break;
            case packet_type::PUBACK:
            case packet_type::PUBREC:
            case packet_type::PUBREL:
            case packet_type::PUBCOMP:
                break;
            case packet_type::PINGREQ:
            case packet_type::PINGRESP:
            case packet_type::DISCONNECT:
                break;
            default:
                client->disconnect();
                break;
        }
    }

    virtual bool on_first_message(std::shared_ptr<tps::net::connection<T>> netClient,
                                  tps::net::message<T>& msg)
    {
        if (packet_type(msg.hdr.byte.bits.type) == packet_type::CONNECT)
        {
            auto pkt = mqtt_packet::create<mqtt_connect>(msg.hdr.byte.byte);
            pkt->unpack(msg);

            mqtt_connack connack;
//            c = handle_connect();
            tps::net::message<T> reply;
            connack.pack(reply);
            this->message_client(netClient, reply);

            return true;
        }
        return false;
    }

private:
    struct core core;
};

#endif // SERVER_H
