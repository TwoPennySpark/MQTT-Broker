#include "server.h"

bool server::on_client_connect(pConnection)
{
    return true;
}

void server::on_client_disconnect(pConnection client)
{
    tps::net::message<mqtt_header> msg;
    msg.hdr.byte.bits.type = uint8_t(packet_type::ERROR);

    m_qMessagesIn.push_back(tps::net::owned_message<mqtt_header>({std::move(client), std::move(msg)}));
}

bool server::on_first_message(pConnection netClient, tps::net::message<mqtt_header>& msg)
{
    const uint8_t CONNECT_MIN_SIZE = 12;
    if (packet_type(msg.hdr.byte.bits.type) == packet_type::CONNECT &&
        msg.hdr.size >= CONNECT_MIN_SIZE)
    {
        // if keepalive bytes != 0
        if (msg.body[9] != 0 || msg.body[8] != 0)
            netClient->set_timer(uint16_t(msg.body[9] | (uint16_t(msg.body[8]) << 8)));
        return true;
    }
    else
        return false;
}

void server::on_message(pConnection netClient, tps::net::message<mqtt_header>& msg)
{
    auto newPkt = mqtt_packet::create(msg);

    auto type = packet_type(newPkt->header.bits.type);
    if (type == packet_type::CONNECT)
    {
        std::cout << "\n\t{CONNECT}\n" << dynamic_cast<mqtt_connect&>(*newPkt);
        handle_connect(netClient, dynamic_cast<mqtt_connect&>(*newPkt));
        return;
    }

    auto res = m_core.find_client(netClient);
    if (!res)
        return;
    auto& client = res.value().get();

    switch (type)
    {
        case packet_type::SUBSCRIBE:
            std::cout << "\n\t{SUBSCRIBE}\n" << dynamic_cast<mqtt_subscribe&>(*newPkt);
            handle_subscribe(client, dynamic_cast<mqtt_subscribe&>(*newPkt));
            break;
        case packet_type::UNSUBSCRIBE:
            std::cout << "\n\t{UNSUB}\n" << dynamic_cast<mqtt_unsubscribe&>(*newPkt);
            handle_unsubscribe(client,  dynamic_cast<mqtt_unsubscribe&>(*newPkt));
            break;
        case packet_type::PUBLISH:
            std::cout << "\n\t{PUBLISH}\n" << dynamic_cast<mqtt_publish&>(*newPkt);
            handle_publish(client,  dynamic_cast<mqtt_publish&>(*newPkt));
            break;
        case packet_type::PUBACK:
            std::cout << "\n\t{PUBACK}\n" << dynamic_cast<mqtt_puback&>(*newPkt);
            handle_puback(client,  dynamic_cast<mqtt_puback&>(*newPkt));
            break;
        case packet_type::PUBREC:
            std::cout << "\n\t{PUBREC}\n" << dynamic_cast<mqtt_pubrec&>(*newPkt);
            handle_pubrec(client,  dynamic_cast<mqtt_pubrec&>(*newPkt));
            break;
        case packet_type::PUBREL:
            std::cout << "\n\t{PUBREL}\n" << dynamic_cast<mqtt_pubrel&>(*newPkt);
            handle_pubrel(client,  dynamic_cast<mqtt_pubrel&>(*newPkt));
            break;
        case packet_type::PUBCOMP:
            std::cout << "\n\t{PUBCOMP}\n" << dynamic_cast<mqtt_pubcomp&>(*newPkt);
            handle_pubcomp(client,  dynamic_cast<mqtt_pubcomp&>(*newPkt));
            break;
        case packet_type::PINGREQ:
            std::cout << "\n\t{PINGREQ}\n" << dynamic_cast<mqtt_pingreq&>(*newPkt);
            handle_pingreq(client);
            break;
        case packet_type::DISCONNECT:
            std::cout << "\n\t{DISCONNECT}\n" << dynamic_cast<mqtt_disconnect&>(*newPkt);
            disconnect(client, DISCONNECT);
            break;
        case packet_type::ERROR:
            std::cout << "\n\t{ERROR}\n" << dynamic_cast<mqtt_disconnect&>(*newPkt);
            disconnect(client, PUBLISH_WILL);
            break;
        case packet_type::CONNACK:
        case packet_type::SUBACK:
        case packet_type::UNSUBACK:
        case packet_type::PINGRESP:
        default:
            std::cout << "\n\t{UNEXPECTED TYPE}: "<< uint8_t(type) << "\n";
            break;
    }
}

