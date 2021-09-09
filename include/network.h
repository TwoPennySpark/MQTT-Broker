#ifndef NETWORK_H
#define NETWORK_H

#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <vector>

/* Set non-blocking socket */
int set_nonblocking(int);

/*
 * Set TCP_NODELAY flag to true, disabling Nagle's algorithm, no more waiting
 * for incoming packets on the buffer
 */
int set_tcp_nodelay(int);

/* Auxiliary function for creating epoll server */
int create_and_bind(const char *, uint16_t);

/*
 * Create a non-blocking socket and make it listen on the specfied address and
 * port
 */
int make_listen(const char *, uint16_t);

/* Accept a connection and add it to the right epollfd */
int accept_connection(int);

/* I/O management functions */

/*
 * Send all data in a loop, avoiding interruption based on the kernel buffer
 * availability
 */
ssize_t send_bytes(int, const unsigned char *, size_t);

/*
 * Receive (read) an arbitrary number of bytes from a file descriptor and
 * store them in a buffer
 */
ssize_t recv_bytes(int, unsigned char *, size_t);


/* Event loop wrapper class, define an EPOLL loop and his status. The
 * EPOLL instance use EPOLLONESHOT for each event and must be re-armed
 * manually, in order to allow future uses on a multithreaded architecture.
 */
class evloop {

private:
    int epollfd;
    int max_events;
    int timeout;
    int status;
    std::vector<struct epoll_event> events;
    /* Dynamic array of periodic tasks, a pair descriptor - closure */
    uint periodic_maxsize;
    uint periodic_nr;
    struct periodic_task{
        int timerfd;
        struct closure *closure;
    };
    std::vector<periodic_task> periodic_tasks;

public:
    evloop(int _max_events, int _timeout);

    /* Epoll management methods */
    int epoll_add(int fd, uint evs, void *data);
    /*
     * Modify an epoll-monitored descriptor, automatically set EPOLLONESHOT in
     * addition to the other flags, which can be EPOLLIN for read and EPOLLOUT for
     * write
     */
    int epoll_mod(int fd, uint evs, void *data);
    /*
     * Remove a descriptor from an epoll descriptor, making it no-longer monitored
     * for events
     */
    int epoll_del(int fd);

    /*
     * Register a closure with a function to be executed every time the
     * paired descriptor is re-armed.
     */
    void evloop_add_callback(struct closure *cb);
    /*
     * Register a periodic closure with a function to be executed every
     * defined interval of time.
     */
    void evloop_add_periodic_task(int seconds,
                                  long ns,
                                  struct closure *cb);
    /*
     * Rearm the file descriptor associated with a closure for read action,
     * making the event loop to monitor the callback for reading events
     */
    int evloop_rearm_callback_read(struct closure *cb);
    /*
     * Rearm the file descriptor associated with a closure for write action,
     * making the event loop to monitor the callback for writing events
     */
    int evloop_rearm_callback_write(struct closure *cb);
    /*
     * Unregister a closure by removing the associated descriptor from the
     * EPOLL loop
     */
    int evloop_del_callback(struct closure *cb);

    /*
     * Blocks in a while(1) loop awaiting for events to be raised on monitored
     * file descriptors and executing the paired callback previously registered
     */
    int evloop_wait();
};

typedef void callback(evloop *, void *);

#define UUID_LEN 16
/*
 * Callback object, represents a callback function with an associated
 * descriptor if needed, args is a void pointer which can be a structure
 * pointing to callback parameters and closure_id is a UUID for the closure
 * itself.
 * The last two fields are payload, a serialized version of the result of
 * a callback, ready to be sent through wire and a function pointer to the
 * callback function to execute.
 */
struct closure {
    int fd;
    void *obj;
    void *args;
    char closure_id[UUID_LEN];
    struct bytestring *payload;
    callback *call;
};

#endif // NETWORK_H
