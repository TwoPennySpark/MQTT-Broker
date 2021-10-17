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
    subscribers.push_back(std::shared_ptr<subscriber>(sub));
    // It must be added to the session if cleansession is false
    if (!cleansession)
        client->session.subscriptions.push_back(*this);
//        client->session.subscriptions =
//            list_push(client->session.subscriptions, t);

}

//void topic_del_subscriber(struct topic *t,
//                          struct sol_client *client,
//                          bool cleansession) {
//    list_remove_node(t->subscribers, client, compare_cid);
//    // TODO remomve in case of cleansession == false
//}
