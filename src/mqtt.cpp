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

//int32_t mqtt_connect::handle(closure &cb)
//{
//    if (sol.clients.find(payload.client_id) != sol.clients.end())
//    {
//        // Already connected client, 2 CONNECT packet should be interpreted as
//        // a violation of the protocol, causing disconnection of the client

//        sol_info("Received double CONNECT from %s, disconnecting client",
//                 payload.client_id.data());

//        close(cb.fd);
//        sol.clients.erase(payload.client_id);
//        sol.closures.erase(cb.closure_id);

//        // Update stats
//        info.nclients--;
//        info.nconnections--;

//        return -REARM_W;
//    }
//    sol_info("New client connected as %s (c%i, k%u)",
//             payload.client_id.data(),
//             vhdr.bits.clean_session,
//             payload.keepalive);

//    /*
//     * Add the new connected client to the global map, if it is already
//     * connected, kick him out accordingly to the MQTT v3.1.1 specs.
//     */
//    std::shared_ptr<struct sol_client> new_client(new sol_client);
//    new_client->fd = cb.fd;
//    new_client->client_id = payload.client_id;
//    sol.clients.insert(std::make_pair(new_client->client_id, *new_client));

//    /* Substitute fd on callback with closure */
//    cb.obj = new_client.get();

//    /* Respond with a connack */
////    mqtt_packet *response = malloc(sizeof(*response));
////    unsigned char byte = CONNACK_BYTE;

//    // TODO check for session already present

////    if (pkt->connect.bits.clean_session == false)
////        new_client->session.subscriptions = list_create(NULL);

//    uint8_t session_present = 0;
//    uint8_t connect_flags = 0 | (session_present & 0x1) << 0;
//    uint8_t rc = 0;  // 0 means connection accepted

//    if (vhdr.bits.clean_session)
//    {
//        session_present = 0;
//        rc = 0;
//    }
//    else
//    {
//        // TODO: handle this case
//    }
//    mqtt_connack response;
//    response.sp = session_present;
//    response.rc = rc;

////    response->connack = *mqtt_packet_connack(byte, connect_flags, rc);

////    cb->payload = bytestring_create(MQTT_ACK_LEN);
////    cb.payload.resize(MQTT_ACK_LEN);

////    unsigned char *p = pack_mqtt_packet(response, CONNACK);
//    response.pack(cb.payload);
////    memcpy(cb.payload.data(), p, MQTT_ACK_LEN);
////    free(p);

//    sol_debug("Sending CONNACK to %s (%u, %u)", payload.client_id.data(), session_present, rc);

////    free(response);

//    return REARM_W;
//}

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

//mqtt_packet* mqtt_packet::create(uint8_t id)
//{
//switch (id)
//{
//    case CONNECT:
//        return new mqtt_connect(id);
//    case CONNACK:
//        return new mqtt_connack(id);
//    case SUBSCRIBE:
//        return new mqtt_subscribe(id);
//    case UNSUBSCRIBE:
//        return new mqtt_unsubscribe(id);
//    case SUBACK:
//        return new mqtt_suback(id);
//    case PUBLISH:
//        return new mqtt_publish(id);
//    case PUBACK:
//    case PUBREC:
//    case PUBREL:
//    case PUBCOMP:
//        return new mqtt_ack(id);
//    case PINGREQ:
//    case PINGRESP:
//    case DISCONNECT:
//        return new mqtt_packet(id);
//    default:
//        return nullptr;
//}
//}

std::shared_ptr<mqtt_packet> mqtt_packet::create(std::vector<uint8_t> &buf)
{
    mqtt_header hdr;
    uint32_t iterator = 0;
    ::unpack(buf, iterator, hdr.byte);

    uint8_t type = hdr.bits.type;
//    mqtt_packet *pkt = create(hdr.bits.type);

    switch (type)
    {
        case CONNECT:
            return create<mqtt_connect>(type);
        case CONNACK:
            return create<mqtt_connack>(type);
//        case SUBSCRIBE:
//            return new mqtt_subscribe(id);
//        case UNSUBSCRIBE:
//            return new mqtt_unsubscribe(id);
//        case SUBACK:
//            return new mqtt_suback(id);
//        case PUBLISH:
//            return new mqtt_publish(id);
//        case PUBACK:
//        case PUBREC:
//        case PUBREL:
//        case PUBCOMP:
//            return new mqtt_ack(id);
//        case PINGREQ:
//        case PINGRESP:
//        case DISCONNECT:
//            return new mqtt_packet(id);
        default:
            return nullptr;
    }
}

uint64_t mqtt_packet::unpack(const std::vector<uint8_t> & buf)
{
    uint iterator = 0;
    ::unpack(buf, iterator, header.byte);
}

void pack_send()
{
    uint8_t sp = 0, rc = 0;

    std::shared_ptr<mqtt_packet> pkt(mqtt_packet::create<mqtt_connack>(CONNACK, sp, rc, 1));

    std::vector<uint8_t> buf;
    pkt->pack(buf);

//    send(buf)
}

int recv_unpack()
{
    std::vector<uint8_t> buf;
    // recv(buf)
    std::shared_ptr<mqtt_packet> pkt(mqtt_packet::create(buf));

    switch (pkt->header.bits.type)
    {
        case SUBSCRIBE:
        case UNSUBSCRIBE:
        case PUBLISH:
        case PUBREC:
        case PUBREL:
        case PUBCOMP:
        case PINGREQ:
        case PINGRESP:
        case DISCONNECT:
            pkt->unpack(buf);
            break;
        default:
            return -1;
    }
    mqtt_connack c;
    c.unpack(buf);

    return 0;
    // process_pkt(pkt)
}

uint64_t mqtt_connect::unpack(const std::vector<uint8_t>& buf)
{
    uint32_t iterator = 0;
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

uint64_t mqtt_subscribe::unpack(const std::vector<uint8_t> &buf)
{
    uint32_t iterator = 0;
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

uint64_t mqtt_unsubscribe::unpack(const std::vector<uint8_t> &buf)
{
    uint32_t iterator = 0;
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

uint64_t mqtt_publish::unpack(const std::vector<uint8_t> &buf)
{
    uint32_t iterator = 0;
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

uint64_t mqtt_ack::unpack(const std::vector<uint8_t>& buf)
{
    uint32_t iterator = 0;
    uint64_t len = mqtt_decode_length(buf, iterator);

    ::unpack(buf, iterator, pkt_id);

    return len;
}

uint64_t mqtt_suback::unpack(const std::vector<uint8_t>& buf)
{
    uint32_t iterator = 0;
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
