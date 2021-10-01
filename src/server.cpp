#include <time.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include "pack.h"
#include "util.h"
#include "mqtt.h"
#include "network.h"
#include "server.h"
#include "config.h"
#include "core.h"
#include "hashtable.h"

/* Seconds in a Sol, easter egg */
static const double SOL_SECONDS = 88775.24;

/*
 * General informations of the broker, all fields will be published
 * periodically to internal topics
 */
static struct sol_info info;

/* Broker global instance, contains the topic trie and the clients hashtable */
static struct sol sol;

/*
 * Prototype for a command handler, it accepts a pointer to the closure as the
 * link to the client sender of the command and a pointer to the packet itself
 */
typedef int handler(struct closure&, mqtt_packet*);

/* Command handler, each one have responsibility over a defined command packet */
static int connect_handler(struct closure& , mqtt_packet*);
//static int disconnect_handler(struct closure *, mqtt_packet *);
//static int subscribe_handler(struct closure *, mqtt_packet *);
//static int unsubscribe_handler(struct closure *, mqtt_packet *);
//static int publish_handler(struct closure *, mqtt_packet *);
//static int puback_handler(struct closure *, mqtt_packet *);
//static int pubrec_handler(struct closure *, mqtt_packet *);
//static int pubrel_handler(struct closure *, mqtt_packet *);
//static int pubcomp_handler(struct closure *, mqtt_packet *);
//static int pingreq_handler(struct closure *, mqtt_packet *);

/* Command handler mapped usign their position paired with their type */
static handler *handlers[15] = {
    nullptr,
    connect_handler,
//    nullptr,
//    publish_handler,
//    puback_handler,
//    pubrec_handler,
//    pubrel_handler,
//    pubcomp_handler,
//    subscribe_handler,
//    nullptr,
//    unsubscribe_handler,
//    nullptr,
//    pingreq_handler,
//    nullptr,
//    disconnect_handler
};

/*
 * Connection structure for private use of the module, mainly for accepting
 * new connections
 */
struct connection {
    char ip[INET_ADDRSTRLEN + 1];
    int fd;
};

/* I/O closures, for the 3 main operation of the server
 * - Accept a new connecting client
 * - Read incoming bytes from connected clients
 * - Write output bytes to connected clients
 */
static void on_read(evloop &, void *);
static void on_write(evloop &, void *);
static void on_accept(evloop &, void *);

/*
 * Periodic task callback, will be executed every N seconds defined on the
 * configuration
 */
static void publish_stats(evloop &, void *);

/*
 * Accept a new incoming connection assigning ip address and socket descriptor
 * to the connection structure pointer passed as argument
 */
static int accept_new_client(int fd, struct connection *conn)
{
    if (!conn)
        return -1;
    /* Accept the connection */
    int clientsock = accept_connection(fd);
    /* Abort if not accepted */
    if (clientsock == -1)
        return -1;
    /* Just some informations retrieval of the new accepted client connection */
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    if (getpeername(clientsock, (struct sockaddr *) &addr, &addrlen) < 0)
        return -1;
    char ip_buff[INET_ADDRSTRLEN + 1];
    if (inet_ntop(AF_INET, &addr.sin_addr, ip_buff, sizeof(ip_buff)) == NULL)
        return -1;
    struct sockaddr_in sin;
    socklen_t sinlen = sizeof(sin);
    if (getsockname(fd, (struct sockaddr *) &sin, &sinlen) < 0)
        return -1;
    conn->fd = clientsock;
    strcpy(conn->ip, ip_buff);
    return 0;
}

/*
 * Handle new connection, create a a fresh new struct client structure and link
 * it to the fd, ready to be set in EPOLLIN event
 */
