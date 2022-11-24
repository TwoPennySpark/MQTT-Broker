#include <assert.h>
#include "client.h"

tps::net::message<mqtt_header> get_msg(std::unique_ptr<MQTTClient>& client)
{
    client->incoming().wait();
    return client->incoming().pop_front().msg;
}

auto create_client()
{
    auto client = std::make_unique<MQTTClient>();
    auto fut = client->connect("127.0.0.1", 1883);
    try {fut.get();} catch (const std::exception& e) {
        std::cout << e.what() << "\n";
        exit(0);
    }

    return client;
}

void test_connect()
{
    { // connect with ID and clean session
    std::cout << "#1\n";
    auto client = create_client();
    client->connect_server("client0");
    auto msg = get_msg(client);
    auto connack = client->unpack_connack(msg);

    assert(connack.header.byte == uint8_t(packet_type::CONNACK) << 4);
    assert(connack.sp.byte == 0);
    assert(connack.rc == 0);
    }

    { // connect with NO ID and clean session
        std::cout << "#2\n";
    auto client = create_client();
    client->connect_server("");
    auto msg = get_msg(client);
    auto connack = client->unpack_connack(msg);

    assert(connack.header.byte == uint8_t(packet_type::CONNACK) << 4);
    assert(connack.sp.byte == 0);
    assert(connack.rc == 0);
    }

    { // connect with NO ID and NO clean session - violation[MQTT-3.1.3-8]
        std::cout << "#3\n";
    auto client = create_client();
    client->connect_server("", CLEAN_SESSION_FALSE);
    auto msg = get_msg(client);
    auto connack = client->unpack_connack(msg);

    assert(connack.header.byte == uint8_t(packet_type::CONNACK) << 4);
    assert(connack.sp.byte == 0);
    assert(connack.rc == 2);

    client->send_disconnect();
    }

    { // connect with ID and NO clean session without previous session
        std::cout << "#4\n";
    auto client = create_client();
    client->connect_server("client0", CLEAN_SESSION_FALSE);
    auto msg = get_msg(client);
    auto connack = client->unpack_connack(msg);

    assert(connack.header.byte == uint8_t(packet_type::CONNACK) << 4);
    assert(connack.sp.byte == 0);
    assert(connack.rc == 0);

    client->send_disconnect();
    }

    { // connect with ID and NO clean session with previous session
        std::cout << "#5\n";

    // connect first time to drop client's session if it exists
    auto client0 = create_client();
    client0->connect_server("client1", CLEAN_SESSION_TRUE);

    auto msg0 = get_msg(client0);
    auto connack0 = client0->unpack_connack(msg0);

    assert(connack0.header.byte == uint8_t(packet_type::CONNACK) << 4);
    assert(connack0.sp.byte == 0);
    assert(connack0.rc == 0);

    // disconnect
    client0->send_disconnect();

    // connect again to create new session
    auto client1 = create_client();
    client1->connect_server("client1", CLEAN_SESSION_FALSE);
    auto msg1 = get_msg(client1);
    auto connack1 = client1->unpack_connack(msg1);

    assert(connack1.header.byte == uint8_t(packet_type::CONNACK) << 4);
    assert(connack1.sp.byte == 0);
    assert(connack1.rc == 0);

    // disconnect - session should be stored
    client1->send_disconnect();

    // connect again with same id to restore session
    auto client2 = create_client();
    client2->connect_server("client1", CLEAN_SESSION_FALSE);
    auto msg2 = get_msg(client2);
    auto connack2 = client2->unpack_connack(msg2);

    assert(connack2.header.byte == uint8_t(packet_type::CONNACK) << 4);
    assert(connack2.sp.byte == 1);
    assert(connack2.rc == 0);

    client2->send_disconnect();
    }
}

