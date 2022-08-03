#ifndef CORE_H
#define CORE_H

#include <list>
#include <set>
#include <unordered_map>
#include <memory>
#include <iostream>
#include <boost/algorithm/string.hpp>
#include "trie.h"
#include "mqtt.h"

typedef struct topic topic_t;

namespace tps
{
    namespace net
    {
        template <typename T>
        class connection;
    }
}

struct session
{
    bool cleanSession;
    std::set<uint16_t> unregPuback;
    // first - expected pubrec pkt ID, second - dup (number of already sent pubrels using this pkt ID)
    std::unordered_map<uint16_t, uint8_t> unregPubrec;
    // first - expected pubrel pkt ID, second - dup (number of already sent pubcomps using this pkt ID)
    std::unordered_map<uint16_t, uint8_t> unregPubrel;
    std::set<uint16_t> unregPubcomp;
    // TODO add pending confirmed messages
};

/*
 * Wrapper structure around a connected client, each client can be a publisher
 * or a subscriber, it can be used to track sessions too
 */
//template <typename T>
typedef struct client
{
    client(std::shared_ptr<tps::net::connection<mqtt_header>> _netClient): netClient(_netClient){}
    ~client() {std::cout << "CLIENT DELETED:" << clientID << "\n";}

    bool active;
    std::string clientID;
    struct session session;

    bool will;
    bool willQOS;
    bool willRetain;
    std::string willTopic;
    std::string willMsg;

    std::string username;
    std::string password;

    uint16_t keepalive;
    std::shared_ptr<tps::net::connection<mqtt_header>> netClient;
}client_t;

typedef struct topic
{
    topic(const std::string& _name): name(_name){}
    std::string name;
    std::string retainedMsg;
    uint8_t retainedQOS;
    std::vector<std::pair<client_t&, uint8_t>> subscribers;

    ~topic() {std::cout << "Topic Deleted:" << name << "\n";}

    bool unsub(std::shared_ptr<client>& client);
}topic_t;

typedef struct core
{
    trie<topic_t> topics;
    std::unordered_map<std::shared_ptr<tps::net::connection<mqtt_header>>,
                                       std::shared_ptr<client_t>> clients;
    std::unordered_map<std::string, std::shared_ptr<client_t>> clientsIDs;

    void delete_client(std::shared_ptr<client_t> &client);

    std::vector<std::shared_ptr<topic_t>> get_matching_topics(const std::string &topicFilter)
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

}core_t;

#endif // CORE_H
