#include "network.h"

#define _DEFAULT_SOURCE
#include <stdlib.h>
#include <errno.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/eventfd.h>
#include "network.h"
#include "config.h"

/* Set non-blocking socket */
int set_nonblocking(int fd)
{
    int flags, result;
    flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
        goto err;
    result = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    if (result == -1)
        goto err;
    return 0;
err:
    perror("set_nonblocking");
    return -1;
}

/* Disable Nagle's algorithm by setting TCP_NODELAY */
int set_tcp_nodelay(int fd)
{
    int flag = 1;
    return setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int));
}

int create_and_bind(const char *sockpath, uint16_t port)
{
    struct sockaddr_in addr;
    int fd;

    if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
    {
        perror("socket error");
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    if (inet_pton(AF_INET, sockpath, &addr.sin_addr.s_addr) == -1)
    {
        perror("Invalid addr");
        return -1;
    }
    addr.sin_port = htons(port);

    if (bind(fd, (struct sockaddr*) &addr, sizeof(addr)) == -1)
    {
        perror("bind error");
        return -1;
    }
    return fd;
}

/*
 * Create a non-blocking socket and make it listen on the specfied address and
 * port
 */
int make_listen(const char *host, uint16_t port)
{
    int sfd;
    if ((sfd = create_and_bind(host, port)) == -1)
        abort();
    if ((set_nonblocking(sfd)) == -1)
        abort();
    set_tcp_nodelay(sfd);
    if ((listen(sfd, 0)) == -1) {
        perror("listen");
        abort();
    }
    return sfd;
}

int accept_connection(int serversock)
{
    int clientsock;
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    if ((clientsock = accept(serversock,
                             (struct sockaddr *) &addr, &addrlen)) < 0)
        return -1;
    set_nonblocking(clientsock);
    set_tcp_nodelay(clientsock);
    return clientsock;
}

/* Send all bytes contained in buf, updating sent bytes counter */
ssize_t send_bytes(int fd, std::vector<uint8_t>& buf, uint& iter, size_t len)
{
    size_t total = 0;
    size_t bytesleft = len;
    ssize_t n = 0;
    while (total < len) {
        n = send(fd, buf.data()+iter, bytesleft, MSG_NOSIGNAL);
        if (n == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            else
                goto err;
        }
        iter += n;
        total += n;
        bytesleft -= n;
    }
    return total;
err:
    fprintf(stderr, "send(2) - error sending data: %s", strerror(errno));
    return -1;
}

/*
 * Receive a given number of bytes on the descriptor fd, storing the stream of
 * data into a 2 Mb capped buffer
 */
ssize_t recv_bytes(int fd, std::vector<uint8_t>& buf, uint& iter, size_t bufsize)
{
    ssize_t n = 0;
    ssize_t total = 0;
    while (total < (ssize_t) bufsize)
    {
        if ((n = recv(fd, buf.data()+iter, bufsize - total, 0)) < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            } else
                goto err;
        }
        if (n == 0)
            return 0;
        iter += n;
        total += n;
    }
    return total;
err:
    perror("recv(2) - error reading data");
    return -1;
}

#define EVLOOP_INITIAL_SIZE 4

evloop::evloop(int _max_events, int _timeout)
{
    max_events = _max_events;
    events.resize(max_events);
    epollfd = epoll_create1(0);
    timeout = _timeout;
    periodic_maxsize = EVLOOP_INITIAL_SIZE;
    periodic_nr = 0;
    periodic_tasks.resize(EVLOOP_INITIAL_SIZE);
    status = 0;
}

int evloop::epoll_add(int fd, uint evs, void *data)
{
    struct epoll_event ev;
    ev.data.fd = fd;
    // Being ev.data a union, in case of data != NULL, fd will be set to random
    if (data)
        ev.data.ptr = data;
    ev.events = evs | EPOLLET | EPOLLONESHOT;
    return epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev);
}

