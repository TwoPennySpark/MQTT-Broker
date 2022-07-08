#include <assert.h>
#include <math.h>
#include "mqtt.h"
#include "server.h"
#include "NetCommon/net_message.h"

/*
void test_simple_pack_unpack()
{
    tps::net::message msg;

    // PACK
    std::vector<uint8_t> u8s = {0, 100, 255};
    for (auto elem: u8s)
        msg << elem;
    assert(msg.body[0] == u8s[0]);
    assert(msg.body[1] == u8s[1]);
    assert(msg.body[2] == u8s[2]);

    std::vector<int8_t> i8s = {10, 110, 127, -128, -100, -1};
    for (auto elem: i8s)
        msg << elem;
    assert(int8_t(msg.body[3]) == i8s[0]);
    assert(int8_t(msg.body[4]) == i8s[1]);
    assert(int8_t(msg.body[5]) == i8s[2]);
    assert(int8_t(msg.body[6]) == i8s[3]);
    assert(int8_t(msg.body[7]) == i8s[4]);
    assert(int8_t(msg.body[8]) == i8s[5]);

    std::vector<uint16_t> u16s = {0, 1000, 65535};
    for (auto elem: u16s)
        msg << elem;
    assert(msg.body[10]  == uint8_t(u16s[0] >> 8) && msg.body[9] == uint8_t(u16s[0] >> 0));
    assert(msg.body[12] == uint8_t(u16s[1] >> 8) && msg.body[11] == (uint8_t(u16s[1] >> 0)));
    assert(msg.body[14] == uint8_t(u16s[2] >> 8) && msg.body[13] == (uint8_t(u16s[2] >> 0)));

    std::vector<int16_t> i16s = {0, 1000, 32767, -32768, -1000};
    for (auto elem: i16s)
        msg << elem;
    assert(int16_t(msg.body[16]) == uint8_t(i16s[0]) && msg.body[15] == uint8_t(i16s[0]));
    assert(int16_t(msg.body[18]) == uint8_t(i16s[1] >> 8) && msg.body[17] == uint8_t(i16s[1] >> 0));
    assert(int16_t(msg.body[20]) == uint8_t(i16s[2] >> 8) && msg.body[19] == uint8_t(i16s[2] >> 0));
    assert(int16_t(msg.body[22]) == uint8_t(i16s[3] >> 8) && msg.body[21] == uint8_t(i16s[3] >> 0));
    assert(int16_t(msg.body[24]) == uint8_t(i16s[4] >> 8) && msg.body[23] == uint8_t(i16s[4] >> 0));

    std::vector<uint32_t> u32s = {1, 65534, 0xffffffff};
    for (auto elem: u32s)
        msg << elem;
    assert(msg.body[28] == uint8_t(u32s[0] >> 24) && msg.body[27] == uint8_t(u32s[0] >> 16) &&
           msg.body[26] == uint8_t(u32s[0] >> 8) && msg.body[25] == uint8_t(u32s[0] >> 0));
    assert(msg.body[32] == uint8_t(u32s[1] >> 24) && msg.body[31] == uint8_t(u32s[1] >> 16) &&
           msg.body[30] == uint8_t(u32s[1] >> 8) && msg.body[29] == uint8_t(u32s[1] >> 0));
    assert(msg.body[36] == uint8_t(u32s[2] >> 24) && msg.body[35] == uint8_t(u32s[2] >> 16) &&
           msg.body[34] == uint8_t(u32s[2] >> 8) && msg.body[33] == uint8_t(u32s[2] >> 0));


    // UNPACK
    for (int i = 0; i < u8s.size(); i++)
    {
        uint8_t elem = 0;
        msg >> elem;
        assert(elem == u8s[i]);
    }

    for (int i = 0; i < i8s.size(); i++)
    {
        int8_t elem = 0;
        msg >> elem;
        assert(elem == i8s[i]);
    }

    for (int i = 0; i < u16s.size(); i++)
    {
        uint16_t elem = 0;
        msg >> elem;
        assert(elem == u16s[i]);
    }

    for (int i = 0; i < i16s.size(); i++)
    {
        int16_t elem = 0;
        msg >> elem;
        assert(elem == i16s[i]);
    }

    for (int i = 0; i < u32s.size(); i++)
    {
        uint32_t elem = 0;
        msg >> elem;
        assert(elem == u32s[i]);
    }
}

void test_mqtt_encode_decode_length()
{
    tps::net::message<T> msg;

    for (uint64_t len = 0; len < pow(2, 28)-1; len++)
    {
//        printf("IN :%ld\n", len);
        mqtt_encode_length(msg, len);

        uint64_t result = mqtt_decode_length(msg);
//        printf("OUT:%ld\n", result);

        assert(result == len);
    }
}

void test_pack_unpack()
{
    {
        // PINGREQ
        mqtt_header hdr(1, AT_MOST_ONCE, 0, PINGREQ);
        mqtt_packet pkt0(hdr.byte);

        tps::net::message msg;
        pkt0.pack(msg);

        auto pkt1 = mqtt_packet::create(msg);
        assert(pkt0.header.byte == pkt1->header.byte);
    }

    {
        // PINGRESP
        mqtt_header hdr(0, AT_LEAST_ONCE, 1, PINGRESP);
        mqtt_packet pkt0(hdr.byte);

        tps::net::message msg;
        pkt0.pack(msg);

        auto pkt1 = mqtt_packet::create(msg);
        assert(pkt0.header.byte == pkt1->header.byte);
    }

    {
        // PUBLISH
        mqtt_header hdr(1, AT_LEAST_ONCE, 0, PUBLISH);
        std:: string topic = "topic", payload = "message";
        mqtt_publish pkt0(hdr.byte, 128, topic.length(), topic, payload.length(), payload);

        tps::net::message msg;
        pkt0.pack(msg);

        auto pkt1p = mqtt_packet::create(msg);
        std::shared_ptr<mqtt_publish> pkt1 = std::dynamic_pointer_cast<mqtt_publish>(pkt1p);
        assert(pkt0.header.byte == pkt1->header.byte);
        assert(pkt0.pkt_id == pkt1->pkt_id);
        assert(pkt0.topiclen == pkt1->topiclen);
        assert(pkt0.topic == pkt1->topic);
        assert(pkt0.payloadlen == pkt1->payloadlen);
        assert(pkt0.payload == pkt1->payload);
    }

    {
        // SUBACK
        mqtt_header hdr(0, EXACTLY_ONCE, 1, SUBACK);
        std::vector<uint8_t> rcs = {0, 255, 100, 1};
        mqtt_suback pkt0(hdr.byte, 65535, rcs);

        tps::net::message msg;
        pkt0.pack(msg);

        auto pkt1p = mqtt_packet::create(msg);
        std::shared_ptr<mqtt_suback> pkt1 = std::dynamic_pointer_cast<mqtt_suback>(pkt1p);

        assert(pkt0.header.byte == pkt1->header.byte);
        assert(pkt0.pkt_id == pkt1->pkt_id);
        for (uint i = 0; i < pkt0.rcs.size(); i++)
            assert(pkt0.rcs[i] == pkt1->rcs[i]);
    }

    {
        // PUBACK(PUBREC, PUBREL, PUBCOMP)
        mqtt_header hdr(1, AT_MOST_ONCE, 0, PUBACK);
        mqtt_ack pkt0(hdr.byte, 10);

        tps::net::message msg;
        pkt0.pack(msg);

        auto pkt1p = mqtt_packet::create(msg);
        std::shared_ptr<mqtt_ack> pkt1 = std::dynamic_pointer_cast<mqtt_ack>(pkt1p);

        assert(pkt0.header.byte == pkt1->header.byte);
        assert(pkt0.pkt_id == pkt1->pkt_id);
    }
}

void tests()
{
    test_simple_pack_unpack();
//    test_mqtt_encode_decode_length();
//    test_pack_unpack();
}

*/

int main()
{
//    tests();
    server<mqtt_header> broker(5000);
    broker.start();
    broker.update();

    std::cout << "END\n";
    return 0;
}
