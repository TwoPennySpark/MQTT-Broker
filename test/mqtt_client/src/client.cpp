#include "client.h"

tps::net::message<mqtt_header> MQTTClient::pack_connect(mqtt_connect &con)
{
    tps::net::message<mqtt_header> msg;

    uint32_t len = 0;

    msg.hdr.byte.bits.type = con.header.bits.type;

    std::string protocolName = "MQTT";
    uint16_t protocolLen = uint16_t(protocolName.size());
    protocolLen = byteswap16(protocolLen);
    msg << protocolLen;
    msg << protocolName;
    len += sizeof(protocolLen) + uint16_t(protocolName.size());

    uint8_t protocolLevel = 4;
    msg << protocolLevel;
    len += sizeof(protocolLevel);

    mqtt_connect::variable_header vhdr;
    vhdr.byte = con.vhdr.byte;
    msg << vhdr;
    len += sizeof(vhdr);

    uint16_t keepalive = byteswap16(con.payload.keepalive);
    msg << keepalive;
    len += sizeof(keepalive);

    uint16_t clientIDLen = uint16_t(con.payload.clientID.size());
    clientIDLen = byteswap16(clientIDLen);
    msg << clientIDLen;
    msg << con.payload.clientID;
    len += sizeof(clientIDLen) + uint16_t(con.payload.clientID.size());

    if (vhdr.bits.will)
    {
        uint16_t willTopicLen = uint16_t(con.payload.willTopic.size());
        willTopicLen = byteswap16(willTopicLen);
        msg << willTopicLen;
        msg << con.payload.willTopic;
        len += sizeof(willTopicLen) + uint16_t(con.payload.willTopic.size());

        uint16_t willMsgLen = uint16_t(con.payload.willMessage.size());
        willMsgLen = byteswap16(willMsgLen);
        msg << willMsgLen;
        msg << con.payload.willMessage;
        len += sizeof(willMsgLen) + uint16_t(con.payload.willMessage.size());
    }

    if (vhdr.bits.username)
    {
        uint16_t usernameLen = uint16_t(con.payload.username.size());
        usernameLen = byteswap16(usernameLen);
        msg << usernameLen;
        msg << con.payload.username;
        len += sizeof(usernameLen) + uint16_t(con.payload.username.size());
    }

    if (vhdr.bits.password)
    {
        uint16_t passwordLen = uint16_t(con.payload.password.size());
        passwordLen = byteswap16(passwordLen);
        msg << passwordLen;
        msg << con.payload.password;
        len += sizeof(passwordLen) + uint16_t(con.payload.password.size());
    }

    msg.writeHdrSize += mqtt_encode_length(msg, len);

    return msg;
}

void MQTTClient::pack_subscribe(mqtt_subscribe &sub, tps::net::message<mqtt_header> &msg)
{
    uint size = 0;

    msg.hdr.byte = sub.header;

    auto pkt_id = byteswap16(sub.pktID);
    msg << pkt_id;
    size += sizeof(sub.pktID);

    for (auto& [topiclen, topic, qos]: sub.tuples)
    {
        auto topiclenbe = byteswap16(topiclen);
        msg << topiclenbe;
        msg << topic;
        msg << qos;
        size += sizeof(topiclen) + topiclen + sizeof(qos);
    }

    msg.writeHdrSize += mqtt_encode_length(msg, size);
}

void MQTTClient::pack_unsubscribe(mqtt_unsubscribe &unsub, tps::net::message<mqtt_header> &msg)
{
    uint size = 0;

    msg.hdr.byte = unsub.header;

    auto pkt_id = byteswap16(unsub.pktID);
    msg << pkt_id;
    size += sizeof(unsub.pktID);

    for (auto& [topiclen, topic]: unsub.tuples)
    {
        auto topiclenbe = byteswap16(topiclen);
        msg << topiclenbe;
        msg << topic;
        size += sizeof(topiclen) + topiclen;
    }

    msg.writeHdrSize += mqtt_encode_length(msg, size);
}

mqtt_publish MQTTClient::unpack_publish(tps::net::message<mqtt_header> &msg)
{
    mqtt_publish pub;
    pub.header.byte = msg.hdr.byte.byte;
    pub.unpack(msg);

    return pub;
}

