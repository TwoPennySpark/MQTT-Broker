#include "mqtt.h"
#include "NetCommon/net_message.h"

uint8_t mqtt_encode_length(tps::net::message<mqtt_header>& msg, size_t len)
{
    uint8_t bytes = 0;
    const uint8_t MAX_REMAINING_LENGTH_SIZE = 4;
    uint8_t* p = reinterpret_cast<uint8_t*>(&msg.hdr.size);
    do
    {
        if (bytes+1 > MAX_REMAINING_LENGTH_SIZE)
            return bytes;
        uint8_t d = len % 128;
        len /= 128;
        if (len > 0)
            d |= 128;
        p[bytes++] = d;
    } while (len > 0);

    return bytes;
}

uint32_t mqtt_decode_length(tps::net::message<mqtt_header>& msg)
{
    uint32_t len = 0;
    uint8_t bytes = 0;
    const uint8_t MAX_REMAINING_LENGTH_SIZE = 4;
    std::vector<uint8_t> lenV(MAX_REMAINING_LENGTH_SIZE);
    uint8_t* p = reinterpret_cast<uint8_t*>(&msg.hdr.size);

    do
    {
        lenV[bytes] = p[bytes];
        len |= ((lenV[bytes] & 127u) << bytes*7);
    } while (((lenV[bytes++] & 128) != 0) && bytes < MAX_REMAINING_LENGTH_SIZE);

    return len;
}

uint16_t byteswap16(uint16_t x)
{
    return (uint16_t(x >> 8) | uint16_t(x << 8));
}

std::ostream& operator<<(std::ostream& os, const mqtt_header& pkt)
{
    os << "\t=================HEADER=================\n";
    printf("\tTYPE:\t%d\t|\tQOS:\t%d\t|\n\tRETAIN:\t%d\t|\tDUP:\t%d\t|\n",
           pkt.bits.type, pkt.bits.qos, pkt.bits.retain, pkt.bits.dup);
    os << "\t==================BODY==================\n\n";
    return os;
}

std::ostream& operator<<(std::ostream& os, const mqtt_packet& pkt)
{
    os << pkt.header;
    os << "\t================END BODY================\n\n";
    return os;
}

std::ostream& operator<<(std::ostream &os, const mqtt_connect &pkt)
{
    os << pkt.header;
    os << "\tCLIENT ID: \"" << pkt.payload.client_id << "\"" << std::endl;
    os << "\tCLEAN SESSION: " << std::to_string(pkt.vhdr.bits.cleanSession) << std::endl;
    os << "\tKEEPALIVE: " << pkt.payload.keepalive << std::endl;
    if (pkt.vhdr.bits.will)
    {
        os << "\tWILL TOPIC:" << "\"" << pkt.payload.will_topic << "\"" << std::endl;
        os << "\tWILL MSG:" << "\"" << pkt.payload.will_message << "\"" << std::endl;
        os << "\tWILL RETAIN:" << std::to_string(pkt.vhdr.bits.will_retain) << std::endl;
    }
    if (pkt.vhdr.bits.username)
        os << "\tUSERNAME:" << "\"" << pkt.payload.username << "\"" << std::endl;
    if (pkt.vhdr.bits.password)
        os << "\tPASSWORD:" << "\"" << pkt.payload.password << "\"" << std::endl;
    os << "\t================END BODY================\n\n";
    return os;
}

std::ostream& operator<<(std::ostream &os, const mqtt_connack &pkt)
{
    os << pkt.header;
    printf("\tSP:\t%d\t|\tRC:\t%d\t|\n", pkt.sp.bits.session_present, pkt.rc);
    os << "\t================END BODY================\n\n";

    return os;
}

std::ostream& operator<<(std::ostream &os, const mqtt_publish &pkt)
{
    os << pkt.header;
    os << "\tPKT ID: " << pkt.pkt_id << std::endl;
    os << "\tTOPIC[" << pkt.topiclen << "]: " << "\"" << pkt.topic << "\"" << std::endl;
    os << "\tPAYLOAD[" << pkt.payload.size() << "]: " << "\"" << pkt.payload << "\"" << std::endl;
    os << "\t================END BODY================\n\n";

    return os;
}

std::ostream& operator<<(std::ostream &os, const mqtt_subscribe &pkt)
{
    os << pkt.header;
    os << "\tPKT ID: " << pkt.pkt_id << std::endl;
    for (auto& [topiclen, topic, qos]: pkt.tuples)
        os << "\tTOPIC[" << topiclen << "]: "
           << "\"" << topic << "\": " << std::to_string(qos) << std::endl;
    os << "\t================END BODY================\n\n";

    return os;
}

std::ostream& operator<<(std::ostream &os, const mqtt_unsubscribe &pkt)
{
    os << pkt.header;
    os << "\tPKT ID: " << pkt.pkt_id << std::endl;
    for (auto& [topiclen, topic]: pkt.tuples)
        os << "\tTOPIC[" << topiclen << "]: "
           << "\"" << topic << "\"" << std::endl;
    os << "\t================END BODY================\n\n";

    return os;
}

