#include "core.h"

#include <memory>
#include <boost/algorithm/string.hpp>
#include "NetCommon/net_connection.h"

void ::core::delete_client(std::shared_ptr<client_t>& client)
{
    if (client->session.cleanSession)
    {
        // inform all topics about client unsubscription
        for (auto& topic: client->session.subscriptions)
            if (topic.second.unsub(client, false) && !topic.second.subscribers.size())
                topics.erase(topic.first);

        clientsIDs.erase(client->clientID);
    }
    else
        client->active = false;

    clients.erase(client->netClient);
    client->netClient.reset();
}

void topic::sub(std::shared_ptr<client> &client, uint8_t qos)
{
    auto it = subscribers.find(client->clientID);
    // if client is already subscribed - update it's qos [MQTT-3.8.4-3]
    if (it != subscribers.end())
        it->second.second = qos;
    else
    {
        // add new client to the list of subs
        subscriber newSub(*client, qos);
        std::pair<std::string, subscriber> newSubEntry(client->clientID, std::move(newSub));
        subscribers.emplace(std::move(newSubEntry));

        // add this topic to client's subscriptions list
        std::pair<std::string, topic_t&> newTopic(name, *this);
        client->session.subscriptions.emplace(std::move(newTopic));
    }
}

bool topic::unsub(std::shared_ptr<client> &client, bool deleteRecordFromClient)
{
    // find the client
    auto it = subscribers.find(client->clientID);
    if (it != subscribers.end())
    {
        // delete this topic from client's subscriptions
        if (deleteRecordFromClient)
        {
            auto& unsubClient = it->second.first;
            auto it2 = unsubClient.session.subscriptions.find(name);
            if (it2 != unsubClient.session.subscriptions.end())
                unsubClient.session.subscriptions.erase(it2);
        }

        // delete client record
        subscribers.erase(it);
        return true;
    }
    return false;
}

