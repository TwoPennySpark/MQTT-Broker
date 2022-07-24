#include <string.h>
#include <stdlib.h>
#include <memory>
#include <boost/algorithm/string.hpp>
#include "core.h"
#include "NetCommon/net_connection.h"

client::~client()
{
    // reach every topic and delete this client subscription
    for (auto el: session.subscriptions)
    {
//        el->del_subscriber(this, session.cleanSession);
        el.reset();
    }
    printf("CLIENT DELETED\n");
}

void ::core::delete_client(std::shared_ptr<client_t> client)
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

std::vector<std::string> core_t::get_matching_topics(const std::string &topicFilter)
{
    std::vector<std::string> matches;
    std::string prefix = "";

    bool multilvl = false;
    if (topicFilter[topicFilter.length()-1] == '#')
    {
        multilvl = true;
        if (topicFilter.length() == 1)
        {
             topics.apply_func(prefix, nullptr, [&prefix, &matches](trie_node<topic_t>* t)
             {
                 if (t->data->name == prefix)
                     return;

                 matches.push_back(t->data->name);
             });
             return matches;
        }
    }

    std::vector<boost::iterator_range<std::string::const_iterator>> singlelvl;
    if (topicFilter[0] == '+')
        singlelvl.emplace_back(topicFilter.cbegin(), topicFilter.cbegin());
    boost::find_all(singlelvl, topicFilter, "/+");

    for (auto match : singlelvl)
    {
        int64_t index = match.begin() - topicFilter.begin();
        std::cout << index << std::endl;
    }

    return matches;
}
