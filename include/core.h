#ifndef CORE_H
#define CORE_H

#include <list>
#include <unordered_map>
#include "trie.h"
#include "network.h"

struct subscriber;

struct topic
{
    topic(std::string& _name): name(_name){}
    std::string name;
    std::list<subscriber> subscribers;

    void add_subscriber(struct sol_client *client,
                        subscriber &sub,
                        bool cleansession);
};

struct session
{
    std::list<topic> subscriptions;
    std::list<topic> subscribers;
    // TODO add pending confirmed messages
};

/*
 * Wrapper structure around a connected client, each client can be a publisher
 * or a subscriber, it can be used to track sessions too.
 */
struct sol_client
{
    std::string client_id;
    int fd;
    struct session session;
};

struct subscriber
{
    subscriber(uint8_t _qos, sol_client* _client): qos(_qos), client(_client){}
    uint8_t qos;
    struct sol_client *client;
};

/*
 * Main structure, a global instance will be instantiated at start, tracking
 * topics, connected clients and registered closures.
 */
struct sol
{
    std::unordered_map<std::string, sol_client> clients;
    std::unordered_map<std::string, closure> closures;
    trie<topic> topics;
};

void topic_add_subscriber(struct topic *, struct sol_client *, unsigned, bool);
void topic_del_subscriber(struct topic *, struct sol_client *, bool);

#endif // CORE_H
