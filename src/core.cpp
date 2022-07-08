#include <string.h>
#include <stdlib.h>
#include <memory>
#include "core.h"
#include "trie.h"

void topic::add_subscriber(struct client* client,
                          struct subscriber* sub,
                          bool cleansession)
{
//    std::shared_ptr<struct subscriber> sub(new struct subscriber(qos, client));
    subscribers.insert(std::shared_ptr<subscriber>(sub));
    // It must be added to the session if cleansession is false
//    if (!cleansession)
//        client->session.subscriptions.push_back(*this);

}

void topic::del_subscriber(struct client *client,
                          bool cleansession)
{
    // TODO test this out in usub handler
//    for (auto it = subscribers.begin(); it != subscribers.end(); it++)
//        if (it->get()->client == client)
//        {
//            subscribers.erase(it);
//            printf("SUBSCRIBER DELETED\n");
//            break;
//        }
    // TODO remomve in case of cleansession == false
}

client::~client()
{
    // reach every topic and delete this client subscription
    for (auto el: session.subscriptions)
    {
        el->del_subscriber(this, session.cleansession);
        el.reset();
    }
    printf("CLIENT DELETED\n");
}
