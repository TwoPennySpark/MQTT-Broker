#include "mqtt.h"
#include "NetCommon/net_message.h"

const uint8_t MAX_REMAINING_LENGTH_SIZE = 4;

uint8_t mqtt_encode_length(tps::net::message& msg, size_t len)
{
    uint8_t bytes = 0;
    do
    {
        if (bytes+1 > MAX_REMAINING_LENGTH_SIZE)
            return bytes;
        uint8_t d = len % 128;
        len /= 128;
        if (len > 0)
            d |= 128;
        msg << d;
        bytes++;
    } while (len > 0);

    return bytes;
}

uint32_t mqtt_decode_length(tps::net::message& msg)
{
    uint32_t len = 0;
    uint8_t bytes = 0;
    std::vector<uint8_t> lenV(MAX_REMAINING_LENGTH_SIZE);

    do
    {
        msg >> lenV[bytes];
        len |= ((lenV[bytes] & 127u) << bytes*7);
    } while (((lenV[bytes++] & 128) != 0) && bytes < MAX_REMAINING_LENGTH_SIZE);

    return len;
}

std::shared_ptr<mqtt_packet> mqtt_packet::create(tps::net::message& msg)
{
    mqtt_header hdr = {};
    std::shared_ptr<mqtt_packet> ret;

    msg >> hdr.byte;

    switch (hdr.byte >> 4)
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
    ret->unpack(msg);

    return ret;
}

void mqtt_packet::unpack(tps::net::message& msg)
{

}

void mqtt_connect::unpack(tps::net::message& msg)
{
    mqtt_decode_length(msg);

    uint16_t protocolLen = 0;
    msg >> protocolLen;

    std::string protocolName;
    protocolName.resize(protocolLen);
    msg >> protocolName;

    uint8_t protocolLevel = 0;
    msg >> protocolLevel;

    msg >> vhdr.byte;

    msg >> payload.keepalive;

    uint16_t clientIDLength = 0;
    msg >> clientIDLength;

    payload.client_id.resize(clientIDLength);
    msg >> payload.client_id;

    if (vhdr.bits.will)
    {
        uint16_t willTopicLen = 0;
        msg >> willTopicLen;
        payload.will_topic.resize(willTopicLen);
        msg >> payload.will_topic;

        uint16_t willMsgLen = 0;
        msg >> willMsgLen;
        payload.will_message.resize(willMsgLen);
        msg >> payload.will_message;
    }

    if (this->vhdr.bits.username)
    {
        uint16_t usernameLen = 0;
        msg >> usernameLen;
        this->payload.username.resize(usernameLen);
        msg >> payload.username;
    }

    if (this->vhdr.bits.password)
    {
        uint16_t passwordLen = 0;
        msg >> passwordLen;
        payload.username.resize(passwordLen);
        msg >> payload.username;
    }
}

void mqtt_subscribe::unpack(tps::net::message& msg)
{
    uint32_t len = mqtt_decode_length(msg);

    msg >> pkt_id;

    uint32_t remainingBytes = len - sizeof(pkt_id);

    uint16_t count = 0;
    while (remainingBytes > 0)
    {
        tuples.resize(count+1);
        msg >> tuples[count].topic_len;
        remainingBytes -= sizeof(tuples[count].topic_len);

        tuples[count].topic.resize(tuples[count].topic_len);
        msg >> tuples[count].topic;
        msg >> tuples[count].qos;
        remainingBytes -= tuples[count].topic_len +
                          sizeof(tuples[count].qos);
        count++;
    }
}

void mqtt_unsubscribe::unpack(tps::net::message& msg)
{
    uint32_t len = mqtt_decode_length(msg);

    msg >> pkt_id;

    uint32_t remainingBytes = len - sizeof(pkt_id);

    uint16_t count = 0;
    while (remainingBytes > 0)
    {
        tuples.resize(count+1);
        msg >> tuples[count].topic_len;
        remainingBytes -= sizeof(tuples[count].topic_len);

        tuples[count].topic.resize(tuples[count].topic_len);
        msg >> tuples[count].topic;
        remainingBytes -= tuples[count].topic_len;
        count++;
    }
}

void mqtt_publish::unpack(tps::net::message& msg)
{
    uint32_t len = mqtt_decode_length(msg);

    msg >> topiclen;

    topic.resize(topiclen);
    msg >> topic;

    // the len of msg contained in publish packet = packet len - (len of topic size + topic itself) +
    // + len of pkt id when qos level > 0
    payloadlen = len - (sizeof(topiclen) + topiclen);
    // pkt id is a variable field
    if (header.bits.qos > AT_MOST_ONCE)
    {
        msg >> pkt_id;
        payloadlen -= sizeof(pkt_id);
    }

    payload.resize(payloadlen);
    msg >> payload;
}

void mqtt_suback::unpack(tps::net::message& msg)
{
    uint32_t len = mqtt_decode_length(msg);

    msg >> pkt_id;

    uint16_t rcsBytes = len - sizeof(pkt_id);
    rcs.resize(rcsBytes);
    msg >> rcs;
}

void mqtt_ack::unpack(tps::net::message& msg)
{
    mqtt_decode_length(msg);

    msg >> pkt_id;
}

void mqtt_packet::pack(tps::net::message& msg)
{
    msg << header.byte;
    mqtt_encode_length(msg, 0);
}

void mqtt_publish::pack(tps::net::message& msg)
{
    msg << header.byte;
    uint32_t remainingLen = sizeof(topiclen) +
                            topiclen + payloadlen;
    if (header.bits.qos > AT_MOST_ONCE)
        remainingLen += sizeof(pkt_id);
    mqtt_encode_length(msg, remainingLen);

    msg << topiclen;
    msg << topic;
    if (header.bits.qos > AT_MOST_ONCE)
        msg << pkt_id;
    msg << payload;
}

void mqtt_connack::pack(tps::net::message& msg)
{
    msg << header.byte;
    mqtt_encode_length(msg, sizeof(sp));

    msg << sp.byte;
    msg << rc;
}

void mqtt_suback::pack(tps::net::message& msg)
{
    msg << header.byte;
    mqtt_encode_length(msg, sizeof(pkt_id) + rcs.size());

    msg << pkt_id;
    msg << rcs;
}

void mqtt_ack::pack(tps::net::message& msg)
{
    msg << header.byte;
    mqtt_encode_length(msg, sizeof(pkt_id));

    msg << pkt_id;
}
