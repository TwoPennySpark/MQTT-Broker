#include "server.h"

bool server::on_client_connect(std::shared_ptr<tps::net::connection<mqtt_header>>)
{
    return true;
}

void server::on_client_disconnect(std::shared_ptr<tps::net::connection<mqtt_header>> client)
{
    tps::net::message<mqtt_header> msg;
    msg.hdr.byte.bits.type = uint8_t(packet_type::ERROR);

    m_qMessagesIn.push_front(tps::net::owned_message<mqtt_header>({std::move(client), std::move(msg)}));
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

void server::on_message(std::shared_ptr<tps::net::connection<mqtt_header>>& netClient,
                        tps::net::message<mqtt_header>& msg)
{
    auto newPkt = mqtt_packet::create(msg);

    switch(packet_type(newPkt->header.bits.type))
    {
        case packet_type::CONNECT:
            std::cout << "\n\t{CONNECT}\n" << dynamic_cast<mqtt_connect&>(*newPkt);
            handle_connect(netClient, dynamic_cast<mqtt_connect&>(*newPkt));
            break;
        case packet_type::SUBSCRIBE:
            std::cout << "\n\t{SUBSCRIBE}\n" << dynamic_cast<mqtt_subscribe&>(*newPkt);
            handle_subscribe(netClient, dynamic_cast<mqtt_subscribe&>(*newPkt));
            break;
        case packet_type::UNSUBSCRIBE:
            std::cout << "\n\t{UNSUB}\n" << dynamic_cast<mqtt_unsubscribe&>(*newPkt);
            handle_unsubscribe(netClient,  dynamic_cast<mqtt_unsubscribe&>(*newPkt));
            break;
        case packet_type::PUBLISH:
            std::cout << "\n\t{PUBLISH}\n" << dynamic_cast<mqtt_publish&>(*newPkt);
            handle_publish(netClient,  dynamic_cast<mqtt_publish&>(*newPkt));
            break;
        case packet_type::PUBACK:
            std::cout << "\n\t{PUBACK}\n" << dynamic_cast<mqtt_puback&>(*newPkt);
            handle_puback(netClient,  dynamic_cast<mqtt_puback&>(*newPkt));
            break;
        case packet_type::PUBREC:
            std::cout << "\n\t{PUBREC}\n" << dynamic_cast<mqtt_pubrec&>(*newPkt);
            handle_pubrec(netClient,  dynamic_cast<mqtt_pubrec&>(*newPkt));
            break;
        case packet_type::PUBREL:
            std::cout << "\n\t{PUBREL}\n" << dynamic_cast<mqtt_pubrel&>(*newPkt);
            handle_pubrel(netClient,  dynamic_cast<mqtt_pubrel&>(*newPkt));
            break;
        case packet_type::PUBCOMP:
            std::cout << "\n\t{PUBCOMP}\n" << dynamic_cast<mqtt_pubcomp&>(*newPkt);
            handle_pubcomp(netClient,  dynamic_cast<mqtt_pubcomp&>(*newPkt));
            break;
        case packet_type::PINGREQ:
            std::cout << "\n\t{PINGREQ}\n" << dynamic_cast<mqtt_pingreq&>(*newPkt);
            handle_pingreq(netClient);
            break;
        case packet_type::DISCONNECT:
            std::cout << "\n\t{DISCONNECT}\n" << dynamic_cast<mqtt_disconnect&>(*newPkt);
            handle_disconnect(netClient);
            break;
        case packet_type::ERROR:
            std::cout << "\n\t{ERROR}\n" << dynamic_cast<mqtt_disconnect&>(*newPkt);
            handle_error(netClient);
            break;
        case packet_type::CONNACK:
        case packet_type::SUBACK:
        case packet_type::PINGRESP:
        default:
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
        auto& existingClient = it->second;

        // deletion of the client
        // second CONNECT packet came from the same client(address) - violation [MQTT-3.1.0-2]
        if (existingClient->netClient == netClient /*&& existingClient->active*/)
        {
            if (existingClient->will)
            {
                publish_msg(existingClient->willMsg);
                existingClient->will = false;
            }
            existingClient->netClient->disconnect();
            m_core.delete_client(existingClient);
            return; // no CONNACK reply [MQTT-3.1.4-1]
        }

        // replacement of the client
        // second CONNECT packet came from other address - disconnect existing client [MQTT-3.1.4-2]
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
        // create new client
        auto newClient = std::make_shared<client_t>(netClient);
        newClient->clientID = pkt.payload.client_id;

        m_core.clientsIDs.emplace(std::pair<std::string, std::shared_ptr<client_t>>
                               (std::move(pkt.payload.client_id), newClient));
        auto res = m_core.clients.emplace(std::pair<std::shared_ptr<tps::net::connection<mqtt_header>>,
                                        std::shared_ptr<client_t>>(netClient, std::move(newClient)));
        client = res.first->second;

        connack.sp.byte = 0;
    }

    client->active = true;

    // move data from CONNECT packet
    client->session.cleanSession = pkt.vhdr.bits.clean_session;
    if (pkt.vhdr.bits.will)
    {
        client->will = true;
        client->willMsg.header.byte = PUBLISH_BYTE;
        client->willMsg.header.bits.qos = pkt.vhdr.bits.will_qos;
        client->willMsg.header.bits.retain = pkt.vhdr.bits.will_retain;
        client->willMsg.topic = std::move(pkt.payload.will_topic);
        client->willMsg.topiclen = uint16_t(client->willMsg.topic.size());
        client->willMsg.payload = std::move(pkt.payload.will_message);
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

//    if (connack.sp.byte)
//    {
//        for (auto& msg: client->session.savedMsgs)
//        {
//            if (msg.hdr.byte.bits.qos == AT_LEAST_ONCE)
//                client->session.unregPuback.insert(msg.pkt_id);
//            else
//                client->session.unregPubrec.insert(msg.pkt_id);

//            this->message_client(netClient, msg);
//        }
//    }
}

void server::handle_subscribe(std::shared_ptr<tps::net::connection<mqtt_header>>& netClient,
                    mqtt_subscribe& pkt)
{
    auto it = m_core.clients.find(netClient);
    auto& client = it->second;
    mqtt_suback suback(SUBACK_BYTE);
    std::vector<tps::net::message<mqtt_header>> retainedMsgs;

    auto save_retained_msg = [&retainedMsgs](std::shared_ptr<topic_t>& topic)
    {
        tps::net::message<mqtt_header> pubmsg;
        topic->retainedMsg.pack(pubmsg);
        retainedMsgs.emplace_back(std::move(pubmsg));
    };

    for (auto& tuple: pkt.tuples)
    {
        // if topic filter cotains wildcards
        if (tuple.topic[0] == '+' || tuple.topic[tuple.topic.length()-1] == '#'
                || tuple.topic.find("/+") != std::string::npos)
        {
            auto matches = m_core.get_matching_topics(tuple.topic);
            for (auto& topic: matches)
            {
                topic->sub(client, tuple.qos);
                if (topic->retain)
                    save_retained_msg(topic);
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
            topic->sub(client, tuple.qos);
            if (topic->retain)
                save_retained_msg(topic);
        }
        suback.rcs.push_back(tuple.qos);
    }

    // send SUBACK response
    tps::net::message<mqtt_header> reply;
    suback.pkt_id = pkt.pkt_id;
    suback.pack(reply);
    this->message_client(netClient, std::move(reply));

    // send retained msgs
    for (auto& msg: retainedMsgs)
        this->message_client(netClient, std::move(msg));
}

void server::handle_unsubscribe(std::shared_ptr<tps::net::connection<mqtt_header>>& netClient,
                    mqtt_unsubscribe& pkt)
{
    auto it = m_core.clients.find(netClient);
    auto& client = it->second;

    for (auto& tuple: pkt.tuples)
    {
        if (tuple.topic[0] == '+' || tuple.topic[tuple.topic.length()-1] == '#'
                || tuple.topic.find("/+") != std::string::npos)
        {
            auto matches = m_core.get_matching_topics(tuple.topic);
            for (auto& topic: matches)
                if (topic->unsub(client, true) && !topic->subscribers.size())
                    m_core.topics.erase(topic->name);
        }
        else
        {
            auto topicNode = m_core.topics.find(tuple.topic);
            if (topicNode && topicNode->data)
            {
                auto& topic = topicNode->data;
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
    this->message_client(netClient, std::move(reply));
}

void server::publish_msg(mqtt_publish& pkt)
{
    auto node = m_core.topics.find(pkt.topic);
    if (node && node->data)
    {
        auto& topic = node->data;

        // if retain flag set & qos == 0
        if (pkt.header.bits.retain && pkt.header.bits.qos == AT_MOST_ONCE)
        {
            // save new retained msg
            if (pkt.payload.size())
            {
                topic->retainedMsg.payload = pkt.payload;
                topic->retain = true;
            }
            else
                // if payload.size() == 0 delete existing retained msg
                topic->retain = false;
        }

        // send published msg to subscribers with new header (and pkt ID if QoS > 0)
        uint8_t originalQoS = pkt.header.bits.qos;
        tps::net::message<mqtt_header> pubmsgQoS0;
        tps::net::message<mqtt_header> pubmsgQoS12;
        pkt.header.bits.retain = 0;
        pkt.header.bits.dup = 0;

        static uint16_t pktID = 0; //
        pkt.pkt_id = pktID++;

        // create both versions of the message if qos > 0
        // so that there is no need to call mqtt_publish::pack
        // inside next loop for every sub
        pkt.header.bits.qos = AT_MOST_ONCE;
        pkt.pack(pubmsgQoS0);
        if (originalQoS > AT_MOST_ONCE)
        {
            pkt.header.bits.qos = AT_LEAST_ONCE;
            pkt.pack(pubmsgQoS12);
        }
        pkt.header.bits.qos = originalQoS;

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
                if (temp.hdr.byte.bits.qos == AT_LEAST_ONCE)
                    subClient.session.unregPuback.insert(pkt.pkt_id);
                else if (temp.hdr.byte.bits.qos == EXACTLY_ONCE)
                    subClient.session.unregPubrec.emplace(
                                std::make_pair<uint16_t, uint8_t>(uint16_t(pkt.pkt_id), 0));
                this->message_client(subClient.netClient, std::move(temp));
            }
            else
            {
                // save msgs until session is restored
                if (temp.hdr.byte.bits.qos > AT_MOST_ONCE)
                    subClient.session.savedMsgs.emplace_back(std::move(temp));
            }
        }
    }
}

void server::handle_publish(std::shared_ptr<tps::net::connection<mqtt_header>>& netClient,
                    mqtt_publish& pkt)
{
//    std::cout << "[[[[[[[[[[[[[[[[[" << netClient.use_count() << "]]]]]]]]]]]]]\n";
    mqtt_ack ack;
    auto it = m_core.clients.find(netClient);
    // in case you receive a msg from a recently disconnected client
    if (!(it != m_core.clients.end() && it->second->active))
        return;
    auto& client = it->second;

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
            client->session.unregPubrel.emplace(
                        std::make_pair<uint16_t, uint8_t>(uint16_t(pkt.pkt_id), 0));
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

    ack.pkt_id = pkt.pkt_id;
    tps::net::message<mqtt_header> reply;
    ack.pack(reply);
    this->message_client(netClient, std::move(reply));
}

void server::handle_disconnect(std::shared_ptr<tps::net::connection<mqtt_header>>& netClient)
{
    auto it = m_core.clients.find(netClient);
    if (it != m_core.clients.end())
    {
        netClient->disconnect();
        // discard will msg [MQTT-3.14.4-3]
        it->second->will = false;
        m_core.delete_client(it->second);
    }
}

void server::handle_error(std::shared_ptr<tps::net::connection<mqtt_header>>& netClient)
{
    auto it = m_core.clients.find(netClient);
    if (it != m_core.clients.end())
    {
        auto& client = it->second;
        if (client->will)
        {
            publish_msg(client->willMsg);
            client->will = false;
        }
        m_core.delete_client(client);
    }
}

void server::handle_puback(std::shared_ptr<tps::net::connection<mqtt_header>>& netClient, mqtt_puback& pkt)
{
    auto it = m_core.clients.find(netClient);
    if (it != m_core.clients.end())
    {
        auto client = it->second;
        auto itpuback = client->session.unregPuback.find(pkt.pkt_id);
        if (itpuback != client->session.unregPuback.end())
            client->session.unregPuback.erase(itpuback);
    }
}

void server::handle_pubrec(std::shared_ptr<tps::net::connection<mqtt_header>>& netClient, mqtt_pubrec& pkt)
{
    auto it = m_core.clients.find(netClient);
    if (it != m_core.clients.end())
    {
        auto client = it->second;
        auto itpubrec = client->session.unregPubrec.find(pkt.pkt_id);
        if (itpubrec != client->session.unregPubrec.end())
        {
            client->session.unregPubcomp.insert(pkt.pkt_id);

            tps::net::message<mqtt_header> msg;
            mqtt_pubrel pubrel(PUBREL_BYTE);
            pubrel.header.bits.dup = itpubrec->second++;
            pubrel.pkt_id = pkt.pkt_id;
            pubrel.pack(msg);
            this->message_client(client->netClient, std::move(msg));
        }
    }
}

void server::handle_pubrel(std::shared_ptr<tps::net::connection<mqtt_header>>& netClient, mqtt_pubrel& pkt)
{
    auto it = m_core.clients.find(netClient);
    if (it != m_core.clients.end())
    {
        auto client = it->second;
        auto itpubrel = client->session.unregPubrel.find(pkt.pkt_id);
        if (itpubrel != client->session.unregPubrel.end())
        {
            client->session.unregPubrel.erase(itpubrel);

            tps::net::message<mqtt_header> msg;
            mqtt_pubcomp pubcomp(PUBCOMP_BYTE);
            pubcomp.header.bits.dup = itpubrel->second++;
            pubcomp.pkt_id = pkt.pkt_id;
            pubcomp.pack(msg);
            this->message_client(client->netClient, std::move(msg));
        }
    }
}

void server::handle_pubcomp(std::shared_ptr<tps::net::connection<mqtt_header>>& netClient, mqtt_pubcomp& pkt)
{
    auto it = m_core.clients.find(netClient);
    if (it != m_core.clients.end())
    {
        auto client = it->second;
        auto itpubcomp = client->session.unregPubcomp.find(pkt.pkt_id);
        if (itpubcomp != client->session.unregPubcomp.end())
        {
            client->session.unregPubrec.erase(pkt.pkt_id);
            client->session.unregPubcomp.erase(itpubcomp);
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