static void on_accept(evloop& loop, void *arg)
{
    /* struct connection *server_conn = arg; */
    struct closure *server = (struct closure *)arg;
    struct connection conn;
    accept_new_client(server->fd, &conn);
    /* Create a client structure to handle his context connection */
    struct closure *client_closure = (struct closure *)malloc(sizeof(*client_closure));
    if (!client_closure)
        return;
    /* Populate client structure */
    client_closure->fd = conn.fd;
    client_closure->obj = nullptr;
//    client_closure->payload = nullptr;
    client_closure->args = client_closure;
    client_closure->call = on_read;
    generate_uuid(client_closure->closure_id);
    sol.closures.insert(std::make_pair(client_closure->closure_id, *client_closure));
//    hashtable_put(sol.closures, client_closure->closure_id, client_closure);
    /* Add it to the epoll loop */
    loop.evloop_add_callback(client_closure);
    /* Rearm server fd to accept new connections */
    loop.evloop_rearm_callback_read(server);
    /* Record the new client connected */
    info.nclients++;
    info.nconnections++;
    sol_info("New connection from %s on port %s", conn.ip, conf->port);
}

/*
 * Parse packet header, it is required at least the Fixed Header of each
 * packed, which is contained in the first 2 bytes in order to read packet
 * type and total length that we need to recv to complete the packet.
 *
 * This function accept a socket fd, a buffer to read incoming streams of
 * bytes and a structure formed by 2 fields:
 *
 * - buf -> a byte buffer, it will be malloc'ed in the function and it will
 *          contain the serialized bytes of the incoming packet
 * - flags -> flags pointer, copy the flag setting of the incoming packet,
 *            again for simplicity and convenience of the caller.
 */
static ssize_t recv_packet(int clientfd, std::vector<uint8_t>& buf,
                           uint recvIter, uint8_t& command)
{
    ssize_t nbytes = 0;

    /* Read the first byte, it should contain the message type code */
    if ((nbytes = recv_bytes(clientfd, buf, recvIter, 1)) <= 0)
        return -ERRCLIENTDC;
    uint8_t byte = buf[0];
    if (DISCONNECT < byte || CONNECT > byte)
        return -ERRPACKETERR;

    /*
     * Read remaining length bytes which starts at byte 2 and can be long to 4
     * bytes based on the size stored, so byte 2-5 is dedicated to the packet
     * length.
     */
    ssize_t n = 0;
    do {
        if ((n = recv_bytes(clientfd, buf, recvIter, 1)) <= 0)
            return -ERRCLIENTDC;
        nbytes += n;
    } while (buf[recvIter-1] & (1 << 7)); // TO DO: add check for remaining len > 4

    uint decodeIter = 0;
    decodeIter += sizeof(command);
    uint64_t tlen = mqtt_decode_length(buf, decodeIter);

    /*
     * Set return code to -ERRMAXREQSIZE in case the total packet len exceeds
     * the configuration limit `max_request_size`
     */
    if (tlen > conf->max_request_size) {
        nbytes = -ERRMAXREQSIZE;
        goto exit;
    }

    /* Read remaining bytes to complete the packet */
    if ((n = recv_bytes(clientfd, buf, recvIter, tlen)) < 0)
        goto err;
    nbytes += n;
    command = byte;
exit:
    return nbytes;
err:
    shutdown(clientfd, 0);
    close(clientfd);
    return nbytes;
}

/* Handle incoming requests, after being accepted or after a reply */
static void on_read(evloop& loop, void *arg)
{
    struct closure *cb = (closure*)arg;

    /* Raw bytes buffer to handle input from client */
    std::vector<uint8_t> buffer(conf->max_request_size);
    uint iter = 0;
    ssize_t bytes = 0;
    uint8_t command = 0;
    int rc = 0;
    std::shared_ptr<mqtt_packet> pkt;

    /*
     * We must read all incoming bytes till an entire packet is received. This
     * is achieved by following the MQTT v3.1.1 protocol specifications, which
     * send the size of the remaining packet as the second byte. By knowing it
     * we know if the packet is ready to be deserialized and used.
     */
    bytes = recv_packet(cb->fd, buffer, iter, command);

    /*
     * Looks like we got a client disconnection.
     *
     * TODO: Set a error_handler for ERRMAXREQSIZE instead of dropping client
     *       connection, explicitly returning an informative error code to the
     *       client connected.
     */
    if (bytes == -ERRCLIENTDC || bytes == -ERRMAXREQSIZE)
        goto exit;

    /*
     * If a not correct packet received, we must free the buffer and reset the
     * handler to the request again, setting EPOLL to EPOLLIN
     */
    if (bytes == -ERRPACKETERR)
        goto errdc;
    info.bytes_recv++;

    /*
     * Unpack received bytes into a mqtt_packet structure and execute the
     * correct handler based on the type of the operation.
     */
    pkt = mqtt_packet::create(buffer);
    pkt->unpack(buffer);
//    unpack_mqtt_packet(buffer, packet);
//    union mqtt_header hdr(command);

    /* Execute command callback */
//    int rc = handlers[hdr.bits.type](cb, &packet);
    rc = handlers[pkt->header.bits.type](*cb, pkt.get());
    if (rc == REARM_W) {
        cb->call = on_write;

        /*
         * Reset handler to read_handler in order to read new incoming data and
         * EPOLL event for read fds
         */
        loop.evloop_rearm_callback_write(cb);
    } else if (rc == REARM_R) {
        cb->call = on_read;
        loop.evloop_rearm_callback_read(cb);
    }
    // Disconnect packet received
exit:
    return;
errdc:
    sol_error("Dropping client");
    shutdown(cb->fd, 0);
    close(cb->fd);
    sol.clients.erase(((struct sol_client *) cb->obj)->client_id);
    sol.closures.erase(cb->closure_id);
//    hashtable_del(sol.clients, ((struct sol_client *) cb->obj)->client_id);
//    hashtable_del(sol.closures, cb->closure_id);
    info.nclients--;
    info.nconnections--;
    return;
}