mqtt_connack MQTTClient::unpack_connack(tps::net::message<mqtt_header> &msg)
{
    mqtt_connack con;

    con.header.byte = msg.hdr.byte.byte;
    msg >> con.sp.byte;
    msg >> con.rc;

    return con;
}

mqtt_suback MQTTClient::unpack_suback(tps::net::message<mqtt_header> &msg)
{
    mqtt_suback suback;

    suback.header.byte = msg.hdr.byte.byte;
    msg >> suback.pktID;
    suback.pktID = byteswap16(suback.pktID);

    uint16_t rcsBytes = msg.hdr.size - sizeof(suback.pktID);
    suback.rcs.resize(rcsBytes);
    msg >> suback.rcs;

    return suback;
}

mqtt_ack MQTTClient::unpack_ack(tps::net::message<mqtt_header> &msg)
{
    mqtt_ack ack;
    ack.header.byte = msg.hdr.byte.byte;
    ack.unpack(msg);

    return ack;
}

void MQTTClient::connect_server(const std::string &clientID, uint8_t cleanSession)
{
    mqtt_connect con(CONNECT_BYTE);
    con.vhdr.bits.cleanSession = cleanSession;
    con.payload.clientID = clientID;
    send(pack_connect(con));
}

uint16_t MQTTClient::publish(std::string &topic, std::string &msg, uint8_t qos, uint8_t retain)
{
    static uint16_t pktID = 0;

    mqtt_publish pub(PUBLISH_BYTE);
    pub.header.bits.qos = qos;
    pub.header.bits.retain = retain;
    pub.pktID = pktID++;
    pub.topic = topic;
    pub.topiclen = uint16_t(pub.topic.size());
    pub.payload = msg;

    tps::net::message<mqtt_header>pubmsg;
    pub.pack(pubmsg);
    send(std::move(pubmsg));

    return pub.pktID;
}

uint16_t MQTTClient::subscribe(std::vector<std::pair<std::string, uint8_t>>& topics)
{
    mqtt_subscribe subreq(SUBREQ_BYTE);
    subreq.header.bits.qos = 1; // [MQTT-3.8.1-1]

    static uint16_t pktID = 0;
    subreq.pktID = pktID++;

    for (auto& p: topics)
    {
        std::tuple<uint16_t, std::string, uint8_t> t;
        auto& [topiclen, topic, qos] = t;{}

        topic = std::move(p.first);
        topiclen = uint16_t(topic.size());
        qos = p.second;
        subreq.tuples.emplace_back(t);
    }

    tps::net::message<mqtt_header> msg;
    pack_subscribe(subreq, msg);
    send(std::move(msg));

    return subreq.pktID;
}

void MQTTClient::unsubscribe()
{
    mqtt_unsubscribe unsub(UNSUB_BYTE);
    unsub.header.bits.qos = 1; // [MQTT-3.10.1-1]

    static uint16_t pktID = 0;
    unsub.pktID = pktID++;

    std::pair<uint16_t, std::string> t;
    auto& [topiclen, topic] = t;{}

    topic = "/foo";
    topiclen = uint16_t(topic.size());
    unsub.tuples.emplace_back(t);

    topic = "/bar";
    topiclen = uint16_t(topic.size());
    unsub.tuples.emplace_back(t);

    tps::net::message<mqtt_header> msg;
    pack_unsubscribe(unsub, msg);
    this->send(std::move(msg));
}

void MQTTClient::send_disconnect()
{
    mqtt_disconnect disc(DISC_BYTE);
    tps::net::message<mqtt_header> msg;
    disc.pack(msg);
    send(std::move(msg));
}

void MQTTClient::send_ack(packet_type type, uint16_t pktID)
{
    tps::net::message<mqtt_header> msg;
    mqtt_ack ack;
    ack.header.bits.type = uint8_t(type);
    if (type == packet_type::PUBREL) // [MQTT-3.6.1-1]
        ack.header.bits.qos = 1;
    ack.pktID = pktID;
    ack.pack(msg);
    send(std::move(msg));
}

void MQTTClient::pingreq()
{
    mqtt_pingreq pingreq(PINGREQ_BYTE);
    tps::net::message<mqtt_header> msg;

    pingreq.pack(msg);
    send(std::move(msg));
}
