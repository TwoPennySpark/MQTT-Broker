﻿#include "server.h"

bool server::on_client_connect(std::shared_ptr<tps::net::connection<mqtt_header>>)
{
    return true;
}

void server::on_client_disconnect(std::shared_ptr<tps::net::connection<mqtt_header>> client)
{
    tps::net::message<mqtt_header> msg;
    msg.hdr.byte.bits.type = uint8_t(packet_type::DISCONNECT);

    m_qMessagesIn.push_front(tps::net::owned_message<mqtt_header>({client, std::move(msg)}));
}

bool server::on_first_message(std::shared_ptr<tps::net::connection<mqtt_header>>,
                              tps::net::message<mqtt_header>& msg)
{
    const uint8_t CONNECT_MIN_SIZE = 12;
    if (packet_type(msg.hdr.byte.bits.type) == packet_type::CONNECT
            && msg.hdr.size >= CONNECT_MIN_SIZE)
        return true;
    else
        return false;
}

void server::on_message(std::shared_ptr<tps::net::connection<mqtt_header>> netClient, tps::net::message<mqtt_header>& msg)
{
    auto newPkt = mqtt_packet::create(msg);

    switch(packet_type(newPkt->header.bits.type))
    {
        case packet_type::CONNECT:
        {
            std::cout << "\n\t{CONNECT}\n" << dynamic_cast<mqtt_connect&>(*newPkt);
            handle_connect(netClient, dynamic_cast<mqtt_connect&>(*newPkt));
            break;
        }
        case packet_type::SUBSCRIBE:
        {
            std::cout << "\n\t{SUBSCRIBE}\n" << dynamic_cast<mqtt_subscribe&>(*newPkt);
            handle_subscribe(netClient, dynamic_cast<mqtt_subscribe&>(*newPkt));
            break;
        }
        case packet_type::UNSUBSCRIBE:
        {
            std::cout << "\n\t{UNSUB}\n" << dynamic_cast<mqtt_unsubscribe&>(*newPkt);
            handle_unsubscribe(netClient,  dynamic_cast<mqtt_unsubscribe&>(*newPkt));
            break;
        }
        case packet_type::PUBLISH:
        {
            std::cout << "\n\t{PUBLISH}\n" << dynamic_cast<mqtt_publish&>(*newPkt);
            handle_publish(netClient,  dynamic_cast<mqtt_publish&>(*newPkt));
            break;
        }
        case packet_type::PUBACK:
        case packet_type::PUBREC:
        case packet_type::PUBREL:
        case packet_type::PUBCOMP:
            break;
        case packet_type::PINGREQ:
        {
            std::cout << "\n\t{PINGREQ}\n" << dynamic_cast<mqtt_pingreq&>(*newPkt);
            handle_pingreq(netClient);
            break;
        }
        case packet_type::DISCONNECT:
        {
            std::cout << "\n\t{DISCONNECT}\n" << dynamic_cast<mqtt_disconnect&>(*newPkt);
            handle_disconnect(netClient);
            break;
        }
        case packet_type::CONNACK:
        case packet_type::SUBACK:
        case packet_type::PINGRESP:
        default:
            netClient->disconnect();
            break;
    }
}