static void on_write(evloop &loop, void *arg)
{
    struct closure *cb = (closure*)arg;
    ssize_t sent;
    uint iter = 0;
    if ((sent = send_bytes(cb->fd, cb->payload, iter, cb->payload.size())) < 0)
        sol_error("Error writing on socket to client %s: %s",
                  ((struct sol_client *) cb->obj)->client_id.data(), strerror(errno));

    // Update information stats
    info.bytes_sent += sent;
//    bytestring_release(cb->payload);
//    cb->payload = NULL;

    /*
     * Re-arm callback by setting EPOLL event on EPOLLIN to read fds and
     * re-assigning the callback `on_read` for the next event
     */
    cb->call = on_read;
    loop.evloop_rearm_callback_read(cb);
}

/*
 * Statistics topics, published every N seconds defined by configuration
 * interval
 */
#define SYS_TOPICS 14

static std::string sys_topics[SYS_TOPICS] = {
    "$SOL/",
    "$SOL/broker/",
    "$SOL/broker/clients/",
    "$SOL/broker/bytes/",
    "$SOL/broker/messages/",
    "$SOL/broker/uptime/",
    "$SOL/broker/uptime/sol",
    "$SOL/broker/clients/connected/",
    "$SOL/broker/clients/disconnected/",
    "$SOL/broker/bytes/sent/",
    "$SOL/broker/bytes/received/",
    "$SOL/broker/messages/sent/",
    "$SOL/broker/messages/received/",
    "$SOL/broker/memory/used"
};

static void run(evloop& loop) {
    if (loop.evloop_wait() < 0) {
        sol_error("Event loop exited unexpectedly: %s", strerror(loop.get_status()));
    }
}

/*
 * Cleanup function to be passed in as destructor to the Hashtable for
 * connecting clients
 */
//static int client_destructor(struct hashtable_entry *entry) {
//    if (!entry)
//        return -1;
//    struct sol_client *client = entry->val;
//    if (client->client_id)
//        free(client->client_id);
//    free(client);
//    return 0;
//}

/*
 * Cleanup function to be passed in as destructor to the Hashtable for
 * registered closures.
 */
//static int closure_destructor(struct hashtable_entry *entry) {
//    if (!entry)
//        return -1;
//    struct closure *closure = entry->val;
//    if (closure->payload)
//        bytestring_release(closure->payload);
//    free(closure);
//    return 0;
//}

