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
        {
            uint16_t keepalive = uint16_t(msg.body[9] | (uint16_t(msg.body[8]) << 8));
            uint32_t mls = keepalive * 1000 * 3 / 2; // [MQTT-3.1.2-24]
            netClient->set_timer(mls);
        }
        return true;
    }

    return false;
}

void server::on_message(pConnection netClient, tps::net::message<mqtt_header>& msg)
{
    auto newPkt = mqtt_packet::create(msg);
    if (!newPkt)
        return;

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
    connack.rc = 0;

    if (pkt.payload.protocolLevel != 4)
    {
        connack.rc = 1; // [MQTT-3.1.2-2]
        goto reply;
    }
    if (!pkt.vhdr.bits.cleanSession && !pkt.payload.client_id.size())
    {
        connack.rc = 2; // [MQTT-3.1.3-8]
        goto reply;
    }

    // check if client with same client ID already exists
    if (auto res = m_core.find_client(pkt.payload.client_id))
    {
        auto& existingClient = res.value().get();

        // deletion of the client
        // second CONNECT packet came from the same client(address) - violation [MQTT-3.1.0-2]
        if (existingClient->netClient.get() == netClient)
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
            // transform old client into inactive state, even if it's clean session was originally == 1
            disconnect(existingClient, DISCONNECT, core_t::STORE_SESSION);

        // return of the client
        // if there is a stored session (previous CONNECT packet from
        // client with same client ID had clean session bit == 0)
        if (!pkt.vhdr.bits.cleanSession)
        {
            client = m_core.restore_client(existingClient, std::move(netClient));
            connack.sp.byte = 1;
        }
        else
        {
            // delete stored session
            disconnect(existingClient, NONE, core_t::FULL_DELETION);

            client = m_core.add_new_client(std::move(pkt.payload.client_id), std::move(netClient));
        }
    }
    else
        client = m_core.add_new_client(std::move(pkt.payload.client_id), std::move(netClient));

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

reply:
    // send CONNACK response
    tps::net::message<mqtt_header> reply;
    connack.pack(reply);
    client->netClient.get()->send(std::move(reply), this);

    if (connack.rc)
    {
        // [MQTT-3.2.2-4], [MQTT-3.2.2-5]
        netClient->disconnect(this);
        return;
    }

    // if client restores session send all saved msgs
    if (connack.sp.byte)
    {
        for (auto& pkt : client->session.savedMsgs)
        {
            auto ackType = (pkt.header.bits.qos == AT_LEAST_ONCE) ? packet_type::PUBACK : packet_type::PUBREC;
            pkt.pkt_id = client->session.pool.generate_key(ackType);

            tps::net::message<mqtt_header> msg;
            pkt.pack(msg);
            client->netClient.get()->send(std::move(msg), this);
        }
        client->session.savedMsgs.clear();
    }
}

void server::handle_subscribe(pClient& client, mqtt_subscribe& pkt)
{
    mqtt_suback suback;

    std::vector<tps::net::message<mqtt_header>> retainedMsgs;
    auto save_retained_msg = [&retainedMsgs](topic_t& topic, uint8_t qos)
    {
        tps::net::message<mqtt_header> pubmsg;
        topic.retain->header.bits.qos = std::min(qos, topic.retain->header.bits.qos);
        topic.retain->pack(pubmsg);
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
                m_core.subscribe(*client, *topic, qos);
                if (topic->retain)
                    save_retained_msg(*topic, qos);
            }
        }
        else
        {
            auto topic = m_core.find_topic(topicfilter, true);
            m_core.subscribe(*client, topic->get(), qos);
            if (topic->get().retain)
                save_retained_msg(topic->get(), qos);
        }
        suback.rcs.push_back(qos);
    }

    // send SUBACK response
    tps::net::message<mqtt_header> reply;
    suback.pkt_id = pkt.pkt_id;
    suback.pack(reply);
    client->netClient.get()->send(std::move(reply), this);

    // send retained msgs
    for (auto& msg: retainedMsgs)
        client->netClient.get()->send(std::move(msg), this);
}

void server::handle_unsubscribe(pClient& client, mqtt_unsubscribe& pkt)
{
    for (auto& [topiclen, topicfilter]: pkt.tuples)
    {
        // if topic filter cotains wildcards
        if (topicfilter[0] == '+' || topicfilter[topiclen-1] == '#'
                || topicfilter.find("/+") != std::string::npos)
        {
            auto matches = m_core.get_matching_topics(topicfilter);
            for (auto& topic: matches)
                m_core.unsubscribe(*client, *topic);
        }
        else
        {
            if (auto topic = m_core.find_topic(topicfilter))
                m_core.unsubscribe(*client, topic->get());
        }
    }

    // send UNSUBACK response
    mqtt_unsuback unsuback(UNSUBACK_BYTE);
    tps::net::message<mqtt_header> reply;
    unsuback.pkt_id = pkt.pkt_id;
    unsuback.pack(reply);
    client->netClient.get()->send(std::move(reply), this);
}

