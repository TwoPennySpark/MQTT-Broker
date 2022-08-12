#include <assert.h>
#include <math.h>
#include <boost/algorithm/string.hpp>
#include "mqtt.h"
#include "server.h"
#include "NetCommon/net_client.h"
#include "NetCommon/net_message.h"

void test_simple_pack_unpack()
{
    tps::net::message<mqtt_header> msg;

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
    tps::net::message<mqtt_header> msg;

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
        mqtt_header hdr;
        hdr.bits.retain = 1; hdr.bits.qos = AT_MOST_ONCE; hdr.bits.dup = 0;
        hdr.bits.type = uint8_t(packet_type::PINGREQ);
        mqtt_packet pkt0(hdr.byte);

        tps::net::message<mqtt_header> msg;
        pkt0.pack(msg);

        auto pkt1 = mqtt_packet::create(msg);
        assert(pkt0.header.byte == pkt1->header.byte);
    }

    {
        // PINGRESP
        mqtt_header hdr;
        hdr.bits.retain = 0; hdr.bits.qos = AT_LEAST_ONCE; hdr.bits.dup = 1;
        hdr.bits.type = uint8_t(packet_type::PINGRESP);
        mqtt_packet pkt0(hdr.byte);

        tps::net::message<mqtt_header> msg;
        pkt0.pack(msg);

        auto pkt1 = mqtt_packet::create(msg);
        assert(pkt0.header.byte == pkt1->header.byte);
    }

    {
        // PUBLISH
        mqtt_header hdr;
        hdr.bits.retain = 1; hdr.bits.qos = AT_LEAST_ONCE; hdr.bits.dup = 0;
        hdr.bits.type = uint8_t(packet_type::PUBLISH);
        std:: string topic = "topic", payload = "message";
        mqtt_publish pkt0(hdr.byte);
        pkt0.pkt_id = 128; pkt0.topic = topic; pkt0.topiclen = topic.size(); pkt0.payload = payload;

        tps::net::message<mqtt_header> msg;
        pkt0.pack(msg);

        auto pkt1p = mqtt_packet::create(msg);
        mqtt_publish pkt1 = dynamic_cast<mqtt_publish&>(*pkt1p);
        assert(pkt0.header.byte == pkt1.header.byte);
        assert(pkt0.pkt_id == pkt1.pkt_id);
        assert(pkt0.topiclen == pkt1.topiclen);
        assert(pkt0.topic == pkt1.topic);
        assert(pkt0.payload == pkt1.payload);
    }

    {
        // SUBACK
        mqtt_header hdr;
        hdr.bits.retain = 0; hdr.bits.qos = EXACTLY_ONCE; hdr.bits.dup = 1;
        hdr.bits.type = uint8_t(packet_type::SUBACK);
        std::vector<uint8_t> rcs = {0, 255, 100, 1};
        mqtt_suback pkt0(hdr.byte);
        pkt0.pkt_id = 65535; pkt0.rcs = rcs;

        tps::net::message<mqtt_header> msg;
        pkt0.pack(msg);

        auto pkt1p = mqtt_packet::create(msg);
        mqtt_suback pkt1 = dynamic_cast<mqtt_suback&>(*pkt1p);

        assert(pkt0.header.byte == pkt1.header.byte);
        assert(pkt0.pkt_id == pkt1.pkt_id);
        for (uint i = 0; i < pkt0.rcs.size(); i++)
            assert(pkt0.rcs[i] == pkt1.rcs[i]);
    }

    {
        // PUBACK(PUBREC, PUBREL, PUBCOMP)
        mqtt_header hdr;
        hdr.bits.retain = 1; hdr.bits.qos = AT_MOST_ONCE; hdr.bits.dup = 0;
        hdr.bits.type = uint8_t(packet_type::PUBACK);
        mqtt_ack pkt0(hdr.byte);
        pkt0.pkt_id = 10;

        tps::net::message<mqtt_header> msg;
        pkt0.pack(msg);

        auto pkt1p = mqtt_packet::create(msg);
        mqtt_ack pkt1 = dynamic_cast<mqtt_ack&>(*pkt1p);

        assert(pkt0.header.byte == pkt1.header.byte);
        assert(pkt0.pkt_id == pkt1.pkt_id);
    }
}

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
    std::vector<trie_node<topic_t>*> matchesSoFar;
    matchesSoFar.push_back(nullptr); // first search is from root
    std::string prefix = "";
    bool singleIsLast = false; // true when last symbol is '+'

    bool multilvl = false;
    // if there is a '#'(multi-lvl) wildcard, can mean one of two things:
    // 1) topicFilter == "#" or
    // 2) '#' is the last symbol of topicFilter
    if (topicFilter.find("#") != std::string::npos)
    {
        // if topicFilter == "#"
        if (topicFilter.length() == 1)
        {
            // every topic is a match
            topics.apply_func(prefix, nullptr, [&prefix, &matches](trie_node<topic_t>* t)
            {
                if (t->data->name == prefix)
                    return;

                matches.push_back(t->data);
            });
            return matches;
        }
        else // we need to evaluate the expr before '#' first
        {
            multilvl = true;
            prefix = topicFilter;
        }
    }

    // if there is a one or more '+'(single lvl) wildcards
    if (topicFilter.find("+") != std::string::npos)
    {
        std::vector<boost::iterator_range<std::string::const_iterator>> singlelvl;
        boost::find_all(singlelvl, topicFilter, "/+");
        if (topicFilter[0] == '+')
            singlelvl.emplace(singlelvl.cbegin(),
                          boost::iterator_range<std::string::const_iterator>(topicFilter.cbegin(), topicFilter.cbegin()+1));

        auto start = topicFilter.cbegin();
        auto end = singlelvl[0].begin()+1;
        if (topicFilter[0] == '+')
            start++;
        if (topicFilter[topicFilter.size()-1] == '+')
            singleIsLast = true;

        uint i = 0;
        std::vector<trie_node<topic_t>*> temp;
        do
        {
            // prefix = everything that comes before "/+", including '/'
            prefix = std::string(start, end);

            if (singleIsLast && i == singlelvl.size()-1)
                break;

            for (uint j = 0; j < matchesSoFar.size(); j++)
                // +/a   - from matchesSoFar[j](if nullptr - from root) go to "" (prefix) then find all topicnames until '/'
                // /+/a/ - from matchesSoFar[j](if nullptr - from root) go to / (prefix) then find all topicnames until '/'
                // /a/+/ - from matchesSoFar[j](if nullptr - from root) go to /a/ (prefix) then find all topicnames until '/'
                topics.apply_func_key(prefix, matchesSoFar[j], '/',
                    [&temp](trie_node<topic_t>* n) { temp.push_back(n); });

            matchesSoFar = std::move(temp);

            start = singlelvl[i].end()+1;
            end = singlelvl[i+1].begin()+1;
            i++;
        }while (i < singlelvl.size());

        start = singlelvl[singlelvl.size()-1].end()+1;
        end = topicFilter.cend();
        if (!singleIsLast)
            prefix = std::string(start, end);
    }

    if (multilvl)
    {
        prefix.pop_back();
        for (uint i = 0; i < matchesSoFar.size(); i++)
            topics.apply_func(prefix, matchesSoFar[i],
                [&matches](trie_node<topic_t>* n) { matches.push_back(n->data); });
    }
    else
    {
        if (singleIsLast)
        {
            for (uint i = 0; i < matchesSoFar.size(); i++)
                topics.find_all_data_until(prefix, matchesSoFar[i], '/',
                    [&matches](trie_node<topic_t>* n) { matches.push_back(n->data); });
        }
        else
        {
            for (uint i = 0; i < matchesSoFar.size(); i++)
            {
                auto n = topics.find(prefix, matchesSoFar[i]);
                if (n && n->data)
                    matches.push_back(n->data);
            }
        }
    }

    return matches;
}