int start_server(const char *addr, uint16_t port)
{
    /* Initialize global Sol instance */
//    trie_init(&sol.topics);
//    sol.clients = hashtable_create(client_destructor);
//    sol.closures = hashtable_create(closure_destructor);

    struct closure server_closure;

    /* Initialize the sockets, first the server one */
    server_closure.fd = make_listen(addr, port);
//    server_closure.payload = nullptr;
    server_closure.args = &server_closure;
    server_closure.call = on_accept;
    generate_uuid(server_closure.closure_id);

    /* Generate stats topics */
    for (int i = 0; i < SYS_TOPICS; i++)
        sol_topic_put(&sol, topic_create(strdup(sys_topics[i])));
    evloop event_loop(EPOLL_MAX_EVENTS, EPOLL_TIMEOUT);
//    struct evloop *event_loop = evloop_create(EPOLL_MAX_EVENTS, EPOLL_TIMEOUT);

    /* Set socket in EPOLLIN flag mode, ready to read data */
    event_loop.evloop_add_callback(&server_closure);

    /* Add periodic task for publishing stats on SYS topics */
    // TODO Implement
    struct closure sys_closure = {
        .fd = 0,
//        .payload = nullptr,
        .args = &sys_closure,
        .call = publish_stats
    };
    generate_uuid(sys_closure.closure_id);

    /* Schedule as periodic task to be executed every 5 seconds */
    event_loop.evloop_add_periodic_task(5, 0, &sys_closure);
    sol_info("Server start");
    info.start_time = time(nullptr);
    run(event_loop);
//    hashtable_release(sol.clients);
//    hashtable_release(sol.closures);
    sol_info("Sol v%s exiting", VERSION);
    return 0;
}

static void publish_message(unsigned short pkt_id,
                            std::string& topic,
                            std::string& payload)
{
    /* Retrieve the Topic structure from the global map, exit if not found */
    struct topic *t = sol_topic_get(&sol, topic.data());
    if (!t)
        return;

    /* Build MQTT packet with command PUBLISH */
    mqtt_publish pkt(PUBLISH_BYTE,
                                                 pkt_id,
                                                 topic.size(),
                                                 topic,
                                                 payload.size(),
                                                 payload);
    size_t len;
    unsigned char *packed;

    /* Send payload through TCP to all subscribed clients of the topic */
    struct list_node *cur = t->subscribers->head;
    size_t sent = 0L;
    for (; cur; cur = cur->next)
    {
        sol_debug("Sending PUBLISH (d%i, q%u, r%i, m%u, %s, ... (%i bytes))",
                  pkt.publish.header.bits.dup,
                  pkt.publish.header.bits.qos,
                  pkt.publish.header.bits.retain,
                  pkt.publish.pkt_id,
                  pkt.publish.topic,
                  pkt.publish.payloadlen);
        len = MQTT_HEADER_LEN + sizeof(uint16_t) +
            pkt.publish.topiclen + pkt.publish.payloadlen;
        struct subscriber *sub = cur->data;
        struct sol_client *sc = sub->client;

        /* Update QoS according to subscriber's one */
        pkt.publish.header.bits.qos = sub->qos;
        if (pkt.publish.header.bits.qos > AT_MOST_ONCE)
            len += sizeof(uint16_t);
        int remaininglen_offset = 0;
        if ((len - 1) > 0x200000)
            remaininglen_offset = 3;
        else if ((len - 1) > 0x4000)
            remaininglen_offset = 2;
        else if ((len - 1) > 0x80)
            remaininglen_offset = 1;
        len += remaininglen_offset;
        packed = pack_mqtt_packet(&pkt, PUBLISH);
        if ((sent = send_bytes(sc->fd, packed, len)) < 0)
            sol_error("Error publishing to %s: %s",
                      sc->client_id, strerror(errno));

        // Update information stats
        info.bytes_sent += sent;
        info.messages_sent++;
        free(packed);
    }
}

/*
 * Publish statistics periodic task, it will be called once every N config
 * defined seconds, it publish some informations on predefined topics
 */
static void publish_stats(evloop& loop, void *args)
{
    std::string cclients = std::to_string(info.nclients);
    std::string bsent = std::to_string(info.bytes_sent);
    std::string msent = std::to_string(info.messages_sent);
    std::string mrecv = std::to_string(info.messages_recv);
    long long uptime = time(NULL) - info.start_time;
    std::string utime = std::to_string(uptime);
    double sol_uptime = (double)(time(NULL) - info.start_time) / SOL_SECONDS;
    std::string sutime = std::to_string(sol_uptime);

    publish_message(0, sys_topics[5], utime);
    publish_message(0, sys_topics[6], sutime);
    publish_message(0, sys_topics[7], cclients);
    publish_message(0, sys_topics[9], bsent);
    publish_message(0, sys_topics[11], msent);
    publish_message(0, sys_topics[12], mrecv);
}

