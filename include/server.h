#ifndef SERVER_H
#define SERVER_H

#include <string>

/*
 * Epoll default settings for concurrent events monitored and timeout, -1
 * means no timeout at all, blocking undefinitely
 */
#define EPOLL_MAX_EVENTS    256
#define EPOLL_TIMEOUT       -1

/* Error codes for packet reception, signaling respectively
 * - client disconnection
 * - error reading packet
 * - error packet sent exceeds size defined by configuration (generally default
 *   to 2MB)
 */
#define ERRCLIENTDC         1
#define ERRPACKETERR        2
#define ERRMAXREQSIZE       3

/* Return code of handler functions, signaling if there's data payload to be
 * sent out or if the server just need to re-arm closure for reading incoming
 * bytes
 */
#define REARM_R             0
#define REARM_W             1

int start_server(const std::string& addr, uint16_t port);

/* Global informations statistics structure */
struct sol_info
{
    /* Number of clients currently connected */
    int32_t nclients;
    /* Total number of clients connected since the start */
    int32_t nconnections;
    /* Timestamp of the start time */
    int64_t start_time;
    /* Total number of bytes received */
    int64_t bytes_recv;
    /* Total number of bytes sent out */
    int64_t bytes_sent;
    /* Total number of sent messages */
    int64_t messages_sent;
    /* Total number of received messages */
    int64_t messages_recv;
};

#endif // SERVER_H