int evloop::epoll_mod(int fd, uint evs, void *data)
{
    struct epoll_event ev;
    ev.data.fd = fd;
    // Being ev.data a union, in case of data != NULL, fd will be set to random
    if (data)
        ev.data.ptr = data;
    ev.events = evs | EPOLLET | EPOLLONESHOT;
    return epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &ev);
}

int evloop::epoll_del(int fd)
{
    return epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, nullptr);
}

void evloop::evloop_add_callback(struct closure *cb)
{
    if (epoll_add(cb->fd, EPOLLIN, cb) < 0)
        perror("Epoll register callback: ");
}

void evloop::evloop_add_periodic_task(int seconds,
                              long ns,
                              struct closure *cb)
{
    struct itimerspec timervalue;
    int timerfd = timerfd_create(CLOCK_MONOTONIC, 0);
    memset(&timervalue, 0x00, sizeof(timervalue));
    // Set initial expire time and periodic interval
    timervalue.it_value.tv_sec = seconds;
    timervalue.it_value.tv_nsec = ns;
    timervalue.it_interval.tv_sec = seconds;
    timervalue.it_interval.tv_nsec = ns;
    if (timerfd_settime(timerfd, 0, &timervalue, nullptr) < 0) {
        perror("timerfd_settime");
        return;
    }
    // Add the timer to the event loop
    struct epoll_event ev;
    ev.data.fd = timerfd;
    ev.events = EPOLLIN;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, timerfd, &ev) < 0) {
        perror("epoll_ctl(2): EPOLLIN");
        return;
    }
    /* Store it into the event loop */
    if (periodic_nr + 1 > periodic_maxsize) {
        periodic_maxsize *= 2;
        periodic_tasks.resize(periodic_maxsize);
    }
    periodic_tasks[periodic_nr].closure = cb;
    periodic_tasks[periodic_nr].timerfd = timerfd;
    periodic_nr++;
}

int evloop::evloop_wait()
{
    int rc = 0;
    int events_num = 0;
    long int timer = 0L;
    int periodic_done = 0;
    while (1)
    {
        events_num = epoll_wait(epollfd, events.data(),
                            max_events, timeout);
        if (events_num < 0)
        {
            /* Signals to all threads. Ignore it for now */
            if (errno == EINTR)
                continue;
            /* Error occured, break the loop */
            rc = -1;
            status = errno;
            break;
        }
        for (uint i = 0; i < events_num; i++)
        {
            /* Check for errors */
            if ((events[i].events & EPOLLERR) ||
                (events[i].events & EPOLLHUP) ||
                (!(events[i].events & EPOLLIN) &&
                 !(events[i].events & EPOLLOUT)))
            {
                /* An error has occured on this fd, or the socket is not
                   ready for reading, closing connection */
                perror ("epoll_wait(2)");
                epoll_del(events[i].data.fd);
                shutdown(events[i].data.fd, 0);
                close(events[i].data.fd);
                status = errno;
                continue;
            }
            struct closure *closure = (struct closure *)events[i].data.ptr;
            periodic_done = 0;
            for (uint i = 0; i < periodic_nr && periodic_done == 0; i++)
            {
                if (events[i].data.fd == periodic_tasks[i].timerfd)
                {
                    struct closure *c = periodic_tasks[i].closure;
                    (void) read(events[i].data.fd, &timer, 8);
                    c->call(*this, c->args);
                    periodic_done = 1;
                }
            }
            if (periodic_done == 1)
                continue;
            /* No error events, proceed to run callback */
            closure->call(*this, closure->args);
        }
    }
    return rc;
}

int evloop::get_status()
{
    return status;
}

int evloop::evloop_rearm_callback_read(struct closure *cb)
{
    return epoll_mod(cb->fd, EPOLLIN, cb);
}

int evloop::evloop_rearm_callback_write(struct closure *cb)
{
    return epoll_mod(cb->fd, EPOLLOUT, cb);
}

int evloop::evloop_del_callback(struct closure *cb)
{
    return epoll_del(cb->fd);
}