void server::handle_connect(pConnection& netClient, mqtt_connect& pkt)
{
    // points either to an already existing record with same client ID or
    // to a newly created client
    pClient client;
    mqtt_connack connack;
    connack.sp.byte = 0;

    // check if client with same client ID already exists
    if (auto res = m_core.find_client(pkt.payload.client_id))
    {
        auto& existingClient = res.value().get();

        // deletion of the client
        // second CONNECT packet came from the same client(address) - violation [MQTT-3.1.0-2]
        if (existingClient->netClient == netClient)
        {
            disconnect(existingClient, DISCONNECT | PUBLISH_WILL);
            return; // no CONNACK reply [MQTT-3.1.4-1]
        }

        // replacement of the client
        // second CONNECT packet came from other address - disconnect existing client [MQTT-3.1.4-2]
        // protocol doesn't clearly specify whether the new client should overtake old client's session,
        // so, in this implementation, new client only overtakes old client's session when new client's
        // clean session == 0
        if (existingClient->active)
        {
            // transform old client into inactive state, even if it's clean session was originally == 1
            disconnect(existingClient, DISCONNECT, core_t::STORE_SESSION);
        }

        // return of the client
        // if there is a stored session (previous CONNECT packet from
        // client with same client ID had clean session bit == 0)
        if (!pkt.vhdr.bits.cleanSession)
        {
            client = m_core.restore_client(existingClient, netClient);
            connack.sp.byte = 1;
        }
        else
        {
            // delete stored session
            disconnect(client, NONE, core_t::FULL_DELETION);

            client = m_core.add_new_client(pkt.payload.client_id, netClient);
        }
    }
    else
        client = m_core.add_new_client(pkt.payload.client_id, netClient);

    client->active = true;

    // move data from CONNECT packet
    client->session.cleanSession = pkt.vhdr.bits.cleanSession;
    if (pkt.vhdr.bits.will)
    {
        mqtt_publish willMsg;
        willMsg.header.bits.qos = pkt.vhdr.bits.will_qos;
        willMsg.header.bits.retain = pkt.vhdr.bits.will_retain;
        willMsg.topic = std::move(pkt.payload.will_topic);
        willMsg.topiclen = uint16_t(willMsg.topic.size());
        willMsg.payload = std::move(pkt.payload.will_message);
        client->will = std::move(willMsg);
    }
    if (pkt.vhdr.bits.username)
        client->username = std::move(pkt.payload.username);
    if (pkt.vhdr.bits.password)
        client->password = std::move(pkt.payload.password);
    client->keepalive = pkt.payload.keepalive;
    client->netClient = std::move(netClient);

    // send CONNACK response
    tps::net::message<mqtt_header> reply;
    connack.rc = 0;
    connack.pack(reply);
    message_client(client->netClient, std::move(reply));

    // if client restores session send all saved msgs
    if (connack.sp.byte)
    {
        for (auto& [msg, id]: client->session.savedMsgs)
        {
            if (msg.hdr.byte.bits.qos == AT_LEAST_ONCE)
                client->session.unregPuback.emplace(id);
            else
                client->session.unregPubrec.emplace(id, 0);

            message_client(client->netClient, std::move(msg));
        }
        client->session.savedMsgs.clear();
    }
}