void test_publish()
{
    { // 1st client subs to topic with qos 0, 2nd client publishes msg on topic with qos 0
        std::cout << "#1\n";

    // 1st client
    auto client0 = create_client();
    client0->connect_server("client1");
    get_msg(client0); // get CONNACK

    // send SUB
    std::vector<std::pair<std::string, uint8_t>> topics;
    std::string topic = "/foo";
    topics.emplace_back(topic, AT_MOST_ONCE);
    auto pktID = client0->subscribe(topics);

    // recv SUBACK
    auto msg0 = get_msg(client0);
    auto suback = client0->unpack_suback(msg0);
    assert(suback.rcs[0] == AT_MOST_ONCE);
    assert(suback.pktID == pktID);

    // 2nd client
    auto client1 = create_client();
    client1->connect_server("client2");
    get_msg(client1); // get CONNACK

    // send PUB
    std::string msg = "msg_example";
    client1->publish(topic, msg, AT_MOST_ONCE);

    // 1st recieves PUB from 2nd
    auto msg01 = get_msg(client0);
    auto pub = client0->unpack_publish(msg01);
    assert(pub.header.bits.qos == AT_MOST_ONCE);
    assert(pub.topic == topic);
    assert(pub.payload == msg);
    }

    { // 1st client subs to topic with qos 1, 2nd client publishes msg on topic with qos 0
        std::cout << "#2\n";

    // 1st client
    auto client0 = create_client();
    client0->connect_server("client1");
    get_msg(client0); // get CONNACK

    // send SUB
    std::vector<std::pair<std::string, uint8_t>> topics;
    std::string topic = "/foo";
    topics.emplace_back(topic, AT_LEAST_ONCE);
    auto pktID = client0->subscribe(topics);

    // recv SUBACK
    auto msg0 = get_msg(client0);
    auto suback = client0->unpack_suback(msg0);
    assert(suback.rcs[0] == AT_LEAST_ONCE);
    assert(suback.pktID == pktID);

    // 2nd client
    auto client1 = create_client();
    client1->connect_server("client2");
    get_msg(client1); // get CONNACK

    // send PUB
    std::string msg = "msg_example";
    client1->publish(topic, msg, AT_MOST_ONCE);

    // 1st recieves PUB from 2nd
    auto msg01 = get_msg(client0);
    auto pub = client0->unpack_publish(msg01);
    assert(pub.header.bits.qos == AT_MOST_ONCE);
    assert(pub.topic == topic);
    assert(pub.payload == msg);
    }

    { // 1st client subs to topic with qos 1, 2nd client publishes msg on topic with qos 1
        std::cout << "#3\n";

    // 1st client connect
    auto client0 = create_client();
    client0->connect_server("client1");
    get_msg(client0); // get CONNACK

    // send SUB
    std::vector<std::pair<std::string, uint8_t>> topics;
    std::string topic = "/foo";
    topics.emplace_back(topic, AT_LEAST_ONCE);
    auto pktID = client0->subscribe(topics);

    // recv SUBACK
    auto msg0 = get_msg(client0);
    auto suback = client0->unpack_suback(msg0);
    assert(suback.rcs[0] == AT_LEAST_ONCE);
    assert(suback.pktID == pktID);

    // 2nd client connect
    auto client1 = create_client();
    client1->connect_server("client2");
    get_msg(client1); // get CONNACK

    // send PUB
    std::string msg = "msg_example";
    auto pktID1 = client1->publish(topic, msg, AT_LEAST_ONCE);

    // recv PUBACK
    auto msg1 = get_msg(client1); // recv puback
    auto ack = client1->unpack_ack(msg1);
    assert(ack.header.bits.type == uint8_t(packet_type::PUBACK));
    assert(ack.pktID == pktID1);

    // 1st recieves PUB from 2nd
    auto msg01 = get_msg(client0);
    auto pub = client0->unpack_publish(msg01);
    assert(pub.header.bits.qos == AT_LEAST_ONCE);
    assert(pub.topic == topic);
    assert(pub.payload == msg);
    }

    { // 1st client subs to topic with qos 2, 2nd client publishes msg on topic with qos 0
        std::cout << "#4\n";

    // 1st client
    auto client0 = create_client();
    client0->connect_server("client1");
    get_msg(client0); // get CONNACK

    // send SUB
    std::vector<std::pair<std::string, uint8_t>> topics;
    std::string topic = "/foo";
    topics.emplace_back(topic, EXACTLY_ONCE);
    auto pktID = client0->subscribe(topics);

    // recv SUBACK
    auto msg0 = get_msg(client0);
    auto suback = client0->unpack_suback(msg0);
    assert(suback.rcs[0] == EXACTLY_ONCE);
    assert(suback.pktID == pktID);

    // 2nd client
    auto client1 = create_client();
    client1->connect_server("client2");
    get_msg(client1); // get CONNACK

    // send PUB
    std::string msg = "msg_example";
    client1->publish(topic, msg, AT_MOST_ONCE);

    // 1st recieves PUB from 2nd
    auto msg01 = get_msg(client0);
    auto pub = client0->unpack_publish(msg01);
    assert(pub.header.bits.qos == AT_MOST_ONCE);
    assert(pub.topic == topic);
    assert(pub.payload == msg);
    }

    { // 1st client subs to topic with qos 2, 2nd client publishes msg on topic with qos 1
        std::cout << "#5\n";

    // 1st client
    auto client0 = create_client();
    client0->connect_server("client1");
    get_msg(client0); // get CONNACK

    // send SUB
    std::vector<std::pair<std::string, uint8_t>> topics;
    std::string topic = "/foo";
    topics.emplace_back(topic, EXACTLY_ONCE);
    auto pktID = client0->subscribe(topics);

    // recv SUBACK
    auto msg0 = get_msg(client0);
    auto suback = client0->unpack_suback(msg0);
    assert(suback.rcs[0] == EXACTLY_ONCE);
    assert(suback.pktID == pktID);

    // 2nd client
    auto client1 = create_client();
    client1->connect_server("client2");
    get_msg(client1); // get CONNACK

    // send PUB
    std::string msg = "msg_example";
    auto pktID1 = client1->publish(topic, msg, AT_LEAST_ONCE);

    // recv PUBACK
    auto msg1 = get_msg(client1);
    auto ack = client1->unpack_ack(msg1);
    assert(ack.header.bits.type == uint8_t(packet_type::PUBACK));
    assert(ack.pktID == pktID1);

    // 1st recieves PUB from 2nd
    auto msg01 = get_msg(client0);
    auto pub = client0->unpack_publish(msg01);
    assert(pub.header.bits.qos == AT_LEAST_ONCE);
    assert(pub.topic == topic);
    assert(pub.payload == msg);
    }

    { // 1st client subs to topic with qos 2, 2nd client publishes msg on topic with qos 2
        std::cout << "#6\n";

    // 1st client
    auto client0 = create_client();
    client0->connect_server("client1");
    get_msg(client0); // get CONNACK

    // send SUB
    std::vector<std::pair<std::string, uint8_t>> topics;
    std::string topic = "/foo";
    topics.emplace_back(topic, EXACTLY_ONCE);
    auto pktID = client0->subscribe(topics);

    // recv SUBACK
    auto msg0 = get_msg(client0);
    auto suback = client0->unpack_suback(msg0);
    assert(suback.rcs[0] == EXACTLY_ONCE);
    assert(suback.pktID == pktID);

    // 2nd client
    auto client1 = create_client();
    client1->connect_server("client2");
    get_msg(client1); // get CONNACK

    // send PUB
    std::string msg1 = "msg_example";
    auto pktID1 = client1->publish(topic, msg1, EXACTLY_ONCE);

    // recv PUBREC
    auto msg12 = get_msg(client1);
    auto ack1 = client1->unpack_ack(msg12);
    assert(ack1.header.bits.type == uint8_t(packet_type::PUBREC));
    assert(ack1.pktID == pktID1);

    client1->send_ack(packet_type::PUBREL, ack1.pktID); // send PUBREL

    // recv PUBCOMP
    auto msg13 = get_msg(client1);
    auto ack12 = client1->unpack_ack(msg13);
    assert(ack12.header.bits.type == uint8_t(packet_type::PUBCOMP));
    assert(ack12.pktID == pktID1);

    // 1st recieves PUB from 2nd
    auto msg01 = get_msg(client0);
    auto pub = client0->unpack_publish(msg01);
    assert(pub.header.bits.qos == EXACTLY_ONCE);
    assert(pub.topic == topic);
    assert(pub.payload == msg1);

    client0->send_ack(packet_type::PUBREC, ack1.pktID); // send PUBREC

    // recv PUBREL
    auto msg02 = get_msg(client0);
    auto ack01 = client1->unpack_ack(msg02);
    assert(ack01.header.bits.type == uint8_t(packet_type::PUBREL));
    assert(ack01.pktID == pub.pktID);
    }
}

int main()
{
    test_connect();
    test_publish();

    std::cout << "END\n";
    return 0;
}
