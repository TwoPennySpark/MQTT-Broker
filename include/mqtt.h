#ifndef MQTT_H
#define MQTT_H

#include <memory>
#include "pack.h"

#define MQTT_HEADER_LEN_MIN 2 // header(1 byte) + min possible len of Remaining Length field(1 byte)
#define MQTT_HEADER_LEN_MAX 5 // header(1 byte) + max possible len of Remaining Length field(4 byte)
#define MQTT_ACK_LEN    4

/*
 * Stub bytes, useful for generic replies, these represent the first byte in
 * the fixed header
 */
#define CONNACK_BYTE  0x20
#define PUBLISH_BYTE  0x30
#define PUBACK_BYTE   0x40
#define PUBREC_BYTE   0x50
#define PUBREL_BYTE   0x60
#define PUBCOMP_BYTE  0x70
#define SUBACK_BYTE   0x90
#define UNSUBACK_BYTE 0xB0
#define PINGRESP_BYTE 0xD0

/* Message types */
enum packet_type
{
    CONNECT     = 1,
    CONNACK     = 2,
    PUBLISH     = 3,
    PUBACK      = 4,
    PUBREC      = 5,
    PUBREL      = 6,
    PUBCOMP     = 7,
    SUBSCRIBE   = 8,
    SUBACK      = 9,
    UNSUBSCRIBE = 10,
    UNSUBACK    = 11,
    PINGREQ     = 12,
    PINGRESP    = 13,
    DISCONNECT  = 14
};

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

enum qos_level { AT_MOST_ONCE, AT_LEAST_ONCE, EXACTLY_ONCE };

union mqtt_header
{
    mqtt_header() = default;
    mqtt_header(uint8_t _byte): byte(_byte) {}
    mqtt_header(uint8_t _retain, uint8_t _qos, uint8_t _dup, uint8_t _type): bits(_retain, _qos, _dup, _type){}
    uint8_t byte;
    struct hdr
    {
        hdr() = default;
        hdr(uint8_t _retain, uint8_t _qos, uint8_t _dup, uint8_t _type): retain(_retain), qos(_qos), dup(_dup), type(_type){}
        uint8_t retain : 1;
        uint8_t qos : 2;
        uint8_t dup : 1;
        uint8_t type : 4;
    };
    hdr bits;
};

struct mqtt_packet
{
    union mqtt_header header;
    mqtt_packet() = default;
    mqtt_packet(uint8_t _hdr): header(_hdr){}

    template<typename T, typename... Args>
    static std::shared_ptr<T> create(uint8_t hdr, Args... args)
    {
        return std::shared_ptr<T>(new T(hdr, args...));
    }

    static std::shared_ptr<mqtt_packet> create(std::vector<uint8_t>& buf);

    virtual uint64_t pack(std::vector<uint8_t>&);
    virtual uint64_t unpack(const std::vector<uint8_t>& buf, uint& iterator);

    virtual ~mqtt_packet() = default;
};

struct mqtt_connect: public mqtt_packet
{
    mqtt_connect() = default;
    mqtt_connect(uint8_t _hdr): mqtt_packet(_hdr){}
    mqtt_connect(uint8_t _hdr, uint8_t _vhdr, uint8_t _keepalive, std::string _client_id, std::string _username,
                 std::string _password, std::string _will_topic, std::string _will_message): mqtt_packet(_hdr),
                 vhdr(_vhdr), payload(_keepalive, _client_id, _username, _password, _will_topic, _will_message){}
    union variable_header
    {
        variable_header() = default;
        variable_header(uint8_t _byte): byte(_byte) {}

        uint8_t byte;
        struct {
            int8_t reserved : 1;
            uint8_t clean_session : 1;
            uint8_t will : 1;
            uint8_t will_qos : 2;
            uint8_t will_retain : 1;
            uint8_t password : 1;
            uint8_t username : 1;
        } bits;
    }/*vhdr*/;
    variable_header vhdr;

    struct payload
    {
        payload() = default;
        payload(uint8_t _keepalive, std::string _client_id, std::string _username,
                std::string _password, std::string _will_topic, std::string _will_message):
            keepalive(_keepalive), client_id(_client_id), username(_username), password(_password),
            will_topic(_will_topic), will_message(_will_message){}
        uint16_t keepalive;
        std::string client_id;
        std::string username;
        std::string password;
        std::string will_topic;
        std::string will_message;
    }/*payload*/;
    payload payload;

    uint64_t unpack(const std::vector<uint8_t>& buf, uint& iterator) override;
//    virtual int32_t handle(struct closure& cb) override;
};

