#include <assert.h>
#include <math.h>
#include <boost/algorithm/string.hpp>
#include "mqtt.h"
#include "server.h"
#include "NetCommon/net_client.h"
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
*/


/*
============================================
    '#' - multi-level wildcard

    For example, if a Client subscribes to “sport/tennis/player1/#”, it would receive
    messages published using these topic names:

·         “sport/tennis/player1”
·         “sport/tennis/player1/ranking”
·         “sport/tennis/player1/score/wimbledon”

    Other examples:

·         “sport/#” also matches the singular “sport”, since # includes the parent level.
·         “#” is valid and will receive every Application Message
·         “sport/tennis/#” is valid
·         “sport/tennis#” is not valid
·         “sport/tennis/#/ranking” is not valid

============================================
    '+' - single level wildcard

    For example, “sport/tennis/+” matches:
·         “sport/tennis/player1”
·         “sport/tennis/player2”,
·         but not “sport/tennis/player1/ranking”.

    Also, because the single-level wildcard matches only a single level,
    “sport/+” does not match “sport” but it does match “sport/”.

·         “+” is valid
·         “+/tennis/#” is valid
·         “sport+” is not valid
·         “sport/+/player1” is valid
·         “/finance” matches “+/+” and “/+”, but not “+”
============================================
*/

std::vector<std::shared_ptr<topic_t>> get_matching_topics(trie<topic_t>& topics, const std::string &topicFilter)
{
    std::vector<std::shared_ptr<topic_t>> matches;
    std::string prefix = "";

    bool multilvl = false;
    // if there is a '#' wildcard
    if (topicFilter.find("#") != std::string::npos)
    {
        // '#' is last symbol
        if (topicFilter[topicFilter.length()-1] == '#')
        {
            // if topicFilter == "#"
            if (topicFilter.length() == 1)
            {
                // every topic is a match
                topics.apply_func(prefix, [&prefix, &matches](trie_node<topic_t>* t)
                {
                    if (t->data->name == prefix)
                        return;

                    matches.push_back(t->data);
                });
                return matches;
            }
            multilvl = true;
        }
        else
        {// ERROR: '#' is not last symbol
            return matches;
        }
    }

    if (topicFilter.find("+") != std::string::npos)
    {
        std::vector<boost::iterator_range<std::string::const_iterator>> singlelvl;
        boost::find_all(singlelvl, topicFilter, "/+");
//        if (topicFilter[0] == '+')
//            singlelvl.emplace(singlelvl.cbegin(),
//                              boost::iterator_range<std::string::const_iterator>(topicFilter.cbegin(), topicFilter.cbegin()+1));

        std::vector<trie_node<topic_t>*> matchesSoFar;

        if (topicFilter[0] == '+')
        {
            topics.apply_func_key(prefix, nullptr, '/',
                [&matchesSoFar](trie_node<topic_t>* n)
            {
                matchesSoFar.push_back(n);
            });
        }

        auto start = topicFilter.cbegin();
        auto end = singlelvl[0].begin();
        prefix = std::string(start, end+1);
        topics.apply_func_key(prefix, nullptr, '/',
            [&matchesSoFar](trie_node<topic_t>* n)
        {
            matchesSoFar.push_back(n);
        });

        for (uint i = 0; i < singlelvl.size()-1; i++)
        {
            start = singlelvl[i].end()+1;
            end = singlelvl[i+1].begin()+1;
            prefix = std::string(start, end);

            bool delFlag = false;
            bool found = false;
            for (uint j = 0; j < matchesSoFar.size(); j++)
            {
                topics.apply_func_key(prefix, matchesSoFar[j], '/',
                    [j, &found, &matchesSoFar](trie_node<topic_t>* n)
                {
                    matchesSoFar[j] = n;
                    found = true;
                });
                if (!found)
                {
                    matchesSoFar[j] = nullptr;
                    delFlag = true;
                }
                found = false;
            }
            if (delFlag)
                matchesSoFar.erase(
                            std::remove(matchesSoFar.begin(), matchesSoFar.end(), nullptr));
        }

        start = singlelvl[singlelvl.size()-1].end()+1;
        end = topicFilter.cend();
        prefix = std::string(start, end);
        for (uint j = 0; j < matchesSoFar.size(); j++)
        {
            auto temp = topics.find(prefix, matchesSoFar[j]);
            if (temp && temp->data)
                matches.push_back(temp->data);
        }
    }


    return matches;
}

void test_trie1(trie<topic_t>& topics)
{
    auto add = [&topics](const std::string& topic)
        {topics.insert(topic, std::make_shared<topic_t>(topic));};
    add("a");
    add("aaa/");
    add("aaa/b");
    add("aaa/bbb");
    add("aaa/bbb/c");
    add("aaa/a/cc");

    add("/");
    add("/aaa");
    add("/aaa/");
    add("/aaa/b");
    add("/aaa/bbb/c");
    // test1 - return all topics
    auto matches = get_matching_topics(topics, "#");
    for (auto match: matches)
        std::cout << match->name << "\n";
}

void test_trie2(trie<topic_t>& topics)
{
    auto add = [&topics](const std::string& topic)
        {topics.insert(topic, std::make_shared<topic_t>(topic));};
    std::string topicFilter = "/+/b";
    std::vector<std::string> valid = {"/a/b", "/aaaaaaa/b"};
    std::vector<std::string> invalid = {"a/b", "/a/bb", "/a/b/"};
    for (auto el: invalid)
        add(el);
    for (auto el: valid)
        add(el);

    // test2 - return all topics that match prefix "+"
    auto matches = get_matching_topics(topics, topicFilter);
    for (uint i = 0; i < matches.size(); i++)
        assert(valid[i] == matches[i]->name);
}