//static int connect_handler(struct closure *cb, mqtt_packet& pkt)
//{

//    // TODO just return error_code and handle it on `on_read`
//    if (hashtable_exists(sol.clients,
//                         (const char *) pkt.header.payload.client_id))
//    {

//        // Already connected client, 2 CONNECT packet should be interpreted as
//        // a violation of the protocol, causing disconnection of the client

//        sol_info("Received double CONNECT from %s, disconnecting client",
//                 pkt->connect.payload.client_id);

//        close(cb->fd);
//        hashtable_del(sol.clients, (const char *) pkt->connect.payload.client_id);
//        hashtable_del(sol.closures, cb->closure_id);

//        // Update stats
//        info.nclients--;
//        info.nconnections--;

//        return -REARM_W;
//    }
//    sol_info("New client connected as %s (c%i, k%u)",
//             pkt->connect.payload.client_id,
//             pkt->connect.bits.clean_session,
//             pkt->connect.payload.keepalive);

//    /*
//     * Add the new connected client to the global map, if it is already
//     * connected, kick him out accordingly to the MQTT v3.1.1 specs.
//     */
//    struct sol_client *new_client = malloc(sizeof(*new_client));
//    new_client->fd = cb->fd;
//    const char *cid = (const char *) pkt->connect.payload.client_id;
//    new_client->client_id = strdup(cid);
//    hashtable_put(sol.clients, cid, new_client);

//    /* Substitute fd on callback with closure */
//    cb->obj = new_client;

//    /* Respond with a connack */
//    union mqtt_packet *response = malloc(sizeof(*response));
//    unsigned char byte = CONNACK_BYTE;

//    // TODO check for session already present

//    if (pkt->connect.bits.clean_session == false)
//        new_client->session.subscriptions = list_create(NULL);

//    unsigned char session_present = 0;
//    unsigned char connect_flags = 0 | (session_present & 0x1) << 0;
//    unsigned char rc = 0;  // 0 means connection accepted

//    response->connack = *mqtt_packet_connack(byte, connect_flags, rc);

//    cb->payload = bytestring_create(MQTT_ACK_LEN);
//    unsigned char *p = pack_mqtt_packet(response, CONNACK);
//    memcpy(cb->payload->data, p, MQTT_ACK_LEN);
//    free(p);

//    sol_debug("Sending CONNACK to %s (%u, %u)",
//              pkt->connect.payload.client_id,
//              session_present, rc);

//    free(response);

//    return REARM_W;
//}

static int32_t connect_handler(closure &cb, mqtt_packet* packet)
{
    mqtt_connect* pkt = dynamic_cast<mqtt_connect*>(packet);
    if (sol.clients.find(pkt->payload.client_id) != sol.clients.end())
    {
        // Already connected client, 2 CONNECT packet should be interpreted as
        // a violation of the protocol, causing disconnection of the client

        sol_info("Received double CONNECT from %s, disconnecting client",
                 pkt->payload.client_id.data());

        close(cb.fd);
        sol.clients.erase(pkt->payload.client_id);
        sol.closures.erase(cb.closure_id);

        // Update stats
        info.nclients--;
        info.nconnections--;

        return -REARM_W;
    }
    sol_info("New client connected as %s (c%i, k%u)",
             pkt->payload.client_id.data(),
             pkt->vhdr.bits.clean_session,
             pkt->payload.keepalive);

    /*
     * Add the new connected client to the global map, if it is already
     * connected, kick him out accordingly to the MQTT v3.1.1 specs.
     */
    std::shared_ptr<struct sol_client> new_client(new sol_client);
    new_client->fd = cb.fd;
    new_client->client_id = pkt->payload.client_id;
    sol.clients.insert(std::make_pair(new_client->client_id, *new_client));

    /* Substitute fd on callback with closure */
    cb.obj = new_client.get();

    /* Respond with a connack */
//    mqtt_packet *response = malloc(sizeof(*response));
//    unsigned char byte = CONNACK_BYTE;

    // TODO check for session already present

//    if (pkt->connect.bits.clean_session == false)
//        new_client->session.subscriptions = list_create(NULL);

    uint8_t session_present = 0;
    uint8_t connect_flags = 0 | (session_present & 0x1) << 0;
    uint8_t rc = 0;  // 0 means connection accepted

    if (pkt->vhdr.bits.clean_session)
    {
        session_present = 0;
        rc = 0;
    }
    else
    {
        // TODO: handle this case
    }
    mqtt_connack response;
    response.sp = session_present;
    response.rc = rc;

    response.pack(cb.payload);
    sol_debug("Sending CONNACK to %s (%u, %u)", new_client->client_id.data(), session_present, rc);

    return REARM_W;
}

