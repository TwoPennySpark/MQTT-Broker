#include <assert.h>
#include <math.h>
#include <boost/algorithm/string.hpp>
#include "server.h"

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
    std::vector<std::string> invalid = {"cccc"}; //
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


void test_idpool_gen1()
{ // test gen filling the blank spots
    KeyPool<uint16_t, packet_type> p;
    auto type = packet_type::PUBACK;

    for (int i = 0; i < 10; i++)
        p.generate_key(type);

    // {0, 1, 2, 3, 4, 5, 6, 7, 8, 9}

    p.unregister_key(3);
    p.unregister_key(6);
    p.unregister_key(9);

    // {0, 1, 2}, {4, 5}, {7, 8}

    p.generate_key(type);
    p.generate_key(type);
    p.generate_key(type);

    // {0, 1, 2, 3, 4, 5, 6, 7, 8, 9}

    auto it = p.chunks.begin();
    assert(p.chunks.size() == 1);
    assert(it->start == 0 && it->end == 9 && it->values.size() == 10);
    for (uint16_t i = 0; i < 10; i++)
        assert(p.find(i).value() == type);
}

void test_idpool_gen2()
{ // test gen for case when all ids are taken
    KeyPool<uint16_t, packet_type> p; auto type = packet_type::PUBACK;

    for (uint16_t i = 0; i < 65535; i++)
        p.generate_key(type);
    p.generate_key(type);

    // {0, ... , 65535}

    try {
        p.generate_key(type);
    } catch (...) {
        for (uint16_t i = 0; i < 65535; i++)
            assert(p.find(i).value() == type);
        return;
    }
    assert(1==0);
}

void test_idpool_reg1()
{ // test reg filling the blank spots with different type
    KeyPool<uint16_t, packet_type> p;
    auto type  = packet_type::PUBACK;
    auto type2 = packet_type::PUBREC;

    for (uint16_t i = 0; i < 3; i++)
        p.register_key(i, type);
    p.register_key(3, type2);

    for (uint16_t i = 4; i < 6; i++)
        p.register_key(i, type);
    p.register_key(6, type2);

    for (uint16_t i = 7; i < 9; i++)
        p.register_key(i, type);
    p.register_key(9, type2);

    // {0, 1, 2, 3(2), 4, 5, 6(2), 7, 8, 9(2)}

    assert(p.unregister_key(3));
    assert(p.unregister_key(6));
    assert(p.unregister_key(9));

    // {0, 1, 2}, {4, 5}, {7, 8}

    p.register_key(3, type);
    p.register_key(6, type);
    p.register_key(9, type);

    // {0, 1, 2, 3, 4, 5, 6, 7, 8, 9}

    auto it = p.chunks.begin();
    assert(p.chunks.size() == 1);
    assert(it->start == 0 && it->end == 9 && it->values.size() == 10);
    for (uint16_t i = 0; i < 10; i++)
        assert(p.find(i).value() == type);
}

void test_idpool_reg2()
{ // test reg on existing elements
    KeyPool<uint16_t, packet_type> p;
    auto type  = packet_type::PUBACK;
    auto type2 = packet_type::PUBREC;

    for (uint16_t i = 0; i < 10; i++)
        assert(p.register_key(i, type));

    for (uint16_t i = 0; i < 10; i++)
        assert(!p.register_key(i, type2));

    auto it = p.chunks.begin();
    assert(p.chunks.size() == 1);
    assert(it->start == 0 && it->end == 9 && it->values.size() == 10);
    for (uint16_t i = 0; i < 10; i++)
        assert(p.find(i).value() == type);
}

void test_idpool_reg3()
{ // test reg on bigger elements
    KeyPool<uint16_t, packet_type> p;
    auto type = packet_type::PUBACK;

    p.register_key(100, type);
    p.register_key(1000, type);

    auto it = p.chunks.begin();
    assert(p.chunks.size() == 2);
    assert(it->start == 100 && it->end == 100 && it->values.size() == 1);
    it++;
    assert(it->start == 1000 && it->end == 1000 && it->values.size() == 1);
    assert(p.find(100).value() == type);
    assert(p.find(1000).value() == type);
}