void server::handle_connect(std::shared_ptr<tps::net::connection<mqtt_header>>& netClient,
                    mqtt_connect& pkt)
{
    // points either to an already existing record with same client ID or
    // to newly created client that will be inserted
    std::shared_ptr<client_t> client;
    mqtt_connack connack(CONNACK_BYTE);

    // if client with the same client ID already exists(can be currently connected, or store session)
    auto it = m_core.clientsIDs.find(pkt.payload.client_id);
    if (it != m_core.clientsIDs.end())
    {
        auto existingClient = it->second;

        // deletion of the client
        // second CONNECT packet came from the same client(address) - violation [MQTT-3.1.0-2]
        if (existingClient->netClient == netClient /*&& existingClient->active*/)
        {
            m_core.delete_client(existingClient);
            return;
        }

        // replacement of the client
        // second CONNECT packet came from other address - disconnect existing client[MQTT-3.1.4-2]
        if (existingClient->active)
        {
            existingClient->netClient->disconnect();

            m_core.clients.erase(existingClient->netClient);
            m_core.clients.emplace(std::pair<std::shared_ptr<tps::net::connection<mqtt_header>>,
                                 std::shared_ptr<client_t>>(netClient, existingClient));
        }

        // return of the client
        // if there is a stored session (previous CONNECT packet from
        // client with same client ID had clean session bit == 0)
        if (pkt.vhdr.bits.clean_session)
        {
            // TODO:delete session
            connack.sp.byte = 0;
        }
        else
        {
            connack.sp.byte = 1;
            // TODO:send all saved msgs and resend pending msgs
        }
        client = existingClient;
    }
    else
    {
        auto newClient = std::make_shared<client_t>(netClient);
        newClient->clientID = pkt.payload.client_id;

        connack.sp.byte = 0;
        m_core.clientsIDs.emplace(std::pair<std::string, std::shared_ptr<client_t>>
                               (std::move(pkt.payload.client_id), newClient));
        auto res = m_core.clients.emplace(std::pair<std::shared_ptr<tps::net::connection<mqtt_header>>,
                                        std::shared_ptr<client_t>>(netClient, std::move(newClient)));

        client = res.first->second;
    }

    client->active = true;

    // copy data from CONNECT packet
    client->session.cleanSession = pkt.vhdr.bits.clean_session;
    if (pkt.vhdr.bits.will)
    {
        client->will = true;
        client->willQOS = pkt.vhdr.bits.will_qos;
        client->willRetain = pkt.vhdr.bits.will_retain;
        client->willTopic = std::move(pkt.payload.will_topic);
        client->willMsg = std::move(pkt.payload.will_message);
    }
    if (pkt.vhdr.bits.username)
        client->username = std::move(pkt.payload.username);
    if (pkt.vhdr.bits.password)
        client->password = std::move(pkt.payload.password);
    client->keepalive = pkt.payload.keepalive;
    client->netClient = netClient;

    // send CONNACK response
    tps::net::message<mqtt_header> reply;
    connack.rc = 0;
    connack.pack(reply);
    this->message_client(netClient, std::move(reply));
}

void server::handle_subscribe(std::shared_ptr<tps::net::connection<mqtt_header>>& netClient,
                    mqtt_subscribe& pkt)
{
    auto it = m_core.clients.find(netClient);
    auto client = it->second;
    mqtt_suback suback(SUBACK_BYTE);

    for (auto& tuple: pkt.tuples)
    {
        if (tuple.topic[0] == '+' || tuple.topic[tuple.topic.length()-1] == '#'
                || tuple.topic.find("/+") != std::string::npos)
        {
            auto matches = m_core.get_matching_topics(tuple.topic);

            for (auto& topic: matches)
            {
                std::pair<std::shared_ptr<client_t>, uint8_t> temp(client, tuple.qos);
                topic->subscribers.emplace_back(std::move(temp));
//                topic->subscribers.emplace_back(std::make_pair<std::shared_ptr<client_t>, uint8_t>
//                                          (std::make_shared<client_t>(client), uint8_t(tuple.qos)));
            }
        }
        else
        {
            std::shared_ptr<topic_t> topic;
            auto topicNode = m_core.topics.find(tuple.topic);
            if (topicNode && topicNode->data) // if topic already exists
                topic = topicNode->data;
            else
            { // create new topic
                std::shared_ptr<topic_t> newTopic = std::make_shared<topic_t>(tuple.topic);
                m_core.topics.insert(tuple.topic, newTopic);
                topic = newTopic;
            }

            std::pair<std::shared_ptr<client_t>, uint8_t> temp(client, tuple.qos);
            topic->subscribers.emplace_back(std::move(temp));
        }
        suback.rcs.push_back(tuple.qos);
    }

    // send SUBACK response
    tps::net::message<mqtt_header> reply;
    suback.pkt_id = pkt.pkt_id;
    suback.pack(reply);
    this->message_client(netClient, std::move(reply));
}