void server::publish_msg(mqtt_publish& pkt)
{
    auto topic = m_core.find_topic(pkt.topic);
    if (!topic)
        return;

    // if retain flag set
    if (pkt.header.bits.retain)
    {
        // save new retained msg
        if (pkt.payload.size())
        {
            mqtt_publish retain = pkt;
            retain.header.bits.dup = 0;

            topic->get().retain = std::move(retain);
        }
        else
            // if payload.size() == 0 delete existing retained msg
            topic->get().retain.reset();

        pkt.header.bits.retain = 0; // [MQTT-3.3.1-9]
    }

    pkt.header.bits.dup = 0;

    // prepare version with qos == 0 beforehand, since it doesn't need pkt ID
    tps::net::message<mqtt_header> pubmsgQoS0;
    pubmsgQoS0.hdr.byte.bits.qos = AT_MOST_ONCE;
    pkt.pack(pubmsgQoS0);

    auto originalPktID = pkt.pkt_id;

    // send published msg to subscribers
    for (auto& sub: topic->get().subscribers)
    {
        auto& subClient = sub.second.first;

        // determine QoS level based on published msg QoS and client's
        // max QoS level specified in SUBSCRIBE packet
        auto qos = std::min(sub.second.second, pkt.header.bits.qos);

        if (subClient.active)
        {
            tps::net::message<mqtt_header> temp;
            if (qos == AT_MOST_ONCE)
                temp = pubmsgQoS0;
            else
            {
                // assign pkt ID
                auto ackType = (qos == AT_LEAST_ONCE) ? packet_type::PUBACK : packet_type::PUBREC;
                pkt.pkt_id = subClient.session.pool.generate_key(ackType);

                pkt.pack(temp);

                temp.hdr.byte.bits.qos = qos;
            }
            subClient.netClient.get()->send(std::move(temp), this);
        }
        else
        {
            // save msgs until session is restored
            if (qos > AT_MOST_ONCE)
            {
                subClient.session.savedMsgs.push_back(pkt);
                subClient.session.savedMsgs.back().header.bits.qos = qos;
            }
        }
    }
    pkt.pkt_id = originalPktID;
}

void server::handle_publish(pClient& client, mqtt_publish& pkt)
{
    mqtt_ack ack(PUBACK_BYTE);

    // if it's an attempt to resend qos2 msg that has already been
    // received (publisher didn't get pubrec reply for some reason) [MQTT-4.3.3-2]
    bool bQoS2Resend = false;
    if (pkt.header.bits.qos == EXACTLY_ONCE)
    {
        auto pubrel = client->session.pool.find(pkt.pkt_id);
        if (pubrel && pubrel.value().get() == packet_type::PUBREL)
        {
            // send pubrec again
            ack.header.byte = PUBREC_BYTE;
            ack.header.bits.dup = 1;

            bQoS2Resend = true;
        }

        // QOS == 2 send PUBREC, recv PUBREL, send PUBCOMP
        ack.header.byte = PUBREC_BYTE;
        client->session.pool.register_key(pkt.pkt_id, packet_type::PUBREL);
    }

    if (!bQoS2Resend)
    {
        publish_msg(pkt);

        if (pkt.header.bits.qos == AT_MOST_ONCE)
            return;
    }

    // send reply to publisher if QOS == 1 or 2
    tps::net::message<mqtt_header> reply;
    ack.pkt_id = pkt.pkt_id;
    ack.pack(reply);
    client->netClient.get()->send(std::move(reply), this);
}

void server::disconnect(pClient& client, uint8_t flags, uint8_t manualControl)
{
    mqtt_publish will;
    bool bPubWill = (flags & PUBLISH_WILL) && client->will;

    if (flags & DISCONNECT)
        client->netClient.get()->disconnect(this);

    if (bPubWill)
        will = std::move(*client->will);

    m_core.delete_client(client, manualControl);

    if (bPubWill)
        publish_msg(will);
}

void server::handle_puback(pClient& client, mqtt_puback& pkt)
{
    auto val = client->session.pool.find(pkt.pkt_id);
    if (val && val.value().get() == packet_type::PUBACK)
        client->session.pool.unregister_key(pkt.pkt_id);
}

void server::handle_pubrec(pClient& client, mqtt_pubrec& pkt)
{
    if (auto val = client->session.pool.find(pkt.pkt_id))
    {
        mqtt_pubrel pubrel(PUBREL_BYTE);
        pubrel.pkt_id = pkt.pkt_id;

        auto ackType = val.value().get();
        if (ackType == packet_type::PUBREC)
        {
            client->session.pool.unregister_key(pkt.pkt_id);
            client->session.pool.register_key(pkt.pkt_id, packet_type::PUBCOMP);

        }
        else if (ackType == packet_type::PUBCOMP)
        {
            // PUBREL that was sent earlier didn't get to the receiver - resend PUBREL
            pubrel.header.bits.dup = 1;
        }

        tps::net::message<mqtt_header> msg;
        pubrel.pack(msg);
        client->netClient.get()->send(std::move(msg), this);
    }
}

void server::handle_pubrel(pClient& client, mqtt_pubrel& pkt)
{
    auto val = client->session.pool.find(pkt.pkt_id);
    if (val && val.value().get() == packet_type::PUBREL)
    {
        // [MQTT-4.3.3-2]
        client->session.pool.unregister_key(pkt.pkt_id);

        tps::net::message<mqtt_header> msg;
        mqtt_pubcomp pubcomp(PUBCOMP_BYTE);
        pubcomp.pkt_id = pkt.pkt_id;
        pubcomp.pack(msg);
        client->netClient.get()->send(std::move(msg), this);
    }
}

void server::handle_pubcomp(pClient& client, mqtt_pubcomp& pkt)
{
    auto val = client->session.pool.find(pkt.pkt_id);
    if (val && val.value().get() == packet_type::PUBCOMP)
        client->session.pool.unregister_key(pkt.pkt_id);
}

void server::handle_pingreq(pClient& client)
{
    mqtt_pingresp pingresp(PINGRESP_BYTE);
    tps::net::message<mqtt_header> reply;

    pingresp.pack(reply);
    client->netClient.get()->send(std::move(reply), this);
}