std::ostream& operator<<(std::ostream& os, const mqtt_suback& pkt)
{
    os << pkt.header;
    os << "\tPKT ID: " << pkt.pkt_id << std::endl;
    os << "\tRCS: ";
    for (auto rc: pkt.rcs)
        os << std::to_string(rc) << " ";
    os << std::endl;
    os << "\t================END BODY================\n\n";

    return os;
}

std::ostream& operator<<(std::ostream &os, const mqtt_ack &pkt)
{
    os << pkt.header;
    os << "\tPKT ID: " << pkt.pkt_id << std::endl;
    os << "\t================END BODY================\n\n";

    return os;
}

std::unique_ptr<mqtt_packet> mqtt_packet::create(tps::net::message<mqtt_header>& msg)
{
    std::unique_ptr<mqtt_packet> ret;
    uint8_t byte = msg.hdr.byte.byte;

    switch (packet_type(byte >> 4))
    {
        case packet_type::CONNECT:
            ret = create<mqtt_connect>(byte);
            break;
        case packet_type::CONNACK:
            ret = create<mqtt_connack>(byte);
            break;
        case packet_type::SUBSCRIBE:
            ret = create<mqtt_subscribe>(byte);
            break;
        case packet_type::UNSUBSCRIBE:
            ret = create<mqtt_unsubscribe>(byte);
            break;
        case packet_type::SUBACK:
            ret = create<mqtt_suback>(byte);
            break;
        case packet_type::PUBLISH:
            ret = create<mqtt_publish>(byte);
            break;
        case packet_type::PUBACK:
        case packet_type::PUBREC:
        case packet_type::PUBREL:
        case packet_type::PUBCOMP:
            ret = create<mqtt_ack>(byte);
            break;
        case packet_type::PINGREQ:
        case packet_type::PINGRESP:
        case packet_type::DISCONNECT:
        default:
            ret = create<mqtt_packet>(byte);
            break;
    }

    try {
        ret->unpack(msg);
    } catch (...) {
        ret = nullptr;
    }

    if (msg.data_left_to_pop())
        ret = nullptr;

    return ret;
}

void mqtt_packet::unpack(const tps::net::message<mqtt_header> &msg)
{

}

void mqtt_connect::unpack(const tps::net::message<mqtt_header>& msg)
{
    uint16_t protocolLen = 0;
    msg >> protocolLen;
    protocolLen = byteswap16(protocolLen);

    std::string protocolName;
    protocolName.resize(protocolLen);
    msg >> protocolName;
    if (protocolName != "MQTT")  // [MQTT-3.1.2-1]
        throw std::runtime_error("Invalid protocol name");

    msg >> payload.protocolLevel;

    msg >> vhdr.byte;
    if (vhdr.bits.reserved) // [MQTT-3.1.2-3]
        throw std::runtime_error("Reserved bit must be zero");

    msg >> payload.keepalive;
    payload.keepalive = byteswap16(payload.keepalive);

    uint16_t clientIDLength = 0;
    msg >> clientIDLength;
    clientIDLength = byteswap16(clientIDLength);
    if (clientIDLength > MAX_CLIENT_ID_LEN)  // [MQTT-3.1.3-5]
        throw std::runtime_error("Client ID len must be less than 24 bytes");

    payload.client_id.resize(clientIDLength);
    msg >> payload.client_id;

    if (vhdr.bits.will)
    {
        uint16_t willTopicLen = 0;
        msg >> willTopicLen;
        willTopicLen = byteswap16(willTopicLen);
        payload.will_topic.resize(willTopicLen);
        msg >> payload.will_topic;

        uint16_t willMsgLen = 0;
        msg >> willMsgLen;
        willMsgLen = byteswap16(willMsgLen);
        payload.will_message.resize(willMsgLen);
        msg >> payload.will_message;
    }
    else if (vhdr.bits.will_qos || vhdr.bits.will_retain) // [MQTT-3.1.2-13], [MQTT-3.1.2-15]
        throw std::runtime_error("If the will flag == 0, then the will qos and retain must be == 0");

    if (vhdr.bits.will_qos > EXACTLY_ONCE)
        throw std::runtime_error("the value of qos must not be == 3");

    if (this->vhdr.bits.username)
    {
        uint16_t usernameLen = 0;
        msg >> usernameLen;
        usernameLen = byteswap16(usernameLen);
        this->payload.username.resize(usernameLen);
        msg >> payload.username;
    }

    if (this->vhdr.bits.password)
    {
        if (!this->vhdr.bits.username)
            throw std::runtime_error("if the username flag == 0, the password flag must be == 0");

        uint16_t passwordLen = 0;
        msg >> passwordLen;
        passwordLen = byteswap16(passwordLen);
        payload.username.resize(passwordLen);
        msg >> payload.username;
    }
}

