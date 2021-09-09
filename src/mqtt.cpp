#include "mqtt.h"

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

std::shared_ptr<std::vector<uint8_t>> pack_mqtt_header(const union mqtt_header& hdr)
{
    uint32_t iterator = 0;
    std::shared_ptr<std::vector<uint8_t>> ret(new std::vector<uint8_t>(MQTT_HEADER_LEN_MIN));

    pack(*ret, iterator, hdr.byte);
    mqtt_encode_length(*ret, iterator, 0);

    return ret;
}

std::shared_ptr<std::vector<uint8_t>> pack_mqtt_ack(const mqtt_packet& pkt)
{
    uint32_t iterator = 0;
    const mqtt_ack* ptr = dynamic_cast<const mqtt_ack*>(&pkt);
    std::shared_ptr<std::vector<uint8_t>> ret(new std::vector<uint8_t>(
                                                  MQTT_HEADER_LEN_MIN + sizeof(ptr->pkt_id)));

    pack(*ret, iterator, pkt.header.byte);
    mqtt_encode_length(*ret, iterator, sizeof(ptr->pkt_id));
    pack(*ret, iterator, ptr->pkt_id);

    return ret;
}

std::shared_ptr<std::vector<uint8_t>> pack_mqtt_connack(const mqtt_packet& pkt)
{
    uint32_t iterator = 0;
    const mqtt_connack* ptr = dynamic_cast<const mqtt_connack*>(&pkt);
    std::shared_ptr<std::vector<uint8_t>> ret(new std::vector<uint8_t>(MQTT_HEADER_LEN_MIN +
                                                                       sizeof(ptr->sp.byte) + sizeof(ptr->rc)));

    pack(*ret, iterator, pkt.header.byte);
    mqtt_encode_length(*ret, iterator, sizeof(ptr->sp)+sizeof(ptr->sp));
    pack(*ret, iterator, ptr->sp.byte);
    pack(*ret, iterator, ptr->rc);

    return ret;
}

std::shared_ptr<std::vector<uint8_t>> pack_mqtt_suback(const mqtt_packet& pkt)
{
    uint32_t iterator = 0;
    const mqtt_suback* ptr = dynamic_cast<const mqtt_suback*>(&pkt);

    std::shared_ptr<std::vector<uint8_t>> ret(new std::vector<uint8_t>(MQTT_HEADER_LEN_MAX));

    pack(*ret, iterator, pkt.header.byte);
    uint32_t remainingLen = sizeof(ptr->pkt_id) + ptr->rcslen;
    uint8_t remainingLenSize = mqtt_encode_length(*ret, iterator, remainingLen);

    (*ret).resize(sizeof(pkt.header.byte) + remainingLenSize + remainingLen);

    pack(*ret, iterator, ptr->pkt_id);
    pack(*ret, iterator, ptr->rcs);

    return ret;
}

std::shared_ptr<std::vector<uint8_t>> pack_mqtt_publish(const mqtt_packet& pkt)
{
    uint32_t iterator = 0;
    const mqtt_publish* ptr = dynamic_cast<const mqtt_publish*>(&pkt);

    std::shared_ptr<std::vector<uint8_t>> ret(new std::vector<uint8_t>(MQTT_HEADER_LEN_MAX));

    pack(*ret, iterator, pkt.header.byte);
    uint32_t remainingLen = sizeof(ptr->topiclen) +
                            ptr->topiclen + ptr->payloadlen;
    if (pkt.header.bits.qos > AT_MOST_ONCE)
        remainingLen += sizeof(ptr->pkt_id);
    uint8_t remainingLenSize = mqtt_encode_length(*ret, iterator, remainingLen);

    (*ret).resize(sizeof(pkt.header.byte) + remainingLenSize + remainingLen);

    pack(*ret, iterator, ptr->topiclen);
    pack(*ret, iterator, ptr->topic);
    if (pkt.header.bits.qos > AT_MOST_ONCE)
        pack(*ret, iterator, ptr->pkt_id);
    pack(*ret, iterator, ptr->payload);

    return ret;
}

