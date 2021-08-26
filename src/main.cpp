#include <iostream>
#include <assert.h>
#include <math.h>
#include "mqtt.h"

using namespace std;

void test_simple_pack_unpack()
{
    std::vector<uint8_t> buf(64);
    uint32_t iterator = 0;
//    uint8_t *iterator = buf.data();

#define PRINT_N_CLEAR(buf) \
    for (uint i = 0; i < buf.size(); i++) \
        printf("%x\n", buf[i]); \
    buf.clear();

    // PACK
    std::vector<uint8_t> u8s = {0, 100, 255};
    for (auto elem: u8s)
        pack(buf, iterator, elem);
    assert(buf[0] == u8s[0]);
    assert(buf[1] == u8s[1]);
    assert(buf[2] == u8s[2]);
    PRINT_N_CLEAR(buf);

    std::vector<int8_t> i8s = {10, 110, 127, -128, -100, -1};
    for (auto elem: i8s)
        pack(buf, iterator, elem);
    assert(int8_t(buf[3]) == i8s[0]);
    assert(int8_t(buf[4]) == i8s[1]);
    assert(int8_t(buf[5]) == i8s[2]);
    assert(int8_t(buf[6]) == i8s[3]);
    assert(int8_t(buf[7]) == i8s[4]);
    assert(int8_t(buf[8]) == i8s[5]);
    PRINT_N_CLEAR(buf);

    std::vector<uint16_t> u16s = {0, 1000, 65535};
    for (auto elem: u16s)
        pack(buf, iterator, elem);
    assert(buf[9]  == uint8_t(u16s[0] >> 8) && buf[10] == uint8_t(u16s[0] >> 0));
    assert(buf[11] == uint8_t(u16s[1] >> 8) && buf[12] == (uint8_t(u16s[1] >> 0)));
    assert(buf[13] == uint8_t(u16s[2] >> 8) && buf[14] == (uint8_t(u16s[2] >> 0)));
    PRINT_N_CLEAR(buf);

    std::vector<int16_t> i16s = {0, 1000, 32767, -32768, -1000};
    for (auto elem: i16s)
        pack(buf, iterator, elem);
    assert(int16_t(buf[15]) == uint8_t(i16s[0]) && buf[16] == uint8_t(i16s[0]));
    assert(int16_t(buf[17]) == uint8_t(i16s[1] >> 8) && buf[18] == uint8_t(i16s[1] >> 0));
    assert(int16_t(buf[19]) == uint8_t(i16s[2] >> 8) && buf[20] == uint8_t(i16s[2] >> 0));
    assert(int16_t(buf[21]) == uint8_t(i16s[3] >> 8) && buf[22] == uint8_t(i16s[3] >> 0));
    assert(int16_t(buf[23]) == uint8_t(i16s[4] >> 8) && buf[24] == uint8_t(i16s[4] >> 0));
    PRINT_N_CLEAR(buf);

    std::vector<uint32_t> u32s = {1, 65534, 0xffffffff};
    for (auto elem: u32s)
        pack(buf, iterator, elem);
    assert(buf[25] == uint8_t(u32s[0] >> 24) && buf[26] == uint8_t(u32s[0] >> 16) &&
           buf[27] == uint8_t(u32s[0] >> 8) && buf[28] == uint8_t(u32s[0] >> 0));
    assert(buf[29] == uint8_t(u32s[1] >> 24) && buf[30] == uint8_t(u32s[1] >> 16) &&
           buf[31] == uint8_t(u32s[1] >> 8) && buf[32] == uint8_t(u32s[1] >> 0));
    assert(buf[33] == uint8_t(u32s[2] >> 24) && buf[34] == uint8_t(u32s[2] >> 16) &&
           buf[35] == uint8_t(u32s[2] >> 8) && buf[36] == uint8_t(u32s[2] >> 0));
    PRINT_N_CLEAR(buf);

    // UNPACK
//    iterator--;
    iterator = 0;

//    for (int i = u8s.size()-1; i >= 0; i--)
    for (int i = 0; i < u8s.size(); i++)
    {
        uint8_t elem = 0;
        unpack(buf, iterator, elem);
        assert(elem == u8s[i]);
    }

    for (int i = 0; i < i8s.size(); i++)
    {
        int8_t elem = 0;
        unpack(buf, iterator, elem);
        assert(elem == i8s[i]);
    }

    for (int i = 0; i < u16s.size(); i++)
    {
        uint16_t elem = 0;
        unpack(buf, iterator, elem);
        assert(elem == u16s[i]);
    }

    for (int i = 0; i < i16s.size(); i++)
    {
        int16_t elem = 0;
        unpack(buf, iterator, elem);
        assert(elem == i16s[i]);
    }

    for (int i = 0; i < u32s.size(); i++)
    {
        uint32_t elem = 0;
        unpack(buf, iterator, elem);
        assert(elem == u32s[i]);
    }
#undef PRINT_N_CLEAR
}