void test_trie1()
{
    trie<topic_t> topics;
    auto add = [&topics](const std::string& topic)
        {topics.insert(topic, std::make_shared<topic_t>(topic));};
    std::vector<std::string> valid = {"a", "aaa/", "aaa/b", "aaa/bbb", "aaa/bbb/c",
                                     "aaa/a/cc", "/", "/aaa", "/aaa/b", "/aaa/bbb/c"};
    for (auto& el: valid)
        add(el);

    // test1 - return all topics
    auto matches = get_matching_topics(topics, "#");
    assert(matches.size() == valid.size());
    for (uint i = 0; i < matches.size(); i++)
    {
        if (std::find(valid.begin(), valid.end(), matches[i]->name) == valid.end())
            assert(1 == 0);
    }
}

void test_trie2()
{
    trie<topic_t> topics;
    auto add = [&topics](const std::string& topic)
        {topics.insert(topic, std::make_shared<topic_t>(topic));};
    std::string topicFilter = "/+/b";
    std::vector<std::string> valid = {"/a/b", "/aaaaaaa/b"};
    std::vector<std::string> invalid = {"a/b", "/a/bb", "/a/b/"};
    for (auto& el: invalid)
        add(el);
    for (auto& el: valid)
        add(el);

    // test2 - return all topics that match prefix "+"
    auto matches = get_matching_topics(topics, topicFilter);
    assert(matches.size() == valid.size());
    for (uint i = 0; i < matches.size(); i++)
    {
        if (std::find(valid.begin(), valid.end(), matches[i]->name) == valid.end())
            assert(1 == 0);
    }
}