void mqtt_subscribe::unpack(const tps::net::message<mqtt_header> &msg)
{
    // [MQTT-3.8.1-1]
    if (msg.hdr.byte.bits.qos != 1)
        return;

    msg >> pkt_id;
    pkt_id = byteswap16(pkt_id);

    uint32_t remainingBytes = msg.hdr.size - sizeof(pkt_id);

    if (!remainingBytes) // [MQTT-3.8.3-3].
        return;

    uint16_t count = 0;
    while (remainingBytes > 0)
    {
        tuples.resize(count+1);

        auto& [topiclen, topic, qos] = tuples[count];

        msg >> topiclen;
        topiclen = byteswap16(topiclen);

        if (!topiclen) // [MQTT-4.7.3-1]
            return;
        remainingBytes -= sizeof(topiclen);

        topic.resize(topiclen);
        msg >> topic;
        msg >> qos;

        if (qos > EXACTLY_ONCE)
            return;

        remainingBytes -= topiclen + sizeof(qos);
        count++;
    }
}

void mqtt_unsubscribe::unpack(const tps::net::message<mqtt_header> &msg)
{
    msg >> pkt_id;
    pkt_id = byteswap16(pkt_id);

    uint32_t remainingBytes = msg.hdr.size - sizeof(pkt_id);

    uint16_t count = 0;
    while (remainingBytes > 0)
    {
        tuples.resize(count+1);
        auto& [topiclen, topic] = tuples[count];

        msg >> topiclen;
        topiclen = byteswap16(topiclen);
        remainingBytes -= sizeof(topiclen);

        topic.resize(topiclen);
        msg >> topic;
        remainingBytes -= topiclen;
        count++;
    }
}

void mqtt_publish::unpack(const tps::net::message<mqtt_header> &msg)
{
    msg >> topiclen;
    topiclen = byteswap16(topiclen);

    topic.resize(topiclen);
    msg >> topic;

    // the len of msg contained in publish packet = packet len - (len of topic size + topic itself) +
    // + len of pkt id when qos level > 0
    uint32_t payloadlen = msg.hdr.size - (sizeof(topiclen) + topiclen);
    // pkt id is a variable field
    if (header.bits.qos > AT_MOST_ONCE)
    {
        msg >> pkt_id;
        pkt_id = byteswap16(pkt_id);
        payloadlen -= sizeof(pkt_id);
    }

    payload.resize(payloadlen);
    msg >> payload;
}

void mqtt_suback::unpack(const tps::net::message<mqtt_header> &msg)
{
    msg >> pkt_id;
    pkt_id = byteswap16(pkt_id);

    uint16_t rcsBytes = msg.hdr.size - sizeof(pkt_id);
    rcs.resize(rcsBytes);
    msg >> rcs;
}

void mqtt_ack::unpack(const tps::net::message<mqtt_header> &msg)
{
    msg >> pkt_id;
    pkt_id = byteswap16(pkt_id);
}

void mqtt_packet::pack(tps::net::message<mqtt_header>& msg) const
{
    msg.hdr.byte = header.byte;
    msg.writeHdrSize += mqtt_encode_length(msg, 0);
}

void mqtt_publish::pack(tps::net::message<mqtt_header>& msg) const
{
    msg.hdr.byte = header.byte;
    uint32_t remainingLen = sizeof(topiclen) +
                            topiclen + payload.size();
    if (header.bits.qos > AT_MOST_ONCE)
        remainingLen += sizeof(pkt_id);
    msg.writeHdrSize += mqtt_encode_length(msg, remainingLen);

    uint16_t topiclenbe = byteswap16(topiclen);
    msg << topiclenbe;
    msg << topic;
    if (header.bits.qos > AT_MOST_ONCE)
    {
        uint16_t pkt_idbe = byteswap16(pkt_id);
        msg << pkt_idbe;
    }
    msg << payload;
}

void mqtt_connack::pack(tps::net::message<mqtt_header>& msg) const
{
    msg.hdr.byte = header.byte;
    msg.writeHdrSize += mqtt_encode_length(msg, sizeof(sp) + sizeof(rc));

    msg << sp.byte;
    msg << rc;
}

void mqtt_suback::pack(tps::net::message<mqtt_header>& msg) const
{
    msg.hdr.byte = header.byte;
    msg.writeHdrSize += mqtt_encode_length(msg, sizeof(pkt_id) + rcs.size());

    uint16_t pkt_idbe = byteswap16(pkt_id);
    msg << pkt_idbe;
    msg << rcs;
}

void mqtt_ack::pack(tps::net::message<mqtt_header>& msg) const
{
    msg.hdr.byte = header.byte;
    msg.writeHdrSize += mqtt_encode_length(msg, sizeof(pkt_id));

    uint16_t pkt_idbe = byteswap16(pkt_id);
    msg << pkt_idbe;
}
