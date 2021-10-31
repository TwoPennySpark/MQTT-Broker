#ifndef CORE_H
#define CORE_H

#include <list>
#include <set>
#include <unordered_map>
#include "trie.h"
#include "network.h"

/*
 * 3 operations:
 * 1) publish: TOPICs needs to store it's SUBs list
 * 2) unsub: CLIENTs needs to store info about TOPICs they're subscribed to
 * 3) disconnect: server needs to remove all apearances of CLIENT connected to TOPICs
 */


struct topic;

struct session
{
    bool cleansession;
    std::list<std::shared_ptr<topic>> subscriptions;
    // TODO add pending confirmed messages
};

/*
 * Wrapper structure around a connected client, each client can be a publisher
 * or a subscriber, it can be used to track sessions too.
 */
struct sol_client
{
    ~sol_client();
    int fd;
    std::string client_id;
    struct session session;
};

struct subscriber
{
    subscriber(uint8_t _qos, sol_client* _client): qos(_qos), client(_client){}
    uint8_t qos;
    struct sol_client *client;

    bool operator<(const subscriber& s) const
    {
        return s.client->client_id > client->client_id ? true : false;
    }
//    bool operator>(subscriber& s)
//    {
//        return !(s < *this);
//    }
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

    void add_subscriber(struct sol_client *client,
                        subscriber *sub,
                        bool cleansession);
    void del_subscriber(struct sol_client *client,
                              bool cleansession);
};

/*
 * Main structure, a global instance will be instantiated at start, tracking
 * topics, connected clients and registered closures.
 */
struct sol
{
    trie<topic> topics;
    std::unordered_map<std::string, std::shared_ptr<closure>> closures;
    std::unordered_map<std::string, std::shared_ptr<sol_client>> clients;
};

#endif // CORE_H