void test_trie3()
{
    trie<topic_t> topics;
    auto add = [&topics](const std::string& topic)
        {topics.insert(topic, std::make_shared<topic_t>(topic));};
    std::string topicFilter = "a/+/b/+/c";
    std::vector<std::string> valid = {"a/1/b/1/c", "a/22/b/22/c"};
    std::vector<std::string> invalid = {"a/333/e/333/c", "a/4444/b/4444/d",
                                        "a/1/b/1/c/", "/a/1/b/1/c", "b/1/b/1/c"};
    for (auto& el: invalid)
        add(el);
    for (auto& el: valid)
        add(el);

    // test2 - return all topics that match prefix "+"
    auto matches = get_matching_topics(topics, topicFilter);
    assert(matches.size() == valid.size());
    for (uint i = 0; i < matches.size(); i++)
    {
        if (std::find(valid.begin(), valid.end(), matches[i]->name) == valid.end())
            assert(1 == 0);
    }
}

void test_trie4()
{
    trie<topic_t> topics;
    auto add = [&topics](const std::string& topic)
        {topics.insert(topic, std::make_shared<topic_t>(topic));};
    std::string topicFilter = "/+/b/+/c";
    std::vector<std::string> valid = {"/1/b/1/c", "/22/b/22/c"};
    std::vector<std::string> invalid = {"/333/e/333/c", "/4444/b/4444/d",
                                        "/1/b/1/c/", "/a/1/b/1/c"};
    for (auto& el: invalid)
        add(el);
    for (auto& el: valid)
        add(el);

    // test2 - return all topics that match prefix "+"
    auto matches = get_matching_topics(topics, topicFilter);
    assert(matches.size() == valid.size());
    for (uint i = 0; i < matches.size(); i++)
    {
        if (std::find(valid.begin(), valid.end(), matches[i]->name) == valid.end())
            assert(1 == 0);
    }
}

void test_trie5()
{
    trie<topic_t> topics;
    auto add = [&topics](const std::string& topic)
        {topics.insert(topic, std::make_shared<topic_t>(topic));};
    std::string topicFilter = "/+/+/c";
    std::vector<std::string> valid = {"/1/b/c", "/22/b/c"};
    std::vector<std::string> invalid = {"/333/e/333/c", "/4444/4444/d",
                                        "/1/b/c/", "6/b/c"};
    for (auto& el: invalid)
        add(el);
    for (auto& el: valid)
        add(el);

    // test2 - return all topics that match prefix "+"
    auto matches = get_matching_topics(topics, topicFilter);
    assert(matches.size() == valid.size());
    for (uint i = 0; i < matches.size(); i++)
    {
        if (std::find(valid.begin(), valid.end(), matches[i]->name) == valid.end())
            assert(1 == 0);
    }
}

void test_trie6()
{
    trie<topic_t> topics;
    auto add = [&topics](const std::string& topic)
        {topics.insert(topic, std::make_shared<topic_t>(topic));};
    std::string topicFilter = "+/a/bb/ccc";
    std::vector<std::string> valid = {"11wdsada90/a/bb/ccc", "a/a/bb/ccc",
                                      "/a/bb/ccc"};
    std::vector<std::string> invalid = {"/1/a/bb/ccc", "2/a/bb/ccc/",
                                        "/a/bb/ccc/"};
    for (auto& el: invalid)
        add(el);
    for (auto& el: valid)
        add(el);

    // test2 - return all topics that match prefix "+"
    auto matches = get_matching_topics(topics, topicFilter);
    assert(matches.size() == valid.size());
    for (uint i = 0; i < matches.size(); i++)
    {
        if (std::find(valid.begin(), valid.end(), matches[i]->name) == valid.end())
            assert(1 == 0);
    }
}

