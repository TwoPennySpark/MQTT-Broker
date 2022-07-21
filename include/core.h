#ifndef CORE_H
#define CORE_H

#include <list>
#include <set>
#include <unordered_map>
#include <memory>
#include "trie.h"
#include "mqtt.h"

typedef struct topic topic_t;

namespace tps
{
    namespace net
    {
        template <typename T>
        class connection;
    }
}

struct session
{
    void clear()
    {
        subscriptions.clear();
    }

    bool cleanSession;
    std::list<std::shared_ptr<topic_t>> subscriptions;
    std::set<uint16_t> unregPuback;
    std::set<uint16_t> unregPubrec;
    std::set<uint16_t> unregPubrel;
    std::set<uint16_t> unregPubcomp;
    // TODO add pending confirmed messages
};

/*
 * Wrapper structure around a connected client, each client can be a publisher
 * or a subscriber, it can be used to track sessions too
 */
//template <typename T>
typedef struct client
{
    ~client();
    bool active;
    std::string clientID;
    struct session session;

    bool will;
    bool willQOS;
    bool willRetain;
    std::string willTopic;
    std::string willMsg;

    std::string username;
    std::string password;

    uint16_t keepalive;
    std::shared_ptr<tps::net::connection<mqtt_header>> netClient;
}client_t;

typedef struct subscriber
{
    subscriber(uint8_t _qos, client_t& _client): qos(_qos), client(_client){}
    uint8_t qos;
    client_t& client;

    bool operator<(const subscriber& s) const
    {
        return s.client.clientID > client.clientID ? true : false;
    }
    struct compare
    {
        bool operator() (const std::shared_ptr<subscriber>& a,
                         const std::shared_ptr<subscriber>& b) const
        {
            return *a < *b;
        }
    };
}subscriber_t;

typedef struct topic
{
    topic(const std::string& _name): name(_name){}
    std::string name;
    std::string retainedMsg;
    uint8_t retainedQOS;
    std::set<std::shared_ptr<subscriber>, subscriber::compare> subscribers;

    ~topic()
    {
        printf("Topic Deleted:%s\n", name.data());
    }
}topic_t;

typedef struct core
{
    trie<topic_t> topics;
    std::unordered_map<std::shared_ptr<tps::net::connection<mqtt_header>>,
                                       std::shared_ptr<client_t>> clients;
    std::unordered_map<std::string, std::shared_ptr<client_t>> clientsIDs;

    void delete_client(std::shared_ptr<client_t> client);

    std::vector<std::string> get_matching_topics(const std::string& topicFilter);
}core_t;

#endif // CORE_H