typedef std::shared_ptr<std::vector<uint8_t>> mqtt_pack_handler(const mqtt_packet& pkt);
static std::vector<mqtt_pack_handler*> mqtt_pack_handlers =
{
    nullptr,
    nullptr,
    pack_mqtt_connack,
    pack_mqtt_publish,
    pack_mqtt_ack,
    pack_mqtt_ack,
    pack_mqtt_ack,
    pack_mqtt_ack,
    nullptr,
    pack_mqtt_suback,
    nullptr,
    pack_mqtt_ack,
    nullptr,
    nullptr,
    nullptr
};

std::shared_ptr<std::vector<uint8_t>> pack_mqtt_packet(const mqtt_packet& pkt)
{
    if (pkt.header.bits.type == PINGREQ || pkt.header.bits.type == PINGRESP)
        return pack_mqtt_header(pkt.header);

    return mqtt_pack_handlers[pkt.header.bits.type](pkt);
}

uint64_t unpack_mqtt_ack(const std::vector<uint8_t>& buf, uint32_t& iterator, mqtt_packet& packet)
{
    mqtt_ack* pkt = dynamic_cast<mqtt_ack*>(&packet);
    uint64_t len = mqtt_decode_length(buf, iterator);

    unpack(buf, iterator, pkt->pkt_id);

    return len;
}

uint64_t unpack_mqtt_suback(const std::vector<uint8_t>& buf, uint32_t& iterator, mqtt_packet& packet)
{
    mqtt_suback* pkt = dynamic_cast<mqtt_suback*>(&packet);
    uint64_t len = mqtt_decode_length(buf, iterator);

    unpack(buf, iterator, pkt->pkt_id);

    uint32_t rcsBytes = len - sizeof(pkt->pkt_id);
    pkt->rcs.resize(rcsBytes);
    unpack(buf, iterator, pkt->rcs);
    pkt->rcslen = rcsBytes;

    return len;
}

uint64_t unpack_mqtt_subscribe(const std::vector<uint8_t>& buf, uint32_t& iterator, mqtt_packet& packet)
{
    mqtt_subscribe* pkt = dynamic_cast<mqtt_subscribe*>(&packet);
    uint64_t len = mqtt_decode_length(buf, iterator);

    unpack(buf, iterator, pkt->pkt_id);

    uint32_t remainingBytes = len - sizeof(pkt->pkt_id);

    uint16_t count = 0;
    while (remainingBytes > 0)
    {
        pkt->tuples.resize(count+1);
        unpack(buf, iterator, pkt->tuples[count].topic_len);
        remainingBytes -= sizeof(pkt->tuples[count].topic_len);

        pkt->tuples[count].topic.resize(pkt->tuples[count].topic_len);
        unpack(buf, iterator, pkt->tuples[count].topic);
        unpack(buf, iterator, pkt->tuples[count].qos);
        remainingBytes -= sizeof(pkt->tuples[count].topic_len) +
                          pkt->tuples[count].topic_len +
                          sizeof(pkt->tuples[count].qos);
        count++;
    }
    pkt->tuples_len = count;

    return len;
}

uint64_t unpack_mqtt_unsubscribe(const std::vector<uint8_t>& buf, uint32_t& iterator, mqtt_packet& packet)
{
    mqtt_unsubscribe* pkt = dynamic_cast<mqtt_unsubscribe*>(&packet);
    uint64_t len = mqtt_decode_length(buf, iterator);

    unpack(buf, iterator, pkt->pkt_id);

    uint32_t remainingBytes = len - sizeof(pkt->pkt_id);

    uint16_t count = 0;
    while (remainingBytes > 0)
    {
        pkt->tuples.resize(count+1);
        unpack(buf, iterator, pkt->tuples[count].topic_len);
        remainingBytes -= sizeof(pkt->tuples[count].topic_len);

        pkt->tuples[count].topic.resize(pkt->tuples[count].topic_len);
        unpack(buf, iterator, pkt->tuples[count].topic);
        remainingBytes -= sizeof(pkt->tuples[count].topic_len) +
                          pkt->tuples[count].topic_len;
        count++;
    }
    pkt->tuples_len = count;

    return len;
}

