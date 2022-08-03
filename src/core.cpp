#include "core.h"

#include <memory>
#include <boost/algorithm/string.hpp>
#include "NetCommon/net_connection.h"

void ::core::delete_client(std::shared_ptr<client_t>& client)
{
    if (client->will)
    {
        mqtt_publish pub(PUBLISH_BYTE);
        pub.topiclen = client->willTopic.size();
        pub.topic = std::move(client->willTopic);
        pub.payloadlen = client->willMsg.size();
        pub.payload = std::move(client->willMsg);

        tps::net::message<mqtt_header> pubmsg;
        pub.pack(pubmsg);

        //
    }
    client->netClient->disconnect();

    if (client->session.cleanSession)
    {
        clients.erase(client->netClient);
        clientsIDs.erase(client->clientID);
    }
    else
        client->active = false;
}

bool topic::unsub(std::shared_ptr<client> &client)
{
    for (auto it = subscribers.begin(); it != subscribers.end(); it++)
    {
        if (&it->first == client.get())
        {
            if (it->first.session.cleanSession)
                subscribers.erase(it);
            else
                it->first.active = false;
            return true;
        }
    }
    return false;
}