void server::handle_subscribe(pClient &client, mqtt_subscribe& pkt)
{
    mqtt_suback suback;

    std::vector<tps::net::message<mqtt_header>> retainedMsgs;
    auto save_retained_msg = [&retainedMsgs](std::shared_ptr<topic_t>& topic, uint8_t qos)
    {
        tps::net::message<mqtt_header> pubmsg;
        topic->retain->header.bits.qos = std::min(qos, topic->retain->header.bits.qos);
        topic->retain->pack(pubmsg);
        retainedMsgs.emplace_back(std::move(pubmsg));
    };

    for (auto& [topiclen, topicfilter, qos]: pkt.tuples)
    {
        // if topic filter cotains wildcards
        if (topicfilter[0] == '+' || topicfilter[topiclen-1] == '#'
                || topicfilter.find("/+") != std::string::npos)
        {
            auto matches = m_core.get_matching_topics(topicfilter);
            for (auto& topic: matches)
            {
                topic->sub(client, qos);
                if (topic->retain)
                    save_retained_msg(topic, qos);
            }
        }
        else
        {
            std::shared_ptr<topic_t> topic;
            auto topicNode = m_core.topics.find(topicfilter);
            if (topicNode && topicNode->data) // if topic already exists
                topic = topicNode->data;
            else
            { // create new topic
                auto newTopic = std::make_shared<topic_t>(topicfilter);
                m_core.topics.insert(topicfilter, newTopic);
                topic = newTopic;
            }
            topic->sub(client, qos);
            if (topic->retain)
                save_retained_msg(topic, qos);
        }
        suback.rcs.push_back(qos);
    }

    // send SUBACK response
    tps::net::message<mqtt_header> reply;
    suback.pkt_id = pkt.pkt_id;
    suback.pack(reply);
    message_client(client->netClient, std::move(reply));

    // send retained msgs
    for (auto& msg: retainedMsgs)
        message_client(client->netClient, std::move(msg));
}

void server::handle_unsubscribe(pClient &client, mqtt_unsubscribe& pkt)
{
    for (auto& [topiclen, topicfilter]: pkt.tuples)
    {
        // if topic filter cotains wildcards
        if (topicfilter[0] == '+' || topicfilter[topiclen-1] == '#'
                || topicfilter.find("/+") != std::string::npos)
        {
            auto matches = m_core.get_matching_topics(topicfilter);
            for (auto& topic: matches)
                // delete topic if there is no more subscribers
                if (topic->unsub(client, true) && !topic->subscribers.size())
                    m_core.topics.erase(topic->name);
        }
        else
        {
            auto topicNode = m_core.topics.find(topicfilter);
            if (topicNode && topicNode->data)
            {
                auto& topic = topicNode->data;
                // delete topic if there is no more subscribers
                if (topic->unsub(client, true) && !topic->subscribers.size())
                    m_core.topics.erase(topicNode->data->name);
            }
        }
    }

    // send UNSUBACK response
    mqtt_unsuback unsuback(UNSUBACK_BYTE);
    tps::net::message<mqtt_header> reply;
    unsuback.pkt_id = pkt.pkt_id;
    unsuback.pack(reply);
    message_client(client->netClient, std::move(reply));
}

void server::publish_msg(mqtt_publish& pkt)
{
    auto node = m_core.topics.find(pkt.topic);
    if (node && node->data)
    {
        auto& topic = node->data;

        // if retain flag set
        if (pkt.header.bits.retain)
        {
            // save new retained msg
            if (pkt.payload.size())
            {
                mqtt_publish retain = pkt;
                retain.header.bits.dup = 0;

                topic->retain = std::move(retain);
            }
            else
                // if payload.size() == 0 delete existing retained msg
                topic->retain.reset();
        }

        // send published msg to subscribers with new header (and pkt ID if QoS > 0)
        auto originalQoS = pkt.header.bits.qos;
        tps::net::message<mqtt_header> pubmsgQoS0, pubmsgQoS12;
        pkt.header.bits.retain = 0; // [MQTT-3.3.1-9]
        pkt.header.bits.dup = 0;

        // create both versions of the message if qos > 0
        // so that there is no need to call mqtt_publish::pack
        // inside next loop for every sub
        pkt.header.bits.qos = AT_MOST_ONCE;
        pkt.pack(pubmsgQoS0);
        if (originalQoS > AT_MOST_ONCE)
        {
            static uint16_t pktID = 0; //
            pkt.pkt_id = pktID++;

            pkt.header.bits.qos = originalQoS;
            pkt.pack(pubmsgQoS12);
        }

        for (auto& sub: topic->subscribers)
        {
            // determine QoS level based on published msg QoS and client
            // max QoS level specified in SUBSCRIBE packet
            auto qos = std::min(sub.second.second, pkt.header.bits.qos);

            // avoiding one extra copy inside message_client
            // publish one of the early created versions depending on QoS
            auto temp = (qos == AT_MOST_ONCE) ? pubmsgQoS0 : pubmsgQoS12;
            temp.hdr.byte.bits.qos = qos;

            auto& subClient = sub.second.first;
            if (subClient.active)
            {
                if (qos == AT_LEAST_ONCE)
                    subClient.session.unregPuback.insert(pkt.pkt_id);
                else if (qos == EXACTLY_ONCE)
                    subClient.session.unregPubrec.emplace(uint16_t(pkt.pkt_id), 0);
                message_client(subClient.netClient, std::move(temp));
            }
            else
            {
                // save msgs until session is restored
                if (qos > AT_MOST_ONCE)
                    subClient.session.savedMsgs.emplace_back(std::move(temp), pkt.pkt_id);
            }
        }
    }
}