void test_trie7()
{
    trie<topic_t> topics;
    auto add = [&topics](const std::string& topic)
        {topics.insert(topic, std::make_shared<topic_t>(topic));};
    std::string topicFilter = "+/+/a/+/bb/";
    std::vector<std::string> valid = {"1/22/a/333/bb/", "f/g/a/3122/bb/",
                                       "/22/a/333/bb/"};
    std::vector<std::string> invalid = {"x/a/333/bb/", "/y/a/333/bb",
                                        "z/22/a/3/33/bb/", "f/g/a/333/bb/ccc"};
    for (auto& el: invalid)
        add(el);
    for (auto& el: valid)
        add(el);

    // test2 - return all topics that match prefix "+"
    auto matches = get_matching_topics(topics, topicFilter);
    assert(matches.size() == valid.size());
    for (uint i = 0; i < matches.size(); i++)
    {
        if (std::find(valid.begin(), valid.end(), matches[i]->name) == valid.end())
            assert(1 == 0);
    }
}

void test_trie8()
{
    trie<topic_t> topics;
    auto add = [&topics](const std::string& topic)
        {topics.insert(topic, std::make_shared<topic_t>(topic));};
    std::string topicFilter = "a/b/#";
    std::vector<std::string> valid = {"a/b/", "a/b/cc/ddd/eeee/fffff/",
                                       "a/b/c"};
    std::vector<std::string> invalid = {"x/a/b/", "a/b",
                                        "/a/b/c", "f/g/h"};
    for (auto& el: invalid)
        add(el);
    for (auto& el: valid)
        add(el);

    // test2 - return all topics that match prefix "+"
    auto matches = get_matching_topics(topics, topicFilter);
    assert(matches.size() == valid.size());
    for (uint i = 0; i < matches.size(); i++)
    {
        if (std::find(valid.begin(), valid.end(), matches[i]->name) == valid.end())
            assert(1 == 0);
    }
}

void test_trie9()
{
    trie<topic_t> topics;
    auto add = [&topics](const std::string& topic)
        {topics.insert(topic, std::make_shared<topic_t>(topic));};
    std::string topicFilter = "+/#";
    std::vector<std::string> valid = {"a/b/", "a/b/cc/ddd/eeee/fffff/",
                                       "a/b/c", "/a/b/c", "/f/g/h/"};
    std::vector<std::string> invalid = {"cccc"};
    for (auto& el: invalid)
        add(el);
    for (auto& el: valid)
        add(el);

    // test2 - return all topics that match prefix "+"
    auto matches = get_matching_topics(topics, topicFilter);
    assert(matches.size() == valid.size());
    for (uint i = 0; i < matches.size(); i++)
    {
        if (std::find(valid.begin(), valid.end(), matches[i]->name) == valid.end())
            assert(1 == 0);
    }
}

void test_trie10()
{
    trie<topic_t> topics;
    auto add = [&topics](const std::string& topic)
        {topics.insert(topic, std::make_shared<topic_t>(topic));};
    std::string topicFilter = "+/a/+/b/+/#";
    std::vector<std::string> valid = {"1/a/22/b/333/", "4/a/5 5/b/66 6/ccc",
                                       "/a/1/b/22/c/ddd/ee/ff/gg/"};
    std::vector<std::string> invalid = {"a/1/b/2/c", "/1/a/2/b/c/", "1/a/22/b/333"};
    for (auto& el: invalid)
        add(el);
    for (auto& el: valid)
        add(el);

    // test2 - return all topics that match prefix "+"
    auto matches = get_matching_topics(topics, topicFilter);
    assert(matches.size() == valid.size());
    for (uint i = 0; i < matches.size(); i++)
    {
        if (std::find(valid.begin(), valid.end(), matches[i]->name) == valid.end())
            assert(1 == 0);
    }
}

void test_trie11()
{
    trie<topic_t> topics;
    auto add = [&topics](const std::string& topic)
        {topics.insert(topic, std::make_shared<topic_t>(topic));};
    std::string topicFilter = "/a/+";
    std::vector<std::string> valid = {"/a/b", "/a/ccc",
                                       "/a/"};
    std::vector<std::string> invalid = {"a/b", "/a/b/", "/a/b/c"};
    for (auto& el: invalid)
        add(el);
    for (auto& el: valid)
        add(el);

    // test2 - return all topics that match prefix "+"
    auto matches = get_matching_topics(topics, topicFilter);
    assert(matches.size() == valid.size());
    for (uint i = 0; i < matches.size(); i++)
    {
        if (std::find(valid.begin(), valid.end(), matches[i]->name) == valid.end())
            assert(1 == 0);
    }
}

