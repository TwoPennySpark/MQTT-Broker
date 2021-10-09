#include "mqtt.h"
#include "util.h"
#include "unistd.h"
#include "server.h"
#include "core.h"

const uint8_t MAX_REMAINING_LENGTH_SIZE = 4;

uint8_t mqtt_encode_length(std::vector<uint8_t>& buf, uint32_t& iterator, size_t len)
{
    uint32_t bytes = 0;
    do
    {
        if (bytes+1 > MAX_REMAINING_LENGTH_SIZE)
            return bytes;
        uint8_t d = len % 128;
        len /= 128;
        if (len > 0)
            d |= 128;
        buf[iterator+bytes++] = d;
    } while (len > 0);

    iterator += bytes;

    return bytes;
}

uint64_t mqtt_decode_length(const std::vector<uint8_t>& buf, uint32_t& iterator)
{
    uint64_t len = 0;
    uint8_t bytes = 0;
    do
    {
        len |= ((buf[iterator+bytes] & 127) << bytes*7);
    } while((buf[iterator+bytes++] & 128) != 0);

    iterator += bytes;

    return len;
}

typedef std::shared_ptr<std::vector<uint8_t>> mqtt_pack_handler(const mqtt_packet& pkt);
//static std::vector<mqtt_pack_handler*> mqtt_pack_handlers =
//{
//    nullptr,
//    nullptr,
//    pack_mqtt_connack,
//    pack_mqtt_publish,
//    pack_mqtt_ack,
//    pack_mqtt_ack,
//    pack_mqtt_ack,
//    pack_mqtt_ack,
//    nullptr,
//    pack_mqtt_suback,
//    nullptr,
//    pack_mqtt_ack,
//    nullptr,
//    nullptr,
//    nullptr
//};

std::shared_ptr<std::vector<uint8_t>> pack_mqtt_packet(const mqtt_packet& pkt)
{
//    if (pkt.header.bits.type == PINGREQ || pkt.header.bits.type == PINGRESP)
//        return pack_mqtt_header(pkt.header);

//    return mqtt_pack_handlers[pkt.header.bits.type](pkt);
}


typedef uint64_t mqtt_unpack_handler(const std::vector<uint8_t>& buf, uint32_t& iterator, mqtt_packet& pkt);
//static std::vector<mqtt_unpack_handler*> mqtt_unpack_handlers =
//{
//    nullptr,
//    unpack_mqtt_connect, //
//    nullptr,
//    unpack_mqtt_publish,
//    unpack_mqtt_ack,
//    unpack_mqtt_ack,
//    unpack_mqtt_ack,
//    unpack_mqtt_ack,
//    unpack_mqtt_subscribe, //
//    unpack_mqtt_suback,
//    unpack_mqtt_unsubscribe, //
//    unpack_mqtt_ack,
//    nullptr,
//    nullptr,
//    nullptr
//};

//{
//    nullptr,
//    nullptr,
//    pack_mqtt_connack, //
//    pack_mqtt_publish,
//    pack_mqtt_ack,
//    pack_mqtt_ack,
//    pack_mqtt_ack,
//    pack_mqtt_ack,
//    nullptr,
//    pack_mqtt_suback,
//    nullptr,
//    pack_mqtt_ack,
//    nullptr,
//    nullptr,
//    nullptr
//};

uint64_t unpack_mqtt_packet(const std::vector<uint8_t>& buf, mqtt_packet& pkt)
{
    uint64_t rc = 0;
    uint32_t iterator = 0;
    unpack(buf, iterator, pkt.header.byte);

//    if (pkt.header.bits.type != DISCONNECT
//        && pkt.header.bits.type != PINGREQ
//        && pkt.header.bits.type != PINGRESP)
//        rc = mqtt_unpack_handlers[pkt.header.bits.type](buf, iterator, pkt);
    return rc;
}

std::shared_ptr<mqtt_packet> mqtt_packet::create(std::vector<uint8_t> &buf)
{
    mqtt_header hdr = {};
    uint32_t iterator = 0;
    std::shared_ptr<mqtt_packet> ret;
    ::unpack(buf, iterator, hdr.byte);

    uint8_t type = hdr.bits.type;
    switch (type)
    {
        case CONNECT:
            ret = create<mqtt_connect>(hdr.byte);
            break;
        case CONNACK:
            ret = create<mqtt_connack>(hdr.byte);
            break;
        case SUBSCRIBE:
            ret = create<mqtt_subscribe>(hdr.byte);
            break;
        case UNSUBSCRIBE:
            ret = create<mqtt_unsubscribe>(hdr.byte);
            break;
        case SUBACK:
            ret = create<mqtt_suback>(hdr.byte);
            break;
        case PUBLISH:
            ret = create<mqtt_publish>(hdr.byte);
            break;
        case PUBACK:
        case PUBREC:
        case PUBREL:
        case PUBCOMP:
            ret = create<mqtt_ack>(hdr.byte);
            break;
        case PINGREQ:
        case PINGRESP:
        case DISCONNECT:
            ret = create<mqtt_packet>(hdr.byte);
            break;
        default:
            return nullptr;
    }
    iterator = 0;
    ret->unpack(buf, iterator);
    return ret;
}

