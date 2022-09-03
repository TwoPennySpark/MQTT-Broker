#include "core.h"
#include "NetCommon/net_message.h"

std::optional<std::reference_wrapper<pClient>> core_t::find_client(
        const std::variant<pConnection, std::reference_wrapper<std::string>>& key)
{
    if (auto netClient = std::get_if<pConnection>(&key))
    {
        auto it = clients.find(*netClient);
        if (it != clients.end())
            return it->second;
    }
    else if (auto clientID = std::get_if<std::reference_wrapper<std::string>>(&key))
    {
        auto it = clientsIDs.find(clientID->get());
        if (it != clientsIDs.end())
            return it->second;
    }

    return std::nullopt;
}

std::string generate_random_client_id()
{
    auto randchar = []() -> char
    {
        const char charset[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
        const size_t max_index = (sizeof(charset) - 1);
        return charset[rand() % max_index];
    };

    std::string str(MAX_CLIENT_ID_LEN, 0);
    std::generate_n(str.begin(), MAX_CLIENT_ID_LEN, randchar);

    return str;
}

pClient& core_t::add_new_client(std::string&& clientID, pConnection&& netClient)
{
    if (!clientID.size()) // [MQTT-3.1.3-6]
    {
        do {clientID = generate_random_client_id();}
        while (clientsIDs.find(clientID) != clientsIDs.end());
        std::cout << "GENERATED CLIENT ID:" << clientID << "\n\n";
    }

    auto newClient = std::make_shared<client_t>(clientID, netClient);

    clientsIDs.emplace(std::move(clientID), newClient);
    auto res = clients.emplace(std::move(netClient), std::move(newClient));
    // store reference to the key inside value
    res.first->second->netClient = res.first->first;

    return res.first->second;
}

pClient& core_t::restore_client(pClient& existingClient, pConnection&& netClient)
{
    auto res = clients.emplace(std::move(netClient), existingClient);
    // store reference to the key inside value
    existingClient->netClient = res.first->first;

    return res.first->second;
}

void core_t::delete_client(pClient& client, uint8_t manualControl)
{
    bool sessionPresent = !client->session.cleanSession;
    if (manualControl)
        sessionPresent = (manualControl == FULL_DELETION) ? false : true;

    if (sessionPresent)
        // switch to inactive state
        client->active = false;
    else
    {
        // delete all client's subscriptions
        for (auto it = client->session.subscriptions.begin(); it != client->session.subscriptions.end();)
        {
            topic_t& topic = it->second;
            ++it;
            unsubscribe(*client, topic);
        }

        clientsIDs.erase(client->clientID);
    }

    client->will.reset();
    client->username.reset();
    client->password.reset();

    clients.erase(client->netClient.get());
}

std::optional<std::reference_wrapper<topic_t>> core_t::find_topic(const std::string& topicname, bool bCreateIfNotExist)
{
    auto topicNode = topics.find(topicname);
    if (topicNode && topicNode->data)
        return *topicNode->data;
    else if (bCreateIfNotExist)
    {
        //  create new topic
        auto newTopic = std::make_shared<topic_t>(topicname);
        topics.insert(topicname, newTopic);
        return *newTopic;
    }
    return std::nullopt;
}

void core_t::subscribe(client_t& client, topic_t& topic, uint8_t qos)
{
    // if client is already subscribed - update it's qos [MQTT-3.8.4-3]
    if (auto it = topic.subscribers.find(client.clientID);
             it != topic.subscribers.end())
        it->second.second = qos;
    else
        // add new client to the list of subs
        topic.subscribers.emplace(client.clientID, topic_t::subscriber(client, qos));

    client.session.subscriptions.emplace(topic.name, topic);

}

void core_t::unsubscribe(client_t& client, topic_t& topic)
{
    client.session.subscriptions.erase(topic.name);
    topic.subscribers.erase(client.clientID);

    // delete topic if there is no more subscribers
    if (!topic.subscribers.size())
        topics.erase(topic.name);
}

std::vector<std::shared_ptr<topic_t>> core_t::get_matching_topics(const std::string& topicFilter)
{
    std::vector<std::shared_ptr<topic_t>> matches;
    std::vector<trie_node<topic_t>*> matchesSoFar;
    matchesSoFar.push_back(nullptr); // first search is from root
    std::string prefix = "";
    bool singleIsLast = false; // true when last symbol is '+'

    bool multilvl = false;
    // if there is a '#'(multi-lvl) wildcard, can mean one of two things:
    // 1) topicFilter == "#" or
    // 2) '#' is the last symbol of topicFilter
    if (topicFilter.find("#") != std::string::npos)
    {
        // if topicFilter == "#"
        if (topicFilter.length() == 1)
        {
            // every topic is a match
            topics.apply_func(prefix, nullptr, [&prefix, &matches](trie_node<topic_t>* t)
            {
                if (t->data->name == prefix)
                    return;

                matches.push_back(t->data);
            });
            return matches;
        }
        else // we need to evaluate the expr before '#' first
        {
            multilvl = true;
            prefix = topicFilter;
        }
    }

    // if there is a one or more '+'(single lvl) wildcards
    if (topicFilter.find("+") != std::string::npos)
    {
        std::vector<boost::iterator_range<std::string::const_iterator>> singlelvl;
        boost::find_all(singlelvl, topicFilter, "/+");
        if (topicFilter[0] == '+')
            singlelvl.emplace(singlelvl.cbegin(),
                              boost::iterator_range<std::string::const_iterator>(topicFilter.cbegin(), topicFilter.cbegin()+1));

        auto start = topicFilter.cbegin();
        auto end = singlelvl[0].begin()+1;
        if (topicFilter[0] == '+')
            start++;
        if (topicFilter[topicFilter.size()-1] == '+')
            singleIsLast = true;

        uint i = 0;
        std::vector<trie_node<topic_t>*> temp;
        do
        {
            // prefix = everything that comes before "/+", including '/'
            prefix = std::string(start, end);

            if (singleIsLast && i == singlelvl.size()-1)
                break;

            for (uint j = 0; j < matchesSoFar.size(); j++)
                // +/a   - from matchesSoFar[j](if nullptr - from root) go to "" (prefix) then find all topicnames until '/'
                // /+/a/ - from matchesSoFar[j](if nullptr - from root) go to / (prefix) then find all topicnames until '/'
                // /a/+/ - from matchesSoFar[j](if nullptr - from root) go to /a/ (prefix) then find all topicnames until '/'
                topics.apply_func_key(prefix, matchesSoFar[j], '/',
                                      [&temp](trie_node<topic_t>* n) { temp.push_back(n); });

            matchesSoFar = std::move(temp);

            start = singlelvl[i].end()+1;
            end = singlelvl[i+1].begin()+1;
            i++;
        }while (i < singlelvl.size());

        start = singlelvl[singlelvl.size()-1].end()+1;
        end = topicFilter.cend();
        if (!singleIsLast)
            prefix = std::string(start, end);
    }

    if (multilvl)
    {
        prefix.pop_back();
        for (uint i = 0; i < matchesSoFar.size(); i++)
            topics.apply_func(prefix, matchesSoFar[i],
                              [&matches](trie_node<topic_t>* n) { matches.push_back(n->data); });
    }
    else
    {
        if (singleIsLast)
        {
            for (uint i = 0; i < matchesSoFar.size(); i++)
                topics.find_all_data_until(prefix, matchesSoFar[i], '/',
                                           [&matches](trie_node<topic_t>* n) { matches.push_back(n->data); });
        }
        else
        {
            for (uint i = 0; i < matchesSoFar.size(); i++)
            {
                auto n = topics.find(prefix, matchesSoFar[i]);
                if (n && n->data)
                    matches.push_back(n->data);
            }
        }
    }

    return matches;
}