void test_trie12()
{
    trie<topic_t> topics;
    auto add = [&topics](const std::string& topic)
        {topics.insert(topic, std::make_shared<topic_t>(topic));};
    std::string topicFilter = "/a/+/+";
    std::vector<std::string> valid = {"/a/b/c", "/a/ccc/dddd",
                                       "/a/e/ff", "/a/r/"};
    std::vector<std::string> invalid = {"a/b/c", "/a/b", "/a/b/c/"};
    for (auto& el: invalid)
        add(el);
    for (auto& el: valid)
        add(el);

    // test2 - return all topics that match prefix "+"
    auto matches = get_matching_topics(topics, topicFilter);
    assert(matches.size() == valid.size());
    for (uint i = 0; i < matches.size(); i++)
    {
        if (std::find(valid.begin(), valid.end(), matches[i]->name) == valid.end())
            assert(1 == 0);
    }
}

void test_trie()
{
    test_trie1();
    test_trie2();
    test_trie3();
    test_trie4();
    test_trie5();
    test_trie6();
    test_trie7();
    test_trie8();
    test_trie9();
    test_trie10();
    test_trie11();
    test_trie12();
}

void tests()
{
    test_trie();
//    test_simple_pack_unpack();
//    test_mqtt_encode_decode_length();
//    test_pack_unpack();
}

#define CONNECT_BYTE 0x10
#define SUBREQ_BYTE  0x80
#define UNSUB_BYTE   0xA0
#define PINGREQ_BYTE 0xC0

