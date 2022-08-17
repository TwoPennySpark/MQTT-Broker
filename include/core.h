#ifndef CORE_H
#define CORE_H

#include <set>
#include <unordered_map>
#include <memory>
#include <iostream>
#include <variant>
#include <boost/algorithm/string.hpp>
#include "trie.h"
#include "mqtt.h"

typedef struct topic topic_t;
typedef struct core core_t;
typedef struct client client_t;

namespace tps::net {template <typename T> class connection;}

using pClient = std::shared_ptr<client_t>;
using pConnection = std::shared_ptr<tps::net::connection<mqtt_header>>;

struct session
{
    bool cleanSession;

    // key - topic name, value - ref to topic struct
    std::unordered_map<std::string, topic_t&> subscriptions;

    std::set<uint16_t> unregPuback;
    // first - expected pubrec pkt ID, second - dup (number of already sent pubrels using this pkt ID)
    std::unordered_map<uint16_t, uint8_t> unregPubrec;
    // first - expected pubrel pkt ID, second - dup (number of already sent pubcomps using this pkt ID)
    std::unordered_map<uint16_t, uint8_t> unregPubrel;
    std::set<uint16_t> unregPubcomp;

    // first - msg, second - pkt ID
    std::vector<std::pair<tps::net::message<mqtt_header>, uint16_t>> savedMsgs;
};

typedef struct client
{
    client(pConnection _netClient): netClient(_netClient){}
    ~client() {std::cout << "[!]CLIENT DELETED:" << clientID << "\n";}

    // if client connected with clean session == 0, then, after disconnection,
    // information about client(session) is not getting deleted, instead we set active = false
    bool active;
    std::string clientID;
    struct session session;

    std::optional<mqtt_publish> will;

    std::optional<std::string> username;
    std::optional<std::string> password;

    uint16_t keepalive;

    pConnection netClient;
}client_t;

typedef struct topic
{
    topic(const std::string& _name): name(_name) {}
    ~topic() {std::cout << "[!]TOPIC DELETED:" << name << "\n";}

    void sub  (std::shared_ptr<client>& client, uint8_t qos);
    bool unsub(std::shared_ptr<client>& client, bool deleteRecordFromClient);

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
    trie<topic_t> topics;

    std::optional<std::reference_wrapper<pClient>> find_client(
        const std::variant<pConnection, std::reference_wrapper<std::string>>& key);

    pClient& add_new_client  (std::string& clientID,   pConnection& netClient);
    pClient& restore_client  (pClient& existingClient, pConnection& netClient);

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

    std::vector<std::shared_ptr<topic_t>> get_matching_topics(const std::string& topicFilter);

private:
    std::unordered_map<pConnection, pClient> clients;
    std::unordered_map<std::string, pClient> clientsIDs;

}core_t;

#endif // CORE_H