static int disconnect_handler(struct closure *cb, mqtt_packet *pkt)
{
    // TODO just return error_code and handle it on `on_read`
    /* Handle disconnection request from client */
    struct sol_client *c = (struct sol_client*)cb->obj;
    sol_debug("Received DISCONNECT from %s", c->client_id.data());
    close(c->fd);
    sol.clients.erase(c->client_id);
    sol.closures.erase(cb->closure_id);
//    hashtable_del(sol.clients, c->client_id);
//    hashtable_del(sol.closures, cb->closure_id);
    // Update stats
    info.nclients--;
    info.nconnections--;
    // TODO remove from all topic where it subscribed
    return -REARM_W;
}

/* Recursive auxiliary function to subscribe to all children of a given topic */
//static void recursive_subscription(struct trie_node *node, void *arg)
//{
//    if (!node || !node->data)
//        return;
//    struct list_node *child = node->children->head;
//    for (; child; child = child->next)
//        recursive_subscription(child->data, arg);
//    struct topic *t = node->data;
//    struct subscriber *s = (subscriber*) arg;
//    t->subscribers.push_back(*s);
//}

/* Recursive auxiliary function to subscribe to all children of a given topic */
static void subscription(trie_node *node, void *arg)
{
    if (!node || !node->data)
        return;
    auto topic = (struct topic*)node->data;
    struct subscriber* sub = (struct subscriber*)arg;
    topic->add_subscriber(sub->client, *sub, true);
}

static int subscribe_handler(struct closure *cb, mqtt_packet *packet)
{
    struct sol_client *c = (sol_client*)cb->obj;
    bool wildcard = false;
    mqtt_subscribe* pkt = dynamic_cast<mqtt_subscribe*>(packet);

    /*
     * We respond to the subscription request with SUBACK and a list of QoS in
     * the same exact order of reception
     */
    std::vector<uint8_t> rcs(pkt->tuples_len);

    /* Subscribe packets contains a list of topics and QoS tuples */
    for (uint32_t i = 0; i < pkt->tuples_len; i++)
    {
        sol_debug("Received SUBSCRIBE from %s", c->client_id.data());

        /*
         * Check if the topic exists already or in case create it and store in
         * the global map
         */
        std::string topicName = pkt->tuples[i].topic;
        sol_debug("\t%s (QoS %i)", topicName.data(), pkt->tuples[i].qos);

        /* Recursive subscribe to all children topics if the topic ends with "/#" */
        if (topicName[pkt->tuples[i].topic_len - 1] == '#' &&
            topicName[pkt->tuples[i].topic_len - 2] == '/')
        {
            topicName.erase(std::remove(topicName.begin(), topicName.end(), '#'),
                            topicName.end());
            wildcard = true;
        }
        else if (topicName[pkt->tuples[i].topic_len - 1] != '/')
            topicName += "/";

        struct subscriber *sub = new subscriber(pkt->tuples[i].qos,
                                                (sol_client*)cb->obj);

        struct topic* topic = nullptr;
        struct trie_node* topic_node = sol.topics.find(topicName);
        if (topic_node && topic_node->data)
            topic = (struct topic*)topic_node->data;
        // TODO check for callback correctly set to obj
        if (!topic)
        {
            topic = new struct topic(topicName);
//            t = topic_create(strdup(topic));
            sol.topics.insert(topicName, topic);
            topic->add_subscriber((sol_client*)cb->obj, *sub, true);
//            sol_topic_put(&sol, topic);
        } else if (wildcard == true)
        {
//            struct subscriber *sub = new subscriber(pkt->tuples[i].qos,
//                                                    (sol_client*)cb->obj);

//            struct subscriber *sub = malloc(sizeof(*sub));
//            sub->client = cb->obj;
//            sub->qos = pkt->tuples[i].qos;
            sol.topics.apply_func(topicName, subscription, sub);
//            trie_prefix_map_tuple(&sol.topics, topicName,
//                                  recursive_subscription, sub);
        }

        // Clean session true for now
//        topic->add_subscriber((sol_client*)cb->obj, sub, true);
        rcs[i] = pkt->tuples[i].qos;
    }

    struct mqtt_suback response(SUBACK_BYTE, pkt->pkt_id, pkt->tuples_len, rcs);
    response.pack(cb->payload);
    sol_debug("Sending SUBACK to %s", c->client_id.data());

    return REARM_W;
}

