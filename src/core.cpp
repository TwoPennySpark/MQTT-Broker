#include <string.h>
#include <stdlib.h>
#include <memory>
#include "core.h"
#include "trie.h"

//static int compare_cid(void *c1, void *c2) {
//    return strcmp(((struct subscriber *) c1)->client->client_id,
//                  ((struct subscriber *) c2)->client->client_id);
//}

void topic::add_subscriber(struct sol_client* client,
                          struct subscriber* sub,
                          bool cleansession)
{
//    std::shared_ptr<struct subscriber> sub(new struct subscriber(qos, client));
    subscribers.insert(std::shared_ptr<subscriber>(sub));
    // It must be added to the session if cleansession is false
//    if (!cleansession)
//        client->session.subscriptions.push_back(*this);

}

void topic::del_subscriber(struct sol_client *client,
                          bool cleansession)
{
    // TODO test this out in usub handler
    for (auto it = subscribers.begin(); it != subscribers.end(); it++)
        if (it->get()->client == client)
        {
            subscribers.erase(it);
            printf("SUBSCRIBER DELETED\n");
            break;
        }
    // TODO remomve in case of cleansession == false
}

sol_client::~sol_client()
{
    // reach every topic and delete this client subscription
    for (auto el: session.subscriptions)
    {
        el->del_subscriber(this, session.cleansession);
        el.reset();
    }
    cb.reset();
    printf("CLIENT DELETED\n");
}