void test_trie3(trie<topic_t>& topics)
{
    auto add = [&topics](const std::string& topic)
        {topics.insert(topic, std::make_shared<topic_t>(topic));};
    std::string topicFilter = "a/+/b/+/c";
    std::vector<std::string> valid = {"a/1/b/1/c", "a/22/b/22/c"};
    std::vector<std::string> invalid = {"a/333/e/333/c", "a/4444/b/4444/d",
                                        "a/1/b/1/c/", "/a/1/b/1/c", "b/1/b/1/c"};
    for (auto el: invalid)
        add(el);
    for (auto el: valid)
        add(el);

    // test2 - return all topics that match prefix "+"
    auto matches = get_matching_topics(topics, topicFilter);
    for (uint i = 0; i < matches.size(); i++)
        assert(valid[i] == matches[i]->name);
}

void test_trie()
{
    std::vector<trie<topic_t>> t;
    t.resize(10);

//    test_trie1(t[0]);
    test_trie2(t[1]);
    test_trie3(t[2]);
}

void tests()
{
    test_trie();
//    test_simple_pack_unpack();
//    test_mqtt_encode_decode_length();
//    test_pack_unpack();
}

#define CONNECT_BYTE  0x10
#define PINGREQ_BYTE  0xC0

template <typename T>
class MQTTClient: public tps::net::client_interface<T>
{
public:
    void pack_connect(mqtt_connect& con, tps::net::message<mqtt_header>& msg)
    {
        msg.hdr.byte.bits.type = con.header.bits.type;
        msg.writeHdrSize += mqtt_encode_length(msg, 12+con.payload.client_id.size());

        std::string protocolName = "MQTT";
        uint16_t protocolLen = protocolName.size();
        msg << protocolLen;
        msg << protocolName;

        uint8_t protocolLevel = 4;
        msg << protocolLevel;

        mqtt_connect::variable_header vhdr(0);
        vhdr.bits.clean_session = con.vhdr.bits.clean_session;
        msg << vhdr;

        uint16_t keepalive = con.payload.keepalive;
        msg << keepalive;

        uint16_t clientIDLen = con.payload.client_id.size();
        msg << clientIDLen;
        msg << con.payload.client_id;
    }

    void unpack_connack(tps::net::message<mqtt_header>& msg, mqtt_connack& con)
    {
        con.header.byte = msg.hdr.byte.byte;
        msg >> con.sp.byte;
        msg >> con.rc;
    }

    void publish()
    {
        static uint16_t pktID = 0;

        mqtt_publish pub(PUBLISH_BYTE);
        pub.header.bits.qos = 1;
        pub.header.bits.retain = 0;
        pub.pkt_id = pktID++;
        pub.topic = "/example";
        pub.topiclen = pub.topic.size();
        pub.payload = "message_example";
        pub.payloadlen = pub.payload.size();

        tps::net::message<mqtt_header>pubmsg;
        pub.pack(pubmsg);
        this->send(std::move(pubmsg));
    }

    void subscribe()
    {

    }

    void pingreq()
    {
        mqtt_pingreq pingreq(PINGREQ_BYTE);
        tps::net::message<T> msg;

        pingreq.pack(msg);
        this->send(std::move(msg));
    }
};

//#define CLIENT

int main()
{
    tests();
    std::cout << "[" << std::this_thread::get_id() << "]MAIN THREAD\n";
    return 0;

#ifdef CLIENT
    MQTTClient<mqtt_header> client;
    client.connect("127.0.0.1", 5000);

    tps::net::tsqueue<int>input;
    std::thread IOThread = std::thread([&input]()
    {
        while (1)
        {
            std::string in;
            std::getline(std::cin, in);
            try {
                input.push_back(std::stoi(in));
            } catch (...) {
                // in case stoi fails
            }
        }
    });

    mqtt_connect con(CONNECT_BYTE);
    con.vhdr.bits.clean_session = 1;
    con.payload.client_id = "foo";
    con.payload.keepalive = 0xffff;

    tps::net::message<mqtt_header>conmsg;
    client.pack_connect(con, conmsg);
    client.send(std::move(conmsg));

    while (1)
    {
        if (client.is_connected())
        {
            if (!input.empty())
            {
                switch (input.pop_front())
                {
                    case 1: client.publish(); break;
                    case 2: client.subscribe(); break;
                    case 3: client.pingreq(); break;
                    case 4: return 0;
                }
            }

            if (!client.incoming().empty())
            {
//                std::cout << "[" << std::this_thread::get_id() << "]MAIN POP BEFORE\n";
                tps::net::message<mqtt_header> msg = client.incoming().pop_front().msg;
//                std::cout << "[" << std::this_thread::get_id() << "]MAIN POP AFTER\n";
                switch (packet_type(msg.hdr.byte.bits.type))
                {
                    case packet_type::CONNACK:
                    {
                        mqtt_connack resp;
                        client.unpack_connack(msg, resp);
                        std::cout << "\t{CONNACK}\n" << resp;
                        break;
                    }
                    case packet_type::PUBACK:
                    {
                        mqtt_puback resp;
                        resp.unpack(msg);
                        std::cout << "\t{PUBACK}\n" << resp;
                        break;
                    }
                    case packet_type::PINGRESP:
                    {
                        std::cout << "\t{PINGRESP}\n";
                        break;
                    }
                    default:
                        std::cout << "DEFAULT: " << msg.hdr.byte.bits.type << "\n";
                        break;
                }
            }
        }
        usleep(100*1000);
    }
#else
    server<mqtt_header> broker(5000);
    broker.start();
    broker.update();
#endif
    std::cout << "END\n";
    return 0;
}