uint64_t unpack_mqtt_publish(const std::vector<uint8_t>&buf, uint32_t& iterator, mqtt_packet& packet)
{
    mqtt_publish* pkt = dynamic_cast<mqtt_publish*>(&packet);
    uint64_t len = mqtt_decode_length(buf, iterator);

    unpack(buf, iterator, pkt->topiclen);

    pkt->topic.resize(pkt->topiclen);
    unpack(buf, iterator, pkt->topic);

    // the len of msg contained in publish packet = packet len - (len of topic size + topic itself) +
    // + len of pkt id when qos level > 0
    pkt->payloadlen = len - (sizeof(pkt->topiclen) + pkt->topiclen);
    // pkt id is a variable field
    if (pkt->header.bits.qos > AT_MOST_ONCE)
    {
        unpack(buf, iterator, pkt->pkt_id);
        pkt->payloadlen -= sizeof(pkt->pkt_id);
    }

    pkt->payload.resize(pkt->payloadlen);
    unpack(buf, iterator, pkt->payload);

    return len;
}

uint64_t unpack_mqtt_connect(const std::vector<uint8_t>& buf, uint32_t& iterator, mqtt_packet& packet)
{
    mqtt_connect* pkt = dynamic_cast<mqtt_connect*>(&packet);
    uint64_t len = mqtt_decode_length(buf, iterator);

    uint16_t protocolLen = 0;
    unpack(buf, iterator, protocolLen);

    std::string protocolName;
    protocolName.resize(protocolLen);
    unpack(buf, iterator, protocolName);

    uint8_t protocolLevel = 0;
    unpack(buf, iterator, protocolLevel);

    unpack(buf, iterator, pkt->vhdr.byte);

    unpack(buf, iterator, pkt->payload.keepalive);

    uint16_t clientIDLength = 0;
    unpack(buf, iterator, clientIDLength);

    pkt->payload.client_id.resize(clientIDLength);
    unpack(buf,iterator, pkt->payload.client_id);

    if (pkt->vhdr.bits.will)
    {
        uint16_t willTopicLen = 0;
        unpack(buf, iterator, willTopicLen);
        pkt->payload.will_topic.resize(willTopicLen);
        unpack(buf, iterator, pkt->payload.will_topic);

        uint16_t willMsgLen = 0;
        unpack(buf, iterator, willMsgLen);
        pkt->payload.will_message.resize(willMsgLen);
        unpack(buf, iterator, pkt->payload.will_message);
    }

    if (pkt->vhdr.bits.username)
    {
        uint16_t usernameLen = 0;
        unpack(buf, iterator, usernameLen);
        pkt->payload.username.resize(usernameLen);
        unpack(buf,iterator, pkt->payload.username);
    }

    if (pkt->vhdr.bits.password)
    {
        uint16_t passwordLen = 0;
        unpack(buf, iterator, passwordLen);
        pkt->payload.username.resize(passwordLen);
        unpack(buf, iterator, pkt->payload.password);
    }

    return len;
}

typedef uint64_t mqtt_unpack_handler(const std::vector<uint8_t>& buf, uint32_t& iterator, mqtt_packet& pkt);
static std::vector<mqtt_unpack_handler*> mqtt_unpack_handlers =
{
    nullptr,
    unpack_mqtt_connect, //
    nullptr,
    unpack_mqtt_publish,
    unpack_mqtt_ack,
    unpack_mqtt_ack,
    unpack_mqtt_ack,
    unpack_mqtt_ack,
    unpack_mqtt_subscribe, //
    unpack_mqtt_suback,
    unpack_mqtt_unsubscribe, //
    unpack_mqtt_ack,
    nullptr,
    nullptr,
    nullptr
};

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

    if (pkt.header.bits.type != DISCONNECT
        && pkt.header.bits.type != PINGREQ
        && pkt.header.bits.type != PINGRESP)
        rc = mqtt_unpack_handlers[pkt.header.bits.type](buf, iterator, pkt);
    return rc;
}