static int unsubscribe_handler(struct closure *cb, union mqtt_packet *pkt)
{
    struct sol_client *c = cb->obj;
    sol_debug("Received UNSUBSCRIBE from %s", c->client_id);
    pkt->ack = *mqtt_packet_ack(UNSUBACK_BYTE, pkt->unsubscribe.pkt_id);
    unsigned char *packed = pack_mqtt_packet(pkt, UNSUBACK);
    cb->payload = bytestring_create(MQTT_ACK_LEN);
    memcpy(cb->payload->data, packed, MQTT_ACK_LEN);
    free(packed);
    sol_debug("Sending UNSUBACK to %s", c->client_id);
    return REARM_W;
}

static int publish_handler(struct closure *cb, union mqtt_packet *pkt) {
    struct sol_client *c = cb->obj;
    sol_debug("Received PUBLISH from %s (d%i, q%u, r%i, m%u, %s, ... (%i bytes))",
              c->client_id,
              pkt->publish.header.bits.dup,
              pkt->publish.header.bits.qos,
              pkt->publish.header.bits.retain,
              pkt->publish.pkt_id,
              pkt->publish.topic,
              pkt->publish.payloadlen);
    info.messages_recv++;
    char *topic = (char *) pkt->publish.topic;
    bool alloced = false;
    unsigned char qos = pkt->publish.header.bits.qos;

    /*
     * For convenience we assure that all topics ends with a '/', indicating a
     * hierarchical level
     */
    if (topic[pkt->publish.topiclen - 1] != '/') {
        topic = append_string((char *) pkt->publish.topic, "/", 1);
        alloced = true;
    }

    /*
     * Retrieve the topic from the global map, if it wasn't created before,
     * create a new one with the name selected
     */
    struct topic *t = sol_topic_get(&sol, topic);
    if (!t) {
        t = topic_create(strdup(topic));
        sol_topic_put(&sol, t);
    }

    // Not the best way to handle this
    if (alloced == true)
        free(topic);
    size_t publen;
    unsigned char *pub;
    struct list_node *cur = t->subscribers->head;
    for (; cur; cur = cur->next) {
        publen = MQTT_HEADER_LEN + sizeof(uint16_t) +
            pkt->publish.topiclen + pkt->publish.payloadlen;
        struct subscriber *sub = cur->data;
        struct sol_client *sc = sub->client;

        /* Update QoS according to subscriber's one */
        pkt->publish.header.bits.qos = sub->qos;
        if (pkt->publish.header.bits.qos > AT_MOST_ONCE)
            publen += sizeof(uint16_t);
        int remaininglen_offset = 0;
        if ((publen - 1) > 0x200000)
            remaininglen_offset = 3;
        else if ((publen - 1) > 0x4000)
            remaininglen_offset = 2;
        else if ((publen - 1) > 0x80)
            remaininglen_offset = 1;
        publen += remaininglen_offset;
        pub = pack_mqtt_packet(pkt, PUBLISH);
        ssize_t sent;
        if ((sent = send_bytes(sc->fd, pub, publen)) < 0)
            sol_error("Error publishing to %s: %s",
                      sc->client_id, strerror(errno));

        // Update information stats
        info.bytes_sent += sent;
        sol_debug("Sending PUBLISH to %s (d%i, q%u, r%i, m%u, %s, ... (%i bytes))",
                  sc->client_id,
                  pkt->publish.header.bits.dup,
                  pkt->publish.header.bits.qos,
                  pkt->publish.header.bits.retain,
                  pkt->publish.pkt_id,
                  pkt->publish.topic,
                  pkt->publish.payloadlen);
        info.messages_sent++;
        free(pub);
    }

    // TODO free publish

    if (qos == AT_LEAST_ONCE) {
        mqtt_puback *puback = mqtt_packet_ack(PUBACK_BYTE, pkt->publish.pkt_id);
        mqtt_packet_release(pkt, PUBLISH);
        pkt->ack = *puback;
        unsigned char *packed = pack_mqtt_packet(pkt, PUBACK);
        cb->payload = bytestring_create(MQTT_ACK_LEN);
        memcpy(cb->payload->data, packed, MQTT_ACK_LEN);
        free(packed);
        sol_debug("Sending PUBACK to %s", c->client_id);
        return REARM_W;
    } else if (qos == EXACTLY_ONCE) {

        // TODO add to a hashtable to track PUBREC clients last
        mqtt_pubrec *pubrec = mqtt_packet_ack(PUBREC_BYTE, pkt->publish.pkt_id);
        mqtt_packet_release(pkt, PUBLISH);
        pkt->ack = *pubrec;
        unsigned char *packed = pack_mqtt_packet(pkt, PUBREC);
        cb->payload = bytestring_create(MQTT_ACK_LEN);
        memcpy(cb->payload->data, packed, MQTT_ACK_LEN);
        free(packed);
        sol_debug("Sending PUBREC to %s", c->client_id);
        return REARM_W;
    }
    mqtt_packet_release(pkt, PUBLISH);

    /*
     * We're in the case of AT_MOST_ONCE QoS level, we don't need to sent out
     * any byte, it's a fire-and-forget.
     */
    return REARM_R;
}