void test_mqtt_encode_length_single_input()
{
    std::vector<uint8_t> buf(4, 0);
    uint32_t iterator = 0;

    for (uint64_t len = 0; len < pow(2, 28)-1; len++)
    {
//        printf("IN :%ld\n", len);
        mqtt_encode_length(buf, iterator, len);
        iterator = 0;

        uint64_t result = mqtt_decode_length(buf, iterator);
//        printf("OUT:%ld\n", result);

        assert(result == len);
        buf.clear();
        iterator = 0;
    }
}

void test_mqtt_encode_length_single_input_multiple_input()
{
    std::vector<uint8_t> buf(pow(2, 28), 0);
    uint32_t iterator = 0;

    for (uint64_t len = 0; len < pow(2, 28)-1; len++)
    {
//        printf("IN :%ld\n", len);
        uint8_t bytesWritten = mqtt_encode_length(buf, iterator, len);

        iterator -= bytesWritten;
        uint64_t result = mqtt_decode_length(buf, iterator);
//        printf("OUT:%ld\n", result);

        assert(result == len);
    }
}


void test_pack_unpack()
{
    {
        // PINGREQ
        mqtt_header hdr(1, AT_MOST_ONCE, 0, PINGREQ);
        auto pkt0 = mqtt_packet_create<mqtt_packet>(hdr.byte);
        auto ret = pack_mqtt_packet(*pkt0);

        mqtt_packet pkt1 = {};
        unpack_mqtt_packet(*ret, pkt1);

        assert(pkt0->header.byte == pkt1.header.byte);
    }

    {
        // PINGRESP
        mqtt_header hdr(0, AT_LEAST_ONCE, 1, PINGRESP);
        auto pkt0 = mqtt_packet_create<mqtt_packet>(hdr.byte);
        auto ret = pack_mqtt_packet(*pkt0);

        mqtt_packet pkt1 = {};
        unpack_mqtt_packet(*ret, pkt1);

        assert(pkt0->header.byte == pkt1.header.byte);
    }

    {
        // PUBLISH
        mqtt_header hdr(1, AT_LEAST_ONCE, 0, PUBLISH);
        std:: string topic = "topic", msg = "message";
        auto pkt0 = mqtt_packet_create<mqtt_publish>(hdr.byte, 128,
                                                     topic.length(), topic,
                                                     msg.length(), msg);
        auto ret = pack_mqtt_packet(*pkt0);

        mqtt_publish pkt1 = {};
        unpack_mqtt_packet(*ret, pkt1);

        assert(pkt0->header.byte == pkt1.header.byte);
        assert(pkt0->pkt_id == pkt1.pkt_id);
        assert(pkt0->topiclen == pkt1.topiclen);
        assert(pkt0->topic == pkt1.topic);
        assert(pkt0->payloadlen == pkt1.payloadlen);
        assert(pkt0->payload == pkt1.payload);
    }
}

int main()
{

//    test_simple_pack_unpack();
//    test_mqtt_encode_length_single_input();
//    test_mqtt_encode_length_single_input_multiple_input();
    test_pack_unpack();

    printf("END\n");
    return 0;
}