struct mqtt_connack: public mqtt_packet
{
    mqtt_connack() = default;
    mqtt_connack(uint8_t _hdr): mqtt_packet (_hdr) {}
    mqtt_connack(uint8_t _hdr, uint8_t _session, uint8_t _rc): mqtt_packet (_hdr), sp(_session), rc(_rc){}
    union session
    {
        session() = default;
        session(uint8_t _byte): byte(_byte){}
        uint8_t byte;
        struct {
            uint8_t session_present : 1;
            uint8_t reserved : 7;
        } bits;
    };
    session sp;
    uint8_t rc;

    uint64_t pack(std::vector<uint8_t>& buf) override;
};

struct mqtt_subscribe: public mqtt_packet
{
    mqtt_subscribe() = default;
    mqtt_subscribe(uint8_t _hdr): mqtt_packet (_hdr) {}
    uint16_t pkt_id;
//    uint16_t tuples_len;
    struct tuple{
        uint16_t topic_len;
        std::string topic;
        uint8_t qos;
    };
    std::vector<tuple> tuples;

    uint64_t unpack(const std::vector<uint8_t>& buf, uint& iterator) override;
};

struct mqtt_unsubscribe: public mqtt_packet
{
    mqtt_unsubscribe() = default;
    mqtt_unsubscribe(uint8_t _hdr): mqtt_packet (_hdr) {}
    uint16_t pkt_id;
    uint16_t tuples_len;
    struct tuple{
        uint16_t topic_len;
        std::string topic;
    };
    std::vector<tuple> tuples;

    uint64_t unpack(const std::vector<uint8_t>& buf, uint& iterator) override;
};

struct mqtt_suback: public mqtt_packet
{
    mqtt_suback() = default;
    mqtt_suback(uint8_t _hdr): mqtt_packet (_hdr){}
    mqtt_suback(uint8_t _hdr, uint16_t _pkt_id, const std::vector<uint8_t>& _rcs):
                mqtt_packet(_hdr), pkt_id(_pkt_id), rcslen(_rcs.size()), rcs(_rcs){}
    uint16_t pkt_id;
    uint16_t rcslen;
    std::vector<uint8_t> rcs;

    uint64_t pack(std::vector<uint8_t>& buf) override;
    uint64_t unpack(const std::vector<uint8_t>& buf, uint& iterator) override;
};

struct mqtt_publish: public mqtt_packet
{
    mqtt_publish() = default;
    mqtt_publish(uint8_t _hdr): mqtt_packet (_hdr) {}
    mqtt_publish(uint8_t _hdr, uint16_t _pkt_id, uint16_t _topiclen, std::string& _topic,
                uint16_t _payloadlen, std::string& _payload):
                mqtt_packet(_hdr), pkt_id(_pkt_id), topiclen(_topiclen), topic(_topic),
                payloadlen(_payloadlen), payload(_payload){}
    uint16_t pkt_id;
    uint16_t topiclen;
    std::string topic;
    uint16_t payloadlen;
    std::string payload;

    uint64_t unpack(const std::vector<uint8_t>& buf, uint& iterator) override;
    uint64_t pack(std::vector<uint8_t>& buf) override;
};

struct mqtt_ack: public mqtt_packet
{
    mqtt_ack() = default;
    mqtt_ack(uint8_t _hdr): mqtt_packet (_hdr){}
    mqtt_ack(uint8_t _hdr, uint16_t _pkt_id): mqtt_packet(_hdr), pkt_id(_pkt_id){}
    uint16_t pkt_id;

    uint64_t unpack(const std::vector<uint8_t>& buf, uint& iterator) override;
    uint64_t pack(std::vector<uint8_t>& buf) override;
};

typedef struct mqtt_ack mqtt_puback;
typedef struct mqtt_ack mqtt_pubrec;
typedef struct mqtt_ack mqtt_pubrel;
typedef struct mqtt_ack mqtt_pubcomp;
typedef struct mqtt_ack mqtt_unsuback;
typedef union mqtt_header mqtt_pingreq;
typedef union mqtt_header mqtt_pingresp;
typedef union mqtt_header mqtt_disconnect;

uint8_t mqtt_encode_length(std::vector<uint8_t>& buf, uint32_t& iterator, size_t len);
uint64_t mqtt_decode_length(const std::vector<uint8_t>& buf, uint32_t& iterator);
uint64_t unpack_mqtt_packet(const std::vector<uint8_t>& buf, mqtt_packet& pkt);
std::shared_ptr<std::vector<uint8_t> > pack_mqtt_packet(const mqtt_packet& pkt);


#endif // MQTT_H