static int puback_handler(struct closure *cb, union mqtt_packet *pkt) {
    sol_debug("Received PUBACK from %s",
              ((struct sol_client *) cb->obj)->client_id);
    // TODO Remove from pending PUBACK clients map
    return REARM_R;
}

static int pubrec_handler(struct closure *cb, union mqtt_packet *pkt) {
    struct sol_client *c = cb->obj;
    sol_debug("Received PUBREC from %s", c->client_id);
    mqtt_pubrel *pubrel = mqtt_packet_ack(PUBREL_BYTE, pkt->publish.pkt_id);
    pkt->ack = *pubrel;
    unsigned char *packed = pack_mqtt_packet(pkt, PUBREC);
    cb->payload = bytestring_create(MQTT_ACK_LEN);
    memcpy(cb->payload->data, packed, MQTT_ACK_LEN);
    free(packed);
    sol_debug("Sending PUBREL to %s", c->client_id);
    return REARM_W;
}

static int pubrel_handler(struct closure *cb, union mqtt_packet *pkt) {
    sol_debug("Received PUBREL from %s",
              ((struct sol_client *) cb->obj)->client_id);
    mqtt_pubcomp *pubcomp = mqtt_packet_ack(PUBCOMP_BYTE, pkt->publish.pkt_id);
    pkt->ack = *pubcomp;
    unsigned char *packed = pack_mqtt_packet(pkt, PUBCOMP);
    cb->payload = bytestring_create(MQTT_ACK_LEN);
    memcpy(cb->payload->data, packed, MQTT_ACK_LEN);
    free(packed);
    sol_debug("Sending PUBCOMP to %s",
              ((struct sol_client *) cb->obj)->client_id);
    return REARM_W;
}

static int pubcomp_handler(struct closure *cb, union mqtt_packet *pkt) {
    sol_debug("Received PUBCOMP from %s",
              ((struct sol_client *) cb->obj)->client_id);
    // TODO Remove from pending PUBACK clients map
    return REARM_R;
}

static int pingreq_handler(struct closure *cb, union mqtt_packet *pkt) {
    sol_debug("Received PINGREQ from %s",
              ((struct sol_client *) cb->obj)->client_id);
    pkt->header = *mqtt_packet_header(PINGRESP_BYTE);
    unsigned char *packed = pack_mqtt_packet(pkt, PINGRESP);
    cb->payload = bytestring_create(MQTT_HEADER_LEN);
    memcpy(cb->payload->data, packed, MQTT_HEADER_LEN);
    free(packed);
    sol_debug("Sending PINGRESP to %s",
              ((struct sol_client *) cb->obj)->client_id);
    return REARM_W;
}
