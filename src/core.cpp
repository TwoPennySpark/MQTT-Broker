#include <string.h>
#include <stdlib.h>
#include <memory>
#include <boost/algorithm/string.hpp>
#include "core.h"
#include "NetCommon/net_connection.h"

client::~client()
{
    // reach every topic and delete this client subscription
    for (auto& el: session.subscriptions)
    {
//        el->del_subscriber(this, session.cleanSession);
        el.reset();
    }
    printf("CLIENT DELETED\n");
}

void ::core::delete_client(std::shared_ptr<client_t>& client)
{
    if (client->will)
    {

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