uint64_t mqtt_packet::unpack(const std::vector<uint8_t>& buf, uint& iterator)
{
    ::unpack(buf, iterator, header.byte);

    return 0;
}

//void pack_send()
//{
//    uint8_t sp = 0, rc = 0;

//    std::shared_ptr<mqtt_packet> pkt(mqtt_packet::create<mqtt_connack>(CONNACK, sp, rc, 1));

//    std::vector<uint8_t> buf;
//    pkt->pack(buf);

////    send(buf)
//}

//int recv_unpack()
//{
//    std::vector<uint8_t> buf;
//    // recv(buf)
//    std::shared_ptr<mqtt_packet> pkt(mqtt_packet::create(buf));

//    switch (pkt->header.bits.type)
//    {
//        case SUBSCRIBE:
//        case UNSUBSCRIBE:
//        case PUBLISH:
//        case PUBREC:
//        case PUBREL:
//        case PUBCOMP:
//        case PINGREQ:
//        case PINGRESP:
//        case DISCONNECT:
//            pkt->unpack(buf);
//            break;
//        default:
//            return -1;
//    }
//    mqtt_connack c;
//    c.unpack(buf);

//    return 0;
//    // process_pkt(pkt)
//}

uint64_t mqtt_connect::unpack(const std::vector<uint8_t>& buf, uint& iterator)
{
    ::unpack(buf, iterator, header.byte);

    uint64_t len = mqtt_decode_length(buf, iterator);

    uint16_t protocolLen = 0;
    ::unpack(buf, iterator, protocolLen);

    std::string protocolName;
    protocolName.resize(protocolLen);
    ::unpack(buf, iterator, protocolName);

    uint8_t protocolLevel = 0;
    ::unpack(buf, iterator, protocolLevel);

    ::unpack(buf, iterator, this->vhdr.byte);

    ::unpack(buf, iterator, this->payload.keepalive);

    uint16_t clientIDLength = 0;
    ::unpack(buf, iterator, clientIDLength);

    this->payload.client_id.resize(clientIDLength);
    ::unpack(buf,iterator, this->payload.client_id);

    if (this->vhdr.bits.will)
    {
        uint16_t willTopicLen = 0;
        ::unpack(buf, iterator, willTopicLen);
        this->payload.will_topic.resize(willTopicLen);
        ::unpack(buf, iterator, this->payload.will_topic);

        uint16_t willMsgLen = 0;
        ::unpack(buf, iterator, willMsgLen);
        this->payload.will_message.resize(willMsgLen);
        ::unpack(buf, iterator, this->payload.will_message);
    }

    if (this->vhdr.bits.username)
    {
        uint16_t usernameLen = 0;
        ::unpack(buf, iterator, usernameLen);
        this->payload.username.resize(usernameLen);
        ::unpack(buf,iterator, this->payload.username);
    }

    if (this->vhdr.bits.password)
    {
        uint16_t passwordLen = 0;
        ::unpack(buf, iterator, passwordLen);
        this->payload.username.resize(passwordLen);
        ::unpack(buf, iterator, this->payload.password);
    }

    return len;
}

uint64_t mqtt_subscribe::unpack(const std::vector<uint8_t> &buf, uint& iterator)
{
    ::unpack(buf, iterator, header.byte);

    uint64_t len = mqtt_decode_length(buf, iterator);

    ::unpack(buf, iterator, pkt_id);

    uint32_t remainingBytes = len - sizeof(pkt_id);

    uint16_t count = 0;
    while (remainingBytes > 0)
    {
        tuples.resize(count+1);
        ::unpack(buf, iterator, tuples[count].topic_len);
        remainingBytes -= sizeof(tuples[count].topic_len);

        tuples[count].topic.resize(tuples[count].topic_len);
        ::unpack(buf, iterator, tuples[count].topic);
        ::unpack(buf, iterator, tuples[count].qos);
        remainingBytes -= sizeof(tuples[count].topic_len) +
                          tuples[count].topic_len +
                          sizeof(tuples[count].qos);
        count++;
    }
    tuples_len = count;

    return len;
}

