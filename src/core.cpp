#include "core.h"
#include "NetCommon/net_message.h"

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
    // if client is already subscribed - update it's qos [MQTT-3.8.4-3]
    if (auto it = subscribers.find(client->clientID); it != subscribers.end())
        it->second.second = qos;
    else
    {
        // add new client to the list of subs
        subscribers.emplace(client->clientID, subscriber(*client, qos));

        // add this topic to client's subscriptions list
        client->session.subscriptions.emplace(name, *this);
    }
}

bool topic::unsub(std::shared_ptr<client> &client, bool deleteRecordFromClient)
{
    // find the client
    if (auto it = subscribers.find(client->clientID); it != subscribers.end())
    {
        // delete this topic from client's subscriptions
        if (deleteRecordFromClient)
        {
            auto& unsubClientSubscriptions = it->second.first.session.subscriptions;
            auto it2 = unsubClientSubscriptions.find(name);
            if (it2 != unsubClientSubscriptions.end())
                unsubClientSubscriptions.erase(it2);
        }

        // delete client record
        subscribers.erase(it);
        return true;
    }
    return false;
}