void server::handle_publish(pClient &client, mqtt_publish& pkt)
{
    mqtt_ack ack;
    ack.pkt_id = pkt.pkt_id;

    // if it's not an attempt to resend qos2 msg that
    // has already been recieved (publisher didn't get pubrec reply for some reason)
    auto itpubrel = client->session.unregPubrel.find(pkt.pkt_id);
    if (!(pkt.header.bits.qos == EXACTLY_ONCE && itpubrel != client->session.unregPubrel.end()))
    {
        publish_msg(pkt);

        // send reply to publisher if QOS == 1 or 2
        // QOS == 1 send PUBACK response
        if (pkt.header.bits.qos == AT_LEAST_ONCE)
        {
            ack.header.byte = PUBACK_BYTE;
        }// QOS == 2 send PUBREC, recv PUBREL, send PUBCOMP
        else if (pkt.header.bits.qos == EXACTLY_ONCE)
        {
            ack.header.byte = PUBREC_BYTE;
            client->session.unregPubrel.emplace(uint16_t(pkt.pkt_id), 0);
        }
        else
            return;
    }
    else
    {
        // send pubrec again
        ack.header.byte = PUBREC_BYTE;
        ack.header.bits.dup = ++itpubrel->second;
    }

    tps::net::message<mqtt_header> reply;
    ack.pack(reply);
    message_client(client->netClient, std::move(reply));
}

void server::disconnect(pClient &client, uint8_t flags, uint8_t manualControl)
{
    if (flags & DISCONNECT)
        client->netClient->disconnect();

    if (flags & PUBLISH_WILL && client->will)
        publish_msg(*client->will);

    m_core.delete_client(client, manualControl);
}

void server::handle_puback(pClient &client, mqtt_puback& pkt)
{
    auto itpuback = client->session.unregPuback.find(pkt.pkt_id);
    if (itpuback != client->session.unregPuback.end())
        client->session.unregPuback.erase(itpuback);
}

void server::handle_pubrec(pClient &client, mqtt_pubrec& pkt)
{
    auto itpubrec = client->session.unregPubrec.find(pkt.pkt_id);
    if (itpubrec != client->session.unregPubrec.end())
    {
        client->session.unregPubcomp.insert(pkt.pkt_id);

        tps::net::message<mqtt_header> msg;
        mqtt_pubrel pubrel(PUBREL_BYTE);
        pubrel.header.bits.dup = itpubrec->second++;
        pubrel.pkt_id = pkt.pkt_id;
        pubrel.pack(msg);
        message_client(client->netClient, std::move(msg));
    }
}

void server::handle_pubrel(pClient& client, mqtt_pubrel& pkt)
{
    auto itpubrel = client->session.unregPubrel.find(pkt.pkt_id);
    if (itpubrel != client->session.unregPubrel.end())
    {
        client->session.unregPubrel.erase(itpubrel);

        tps::net::message<mqtt_header> msg;
        mqtt_pubcomp pubcomp(PUBCOMP_BYTE);
        pubcomp.header.bits.dup = itpubrel->second++;
        pubcomp.pkt_id = pkt.pkt_id;
        pubcomp.pack(msg);
        message_client(client->netClient, std::move(msg));
    }
}

void server::handle_pubcomp(pClient &client, mqtt_pubcomp& pkt)
{
    auto itpubcomp = client->session.unregPubcomp.find(pkt.pkt_id);
    if (itpubcomp != client->session.unregPubcomp.end())
    {
        client->session.unregPubrec.erase(pkt.pkt_id);
        client->session.unregPubcomp.erase(itpubcomp);
    }
}

void server::handle_pingreq(pClient &client)
{
    mqtt_pingresp pingresp(PINGRESP_BYTE);
    tps::net::message<mqtt_header> reply;

    pingresp.pack(reply);
    message_client(client->netClient, std::move(reply));
}