uint64_t mqtt_unsubscribe::unpack(const std::vector<uint8_t> &buf, uint& iterator)
{
    ::unpack(buf, iterator, header.byte);

    uint64_t len = mqtt_decode_length(buf, iterator);

    ::unpack(buf, iterator, pkt_id);

    uint32_t remainingBytes = len - sizeof(pkt_id);

    uint16_t count = 0;
    while (remainingBytes > 0)
    {
        tuples.resize(count+1);
        ::unpack(buf, iterator, tuples[count].topic_len);
        remainingBytes -= sizeof(tuples[count].topic_len);

        tuples[count].topic.resize(tuples[count].topic_len);
        ::unpack(buf, iterator, tuples[count].topic);
        remainingBytes -= sizeof(tuples[count].topic_len) +
                          tuples[count].topic_len;
        count++;
    }
    tuples_len = count;

    return len;
}

uint64_t mqtt_publish::unpack(const std::vector<uint8_t> &buf, uint& iterator)
{
    ::unpack(buf, iterator, header.byte);

    uint64_t len = mqtt_decode_length(buf, iterator);

    ::unpack(buf, iterator, topiclen);

    topic.resize(topiclen);
    ::unpack(buf, iterator, topic);

    // the len of msg contained in publish packet = packet len - (len of topic size + topic itself) +
    // + len of pkt id when qos level > 0
    payloadlen = len - (sizeof(topiclen) + topiclen);
    // pkt id is a variable field
    if (header.bits.qos > AT_MOST_ONCE)
    {
        ::unpack(buf, iterator, pkt_id);
        payloadlen -= sizeof(pkt_id);
    }

    payload.resize(payloadlen);
    ::unpack(buf, iterator, payload);

    return len;
}

uint64_t mqtt_ack::unpack(const std::vector<uint8_t>& buf, uint& iterator)
{
    ::unpack(buf, iterator, header.byte);
    uint64_t len = mqtt_decode_length(buf, iterator);

    ::unpack(buf, iterator, pkt_id);

    return len;
}

uint64_t mqtt_suback::unpack(const std::vector<uint8_t>& buf, uint& iterator)
{
    ::unpack(buf, iterator, header.byte);
    uint64_t len = mqtt_decode_length(buf, iterator);

    ::unpack(buf, iterator, pkt_id);

    uint16_t rcsBytes = len - sizeof(pkt_id);
    rcs.resize(rcsBytes);
    ::unpack(buf, iterator, rcs);
    rcslen = rcsBytes;

    return len;
}

uint64_t mqtt_packet::pack(std::vector<uint8_t> &buf)
{
    uint iterator = 0;
    buf.resize(MQTT_HEADER_LEN_MIN);

    ::pack(buf, iterator, header.byte);
    mqtt_encode_length(buf, iterator, 0);

    return iterator;
}

uint64_t mqtt_publish::pack(std::vector<uint8_t> &buf)
{
    uint32_t iterator = 0;

    buf.resize(MQTT_HEADER_LEN_MAX);

    ::pack(buf, iterator, header.byte);
    uint32_t remainingLen = sizeof(topiclen) +
                            topiclen + payloadlen;
    if (header.bits.qos > AT_MOST_ONCE)
        remainingLen += sizeof(pkt_id);
    uint8_t remainingLenSize = mqtt_encode_length(buf, iterator, remainingLen);

    buf.resize(sizeof(header.byte) + remainingLenSize + remainingLen);

    ::pack(buf, iterator, topiclen);
    ::pack(buf, iterator, topic);
    if (header.bits.qos > AT_MOST_ONCE)
        ::pack(buf, iterator, pkt_id);
    ::pack(buf, iterator, payload);

    return iterator;
}

uint64_t mqtt_connack::pack(std::vector<uint8_t> &buf)
{
    uint32_t iterator = 0;
    buf.resize(MQTT_HEADER_LEN_MIN + sizeof(sp.byte) + sizeof(rc));

    ::pack(buf, iterator, header.byte);
    mqtt_encode_length(buf, iterator, sizeof(sp)+sizeof(sp));
    ::pack(buf, iterator, sp.byte);
    ::pack(buf, iterator, rc);

    return iterator;
}

uint64_t mqtt_suback::pack(std::vector<uint8_t> &buf)
{
    uint32_t iterator = 0;
    buf.resize(MQTT_HEADER_LEN_MAX);

    ::pack(buf, iterator, header.byte);
    uint32_t remainingLen = sizeof(pkt_id) + rcslen;
    uint8_t remainingLenSize = mqtt_encode_length(buf, iterator, remainingLen);

    buf.resize(sizeof(header.byte) + remainingLenSize + remainingLen);

    ::pack(buf, iterator, pkt_id);
    ::pack(buf, iterator, rcs);

    return iterator;
}

uint64_t mqtt_ack::pack(std::vector<uint8_t> &buf)
{
    uint32_t iterator = 0;
    buf.resize(MQTT_HEADER_LEN_MIN + sizeof(pkt_id));

    ::pack(buf, iterator, header.byte);
    mqtt_encode_length(buf, iterator, sizeof(pkt_id));
    ::pack(buf, iterator, pkt_id);

    return iterator;
}