void server::handle_unsubscribe(std::shared_ptr<tps::net::connection<mqtt_header>>& netClient,
                    mqtt_unsubscribe& pkt)
{
    auto it = m_core.clients.find(netClient);
    auto client = it->second;

    for (auto& tuple: pkt.tuples)
    {
        if (tuple.topic[0] == '+' || tuple.topic[tuple.topic.length()-1] == '#'
                || tuple.topic.find("/+") != std::string::npos)
        {
            auto matches = m_core.get_matching_topics(tuple.topic);
            for (auto& topic: matches)
                if (topic->unsub(client) && !topic->subscribers.size())
                    m_core.topics.erase(topic->name);
        }
        else
        {
            auto topicNode = m_core.topics.find(tuple.topic);
            if (topicNode && topicNode->data)
            {
                auto& topic = topicNode->data;
                if (topic->unsub(client) && !topic->subscribers.size())
                    m_core.topics.erase(topicNode->data->name);
            }
        }
    }

    // send UNSUBACK response
    mqtt_unsuback unsuback(UNSUBACK_BYTE);
    tps::net::message<mqtt_header> reply;
    unsuback.pkt_id = pkt.pkt_id;
    unsuback.pack(reply);
    this->message_client(netClient, std::move(reply));
}

void server::handle_publish(std::shared_ptr<tps::net::connection<mqtt_header>>& netClient,
                    mqtt_publish& pkt)
{
    std::cout << "[[[[[[[[[[[[[[[[[" << netClient.use_count() << "]]]]]]]]]]]]]\n";
    auto it = m_core.clients.find(netClient);
    // in case you receive a msg from a recently disconnected client
    if (!(it != m_core.clients.end() && it->second->active))
        return;
    auto client = it->second;

    auto node = m_core.topics.find(pkt.topic);
    if (node && node->data)
    {
        auto& topic = node->data;

        // if retain flag set
        if (pkt.header.bits.retain)
        {
            // save new retained msg
            if (pkt.payloadlen)
            {
                topic->retainedMsg = pkt.payload;
                topic->retainedQOS = pkt.header.bits.qos;
            }
            else
                // if payload == 0 delete existing retained msg
                topic->retainedMsg.clear();
        }

        // send published msg to subscribers
        mqtt_publish pub(PUBLISH_BYTE);
        pub.topiclen = pkt.topiclen;
        pub.topic = std::move(pkt.topic);
        pub.payloadlen = pkt.payloadlen;
        pub.payload = std::move(pkt.payload);
        tps::net::message<mqtt_header> pubmsg;
        pub.pack(pubmsg);
        for (auto& sub: topic->subscribers)
        {
            if (sub.first->active)
            {
                auto temp = pubmsg; // avoiding one extra copy inside message_client
                temp.hdr.byte.bits.qos = sub.second;
                this->message_client(sub.first->netClient, std::move(temp));
            }

            if (sub.second == AT_LEAST_ONCE)
                sub.first->session.unregPuback.insert(pub.pkt_id);
            else if (sub.second == EXACTLY_ONCE)
                sub.first->session.unregPubrec.insert(pub.pkt_id);
        }
    }

    // send reply to publisher if QOS == 1 or 2
    // QOS == 1 send PUBACK response
    uint8_t type = 0;
    if (pkt.header.bits.qos == AT_LEAST_ONCE)
    {
        type = PUBACK_BYTE;
    }// QOS == 2 send PUBREC, recv PUBREL, send PUBCOMP
    else if (pkt.header.bits.qos == EXACTLY_ONCE)
    {
        type = PUBREC_BYTE;
        client->session.unregPubrel.insert(pkt.pkt_id);
    }
    else
        return;

    mqtt_puback ack(type);
    tps::net::message<mqtt_header> reply;
    ack.pkt_id = pkt.pkt_id;
    ack.pack(reply);
    this->message_client(netClient, std::move(reply));
}

void server::handle_disconnect(std::shared_ptr<tps::net::connection<mqtt_header>>& netClient)
{
    netClient->disconnect();
    auto it = m_core.clients.find(netClient);
    if (it != m_core.clients.end())
    {
        auto client = it->second;
        if (client->session.cleanSession)
        {
            m_core.clients.erase(client->netClient);
            m_core.clientsIDs.erase(client->clientID);
        }
        else
        {
            // discard will msg [MQTT-3.14.4-3]
            if (client->will)
                client->will = false;
            it->second->active = false;
        }
    }
}

void server::handle_pingreq(std::shared_ptr<tps::net::connection<mqtt_header>>& netClient)
{
    mqtt_pingresp pingresp(PINGRESP_BYTE);
    tps::net::message<mqtt_header> reply;

    pingresp.pack(reply);
    this->message_client(netClient, std::move(reply));
}

