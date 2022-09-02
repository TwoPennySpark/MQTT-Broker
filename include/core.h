#ifndef CORE_H
#define CORE_H

#include <set>
#include <map>
#include <unordered_map>
#include <memory>
#include <iostream>
#include <variant>
#include <boost/algorithm/string.hpp>
#include "trie.h"
#include "mqtt.h"
#include "keypool.h"

typedef struct core core_t;
typedef struct topic topic_t;
typedef struct client client_t;

namespace tps::net {template <typename T> class connection;}

using pClient = std::shared_ptr<client_t>;
using pConnection = std::shared_ptr<tps::net::connection<mqtt_header>>;

struct session
{
    // if cleanSession == 0 store all data from this struct until the client with same clientID arrives
    bool cleanSession;

    // key - topic name, value - ref to topic struct
    std::unordered_map<std::string, topic_t&> subscriptions;

    // first - expected ack pkt ID, second - expected ack type
    KeyPool<uint16_t, packet_type> pool;

    // messages that were published while client was inactive
    std::vector<mqtt_publish> savedMsgs;
};

typedef struct client
{
    client(const std::string& _clientID, pConnection& _netClient): clientID(_clientID), netClient(_netClient){}
    ~client() {std::cout << "[!]CLIENT DELETED:" << clientID << "\n";}

    std::string clientID;

    // if client connected with clean session == 0, then, after disconnection,
    // information about client(session) is not getting deleted, instead we set active = false
    // inactive state means that client will save all msgs, with qos == 1 & 2, which will be
    // published on the topics to which client subscribed while it was active
    bool active;
    struct session session;

    std::optional<mqtt_publish> will;

    std::optional<std::string> username;
    std::optional<std::string> password;

    uint16_t keepalive;

    std::reference_wrapper<const pConnection> netClient;
}client_t;

typedef struct topic
{
    topic(const std::string& _name): name(_name) {}
    ~topic() {std::cout << "[!]TOPIC DELETED:" << name << "\n";}

    std::string name;

    std::optional<mqtt_publish> retain;

    // first - client ref, second - maximum qos level at which the server can send msgs to the client
    using subscriber = std::pair<client_t&, uint8_t>;
    // key - client ID
    std::unordered_map<std::string, subscriber> subscribers;
}topic_t;

// manages the lifetime of client_t and topic_t objects
// provides means for client and topic creation, search and deletion
typedef struct core
{
public:
    // ===========CLIENTS===========
    // client struct can be found either by clientID or client's corresponding connection object
    std::optional<std::reference_wrapper<pClient>> find_client(
        const std::variant<pConnection, std::reference_wrapper<std::string>>& key);
    pClient& add_new_client  (std::string &&clientID,  pConnection&& netClient);
    pClient& restore_client  (pClient& existingClient, pConnection&& netClient);

    // usually deletion type depends on client::session::cleanSession param
    // but in some special cases caller needs to specify explicitly
    // whether the client should be deleted fully or store session
    enum deletion_flags
    {
        BASED_ON_CS_PARAM = 0,
        FULL_DELETION     = 1,
        STORE_SESSION     = 2,
    };
    void delete_client(pClient& client, uint8_t manualControl = BASED_ON_CS_PARAM);

    // ===========TOPICS===========
    // find topic named topicname, if bCreateIfNotExist == true - create new topic if none was found
    std::optional<std::reference_wrapper<topic_t>> find_topic(const std::string& topicname,
                                                              bool bCreateIfNotExist = false);
    // subscribe client to topic
    void subscribe  (client_t& client, topic_t& topic, uint8_t qos);
    // unsubscribe client from topic, delete topic if it has no subscribers left
    void unsubscribe(client_t& client, topic_t& topic);

    // find all topics that correspond to topicFilter string, that contains wildcards
    std::vector<std::shared_ptr<topic_t>> get_matching_topics(const std::string& topicFilter);

private:
    std::unordered_map<pConnection, pClient> clients;
    std::unordered_map<std::string, pClient> clientsIDs;

    trie<topic_t> topics;
}core_t;

#endif // CORE_H