template <typename T>
class MQTTClient: public tps::net::client_interface<T>
{
public:
    void pack_connect(mqtt_connect& con, tps::net::message<mqtt_header>& msg)
    {
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

        uint16_t clientIDLen = uint16_t(con.payload.client_id.size());
        clientIDLen = byteswap16(clientIDLen);
        msg << clientIDLen;
        msg << con.payload.client_id;
        len += sizeof(clientIDLen) + uint16_t(con.payload.client_id.size());

        if (vhdr.bits.will)
        {
            uint16_t willTopicLen = uint16_t(con.payload.will_topic.size());
            willTopicLen = byteswap16(willTopicLen);
            msg << willTopicLen;
            msg << con.payload.will_topic;
            len += sizeof(willTopicLen) + uint16_t(con.payload.will_topic.size());

            uint16_t willMsgLen = uint16_t(con.payload.will_message.size());
            willMsgLen = byteswap16(willMsgLen);
            msg << willMsgLen;
            msg << con.payload.will_message;
            len += sizeof(willMsgLen) + uint16_t(con.payload.will_message.size());
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
    }

    void pack_subscribe(mqtt_subscribe& sub, tps::net::message<mqtt_header>& msg)
    {
        uint size = 0;

        msg.hdr.byte = sub.header;

        auto pkt_id = byteswap16(sub.pkt_id);
        msg << pkt_id;
        size += sizeof(sub.pkt_id);

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

    void pack_unsubscribe(mqtt_unsubscribe& unsub, tps::net::message<mqtt_header>& msg)
    {
        uint size = 0;

        msg.hdr.byte = unsub.header;

        auto pkt_id = byteswap16(unsub.pkt_id);
        msg << pkt_id;
        size += sizeof(unsub.pkt_id);

        for (auto& [topiclen, topic]: unsub.tuples)
        {
            auto topiclenbe = byteswap16(topiclen);
            msg << topiclenbe;
            msg << topic;
            size += sizeof(topiclen) + topiclen;
        }

        msg.writeHdrSize += mqtt_encode_length(msg, size);
    }

    void unpack_connack(tps::net::message<mqtt_header>& msg, mqtt_connack& con)
    {
        con.header.byte = msg.hdr.byte.byte;
        msg >> con.sp.byte;
        msg >> con.rc;
    }

    void unpack_suback(tps::net::message<mqtt_header>& msg, mqtt_suback& suback)
    {
        suback.header.byte = msg.hdr.byte.byte;
        msg >> suback.pkt_id;
        suback.pkt_id = byteswap16(suback.pkt_id);

        uint16_t rcsBytes = msg.hdr.size - sizeof(suback.pkt_id);
        suback.rcs.resize(rcsBytes);
        msg >> suback.rcs;
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

        tps::net::message<mqtt_header>pubmsg;
        pub.pack(pubmsg);
        this->send(std::move(pubmsg));
    }

    void subscribe()
    {
        mqtt_subscribe subreq(SUBREQ_BYTE);
        subreq.header.bits.qos = 1; // [MQTT-3.8.1-1]

        static uint16_t pktID = 0;
        subreq.pkt_id = pktID++;

        std::tuple<uint16_t, std::string, uint8_t> t;
        auto& [topiclen, topic, qos] = t;
        topic = "/example";
        topiclen = uint16_t(topic.size());
        qos = AT_LEAST_ONCE;
        subreq.tuples.emplace_back(t);

        tps::net::message<T> msg;
        pack_subscribe(subreq, msg);
        this->send(std::move(msg));
    }

    void unsubscribe()
    {
        mqtt_unsubscribe unsub(UNSUB_BYTE);

        static uint16_t pktID = 0;
        unsub.pkt_id = pktID++;

        std::pair<uint16_t, std::string> t;
        auto& [topiclen, topic] = t;
        topic = "/example";
        topiclen = uint16_t(topic.size());
        unsub.tuples.emplace_back(t);

        tps::net::message<T> msg;
        pack_unsubscribe(unsub, msg);
        this->send(std::move(msg));
    }

    void ack(mqtt_ack& ack)
    {
        tps::net::message<T> msg;

        ack.pack(msg);
        this->send(std::move(msg));
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
//    tests();

    std::cout << "[" << std::this_thread::get_id() << "]MAIN THREAD\n";

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
    con.vhdr.bits.will = 1;
    con.vhdr.bits.will_qos = AT_LEAST_ONCE;
    con.vhdr.bits.will_retain = 1;
    con.payload.client_id = "foo1";
    con.payload.keepalive = 0xffff;
    con.payload.will_topic = "/example";
    con.payload.will_message = "[X]WILL: " + con.payload.client_id + " is dead";

    tps::net::message<mqtt_header>conmsg;
    client.pack_connect(con, conmsg);
    client.send(std::move(conmsg));

    std::cout << "~~~~~~" << con.payload.client_id << "~~~~~~\n";
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
                    case 3: client.unsubscribe(); break;
                    case 4: client.pingreq(); break;
                    case 5: return 0;
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
                        std::cout << "\n\t{CONNACK}\n" << resp;
                        break;
                    }
                    case packet_type::SUBACK:
                    {
                        mqtt_suback resp;
                        client.unpack_suback(msg, resp);
                        std::cout << "\n\t{SUBACK}\n" << resp;
                        break;
                    }
                    case packet_type::UNSUBACK:
                    {
                        mqtt_unsuback resp;
                        resp.header = msg.hdr.byte;
                        resp.unpack(msg);
                        std::cout << "\n\t{UNSUBACK}\n" << resp;
                        break;
                    }
                    case packet_type::PUBLISH:
                    {
                        mqtt_publish pub;
                        pub.header = msg.hdr.byte;
                        pub.unpack(msg);

                        mqtt_ack ack;
                        if (pub.header.bits.qos > 0)
                        {
                            if (pub.header.bits.qos == AT_LEAST_ONCE)
                                ack.header = PUBACK_BYTE;
                            else if (pub.header.bits.qos == EXACTLY_ONCE)
                                ack.header = PUBREC_BYTE;
                            ack.pkt_id = pub.pkt_id;
                            client.ack(ack);
                        }
                        std::cout << "\n\t{PUBLISH}\n" << pub;
                        break;
                    }
                    case packet_type::PUBACK:
                    {
                        mqtt_puback resp;
                        resp.header = msg.hdr.byte;
                        resp.unpack(msg);

                        std::cout << "\n\t{PUBACK}\n" << resp;
                        break;
                    }
                    case packet_type::PINGRESP:
                    {
                        mqtt_packet resp;
                        resp.header = msg.hdr.byte;
                        std::cout << "\n\t{PINGRESP}\n" << resp;
                        break;
                    }
                    case packet_type::PUBREL:
                    {
                        mqtt_ack ack;
                        ack.header = msg.hdr.byte;
                        ack.unpack(msg);
                        ack.header = PUBCOMP_BYTE;
                        client.ack(ack);

                        std::cout << "\n\t{PUBREL}\n" << ack;
                        break;
                    }
                    case packet_type::PUBCOMP:
                    {
                        mqtt_ack ack;
                        ack.header = msg.hdr.byte;
                        ack.unpack(msg);

                        std::cout << "\n\t{PUBCOMP}\n" << ack;
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
    server broker(5000);
    broker.start();
    broker.update();
#endif
    std::cout << "END\n";
    return 0;
}
