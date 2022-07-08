#ifndef CORE_H
#define CORE_H

#include <list>
#include <set>
#include <unordered_map>
#include <memory>
#include "trie.h"
#include "mqtt.h"

struct topic;

struct session
{
    bool cleansession;
    std::list<std::shared_ptr<topic>> subscriptions;
    // TODO add pending confirmed messages
};

/*
 * Wrapper structure around a connected client, each client can be a publisher
 * or a subscriber, it can be used to track sessions too
 */
typedef struct client
{
    ~client();
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
}client_t;

struct subscriber
{
    subscriber(uint8_t _qos, client& _client): qos(_qos), client(_client){}
    uint8_t qos;
    struct client& client;

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
};

struct topic
{
    topic(std::string& _name): name(_name){}
    std::string name;
    std::set<std::shared_ptr<subscriber>, subscriber::compare> subscribers;

    ~topic()
    {
        printf("Topic Deleted:%s\n", name.data());
    }

    void add_subscriber(struct client *client,
                        subscriber *sub,
                        bool cleansession);
    void del_subscriber(struct client *client,
                              bool cleansession);
};

struct core
{
    trie<topic> topics;
    std::unordered_map<std::string, std::shared_ptr<client>> clients;
};

#endif // CORE_H
