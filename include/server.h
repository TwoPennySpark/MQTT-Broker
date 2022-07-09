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

protected:
    virtual bool on_client_connect(std::shared_ptr<tps::net::connection<T>> client)
    {
        return true;
    }

    virtual void on_client_disconnect(std::shared_ptr<tps::net::connection<T>> client)
    {

    }

    virtual void on_message(std::shared_ptr<tps::net::connection<T>> netClient, tps::net::message<T>& msg)
    {
        auto newPkt = mqtt_packet::create(msg);

        switch(packet_type(newPkt->header.bits.type))
        {
            case packet_type::CONNECT:
            {
                auto pkt = dynamic_pointer_cast <mqtt_connect>(newPkt);
                handle_connect(netClient, std::move(pkt));
                break;
            }
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
                netClient->disconnect();
                break;
        }
    }

    virtual bool on_first_message(std::shared_ptr<tps::net::connection<T>> netClient,
                                  tps::net::message<T>& msg)
    {
        const uint8_t CONNECT_MIN_SIZE = 10;
        if (packet_type(msg.hdr.byte.bits.type) == packet_type::CONNECT && msg.hdr.size >= CONNECT_MIN_SIZE)
            return true;
        else
            return false;
    }

private:
    void handle_connect(std::shared_ptr<tps::net::connection<T>> netClient,
                        std::shared_ptr<mqtt_connect> pkt)
    {
        // points either to an already existing record with same client ID or
        // to newly created client that will be inserted
        std::shared_ptr<client_t> client;
        mqtt_connack connack(CONNACK_BYTE);

        // if client with the same client ID already exists(can be currently connected, or store session)
        auto it = core.clients.find(pkt->payload.client_id);
        if (it != core.clients.end())
        {
            auto existingClient = (*it).second;
            // second CONNECT packet came from the same client(address) - violation
            if (existingClient->netClient == netClient /*&& existingClient->active*/)
            {
                existingClient->netClient->disconnect();
                existingClient->active = false; //
                return;
            }

            // second CONNECT packet came from other address - disconnect existing client
            if (existingClient->active)
            {
                existingClient->netClient->disconnect();
            }
            else
            {// if there is a stored session (previous CONNECT packet from
             // client with same client ID had clean session bit == 0)
                if (pkt->vhdr.bits.clean_session)
                {
                    // delete session
                    existingClient->session.clear();
                    connack.sp.byte = 0;
                }
                else
                {
                    connack.sp.byte = 1;
                }
            }
            client = existingClient;
        }
        else
        {
            auto newClient = std::make_shared<client_t>();
            newClient->clientID = pkt->payload.client_id;

            connack.sp.byte = 0;

            auto res = core.clients.emplace(std::pair<std::string, std::shared_ptr<client_t>>(
                                 std::move(pkt->payload.client_id), std::move(newClient)));
            client = (*res.first).second;
        }

        client->active = true;

        // copy data from CONNECT packet
        client->session.cleansession = pkt->vhdr.bits.clean_session;
        if (pkt->vhdr.bits.will)
        {
            client->will = true;
            client->willQOS = pkt->vhdr.bits.will_qos;
            client->willRetain = pkt->vhdr.bits.will_retain;
            client->willTopic = std::move(pkt->payload.will_topic);
            client->willMsg = std::move(pkt->payload.will_message);
        }
        if (pkt->vhdr.bits.username)
            client->username = std::move(pkt->payload.username);
        if (pkt->vhdr.bits.password)
            client->password = std::move(pkt->payload.password);
        client->keepalive = pkt->payload.keepalive;
        client->netClient = netClient;

        // send CONNACK response
        tps::net::message<T> reply;
        connack.rc = 0;
        connack.pack(reply);
        this->message_client(netClient, reply);
    }

    struct core core;
};

#endif // SERVER_H