void test_idpool_unreg1()
{ // test unreg on empty
    KeyPool<uint16_t, packet_type> p;
    auto type = packet_type::PUBACK;

    assert(p.unregister_key(0) == false);
    assert(p.unregister_key(-1000) == false);
    assert(p.unregister_key(65535) == false);
    assert(p.chunks.size() == 0);
}

void test_idpool_unreg2()
{ // test unreg on boundary elements
    KeyPool<uint16_t, packet_type> p; auto type = packet_type::PUBACK;

    p.register_key(2, type);p.register_key(3, type);p.register_key(4, type);
    p.register_key(7, type);p.register_key(8, type);p.register_key(9, type);

    // {2,3,4}, {7,8,9}

    p.unregister_key(2);
    p.unregister_key(4);
    p.unregister_key(7);
    p.unregister_key(9);

    // {3}, {8}

    assert(p.chunks.size() == 2);
    auto it = p.chunks.begin();
    assert(it->start == 3 && it->end == 3 && it->values.size() == 1);
    it++;
    assert(it->start == 8 && it->end == 8 && it->values.size() == 1);
    assert(p.find(3).value() == type);
    assert(p.find(8).value() == type);
}

void test_idpool_unreg3()
{ // test unreg on mid elements
    KeyPool<uint16_t, packet_type> p; auto type = packet_type::PUBACK;

    p.register_key(2, type); p.register_key(3, type); p.register_key(4, type); p.register_key(5, type);
    p.register_key(7, type); p.register_key(8, type); p.register_key(9, type); p.register_key(10, type);

    // {2,3,4,5}, {7,8,9,10}

    p.unregister_key(3);p.unregister_key(4);
    p.unregister_key(8);p.unregister_key(9);

    // {2}, {5}, {7}, {10}

    assert(p.chunks.size() == 4);
    auto it = p.chunks.begin();
    assert(it->start == 2 && it->end == 2 && it->values.size() == 1);
    it++;
    assert(it->start == 5 && it->end == 5 && it->values.size() == 1);
    it++;
    assert(it->start == 7 && it->end == 7 && it->values.size() == 1);
    it++;
    assert(it->start == 10 && it->end == 10 && it->values.size() == 1);
    assert(p.find(2).value() == type);
    assert(p.find(5).value() == type);
    assert(p.find(7).value() == type);
    assert(p.find(10).value() == type);
}

void test_idpool_unreg4()
{ // test unreg on mid and border elements
    KeyPool<uint16_t, packet_type> p; auto type = packet_type::PUBACK;
    auto type2 = packet_type::PUBREC;

    p.register_key(2, type);p.register_key(3, type2);p.register_key(4, type2);p.register_key(5, type);
    p.register_key(7, type);p.register_key(8, type);p.register_key(9, type);p.register_key(10, type2);

    // {2,3,4,5}, {7,8,9,10}

    assert(p.unregister_key(4));
    assert(p.unregister_key(3));
    assert(p.unregister_key(2));

    // {5}, {7,8,9,10}

    assert(p.unregister_key(9));
    assert(p.unregister_key(10));
    assert(p.unregister_key(8));

    // {5}, {7}

    assert(p.chunks.size() == 2);
    auto it = p.chunks.begin();
    assert(it->start == 5 && it->end == 5 && it->values.size() == 1);
    it++;
    assert(it->start == 7 && it->end == 7 && it->values.size() == 1);
    assert(p.find(5).value() == type);
    assert(p.find(7).value() == type);
}

void test_idpool()
{
    test_idpool_gen1();
    test_idpool_gen2();
    test_idpool_reg1();
    test_idpool_reg2();
    test_idpool_reg3();
    test_idpool_unreg1();
    test_idpool_unreg2();
    test_idpool_unreg3();
    test_idpool_unreg4();
}

void test()
{
//    test_simple_pack_unpack();
//    test_mqtt_encode_decode_length();
//    test_pack_unpack();
//    test_trie();

    test_idpool();
}

int main()
{
//    test();
    std::cout << "[" << std::this_thread::get_id() << "]MAIN THREAD\n";

    server broker(5000);
    broker.start();
    broker.update();

    std::cout << "END\n";
    return 0;
}
