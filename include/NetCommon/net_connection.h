#ifndef NET_CONNECTION_H
#define NET_CONNECTION_H

#include "net_common.h"
#include "net_message.h"
#include "net_tsqueue.h"

namespace tps
{
    namespace net
    {
        template <typename T>
        class server_interface;

        template <typename T>
        class connection: public std::enable_shared_from_this<connection<T>>
        {
        public:

            enum class owner
            {
                client,
                server
            };

            connection(owner parent, asio::io_context& asioContext, asio::ip::tcp::socket socket, tsqueue<owned_message<T>>& qIn):
                       m_socket(std::move(socket)), m_asioContext(asioContext), m_qMessageIn(qIn), m_nOwnerType(parent)
            {

            }

            ~connection()
            {
                std::cout << "[!]CONNECTION DELETED: "<< m_id << "\n";
            }

            void connect_to_client(uint32_t uid, server_interface<T>* server)
            {
                if (m_nOwnerType == owner::server)
                {
                    if (m_socket.is_open())
                    {
                        m_id = uid;

                        read_first_hdr(server);
                    }
                }
            }

            // ASYNC
            void connect_to_server(const asio::ip::tcp::resolver::results_type& endpoints)
            {
                if (m_nOwnerType == owner::client)
                {
                    asio::async_connect(m_socket, endpoints, [this](std::error_code ec, asio::ip::tcp::endpoint)
                    {
                        if (!ec)
                            read_header(nullptr);
                        else
                            std::cout << "Failed to connect to server\n";
                    });
                }
            }

            // ASYNC
            void disconnect(server_interface<T>* server)
            {
                asio::post(m_asioContext, [this, server]()
                {
                    if (m_socket.is_open())
                    {
                        m_socket.shutdown(asio::ip::tcp::socket::shutdown_both);
                        m_socket.close();
                        server->delete_client(this->shared_from_this());
                    }
                });
            }

            bool is_connected() const
            {
                return m_socket.is_open();
            }

            uint32_t get_ID() const
            {
                return m_id;
            }

            void set_timer(uint32_t mls)
            {
                m_timer = std::make_pair<asio::deadline_timer, uint32_t>
                        (asio::deadline_timer(m_asioContext, posix_time::millisec(mls)), uint32_t(mls));
            }

            void shutdown_cleanup(server_interface<T>* server)
            {
                if (is_connected())
                {
                    m_socket.shutdown(asio::ip::tcp::socket::shutdown_both);
                    m_socket.close();
                    std::cout << "SOCKET CLOSE\n";
                    if (m_nOwnerType == owner::server)
                    {
                        server->on_client_disconnect(this->shared_from_this());
                        server->delete_client(this->shared_from_this());
                    }
                }
            }

            // ASYNC
            template <typename Type>
            void send(Type&& msg, server_interface<T>* server)
            {
                asio::post(m_asioContext, [this, server, msg = std::forward<Type>(msg)]() mutable
                {
                    bool bWritingMessage = !m_qMessageOut.empty();
                    m_qMessageOut.push_back(std::forward<Type>(msg));
                    if (!bWritingMessage)
                        write_header(server);
                });
            }

            class decode_len_t
            {
            public:
                decode_len_t(message<T>& _m_msgTempIn): m_msgTempIn(_m_msgTempIn){}

                std::size_t operator()(const std::error_code& ec, std::size_t)
                {
                    static uint32_t len = 0;
                    static uint8_t lenIndex = 0;

                    if (ec)
                        return 0;

                    if (!m_msgTempIn.hdr.byte.byte)
                        return 2;

                    const uint8_t* const psize = reinterpret_cast<uint8_t*>(&m_msgTempIn.hdr.size);

                    len |= (psize[lenIndex] & 0x7fu) << 7*(lenIndex);
                    if ((psize[lenIndex] & 0x80) != 0)
                    {
                        if (++lenIndex < sizeof(m_msgTempIn.hdr.size))
                            return 1;
                        else
                        {
                            // return imposible size to signal an error
                            m_msgTempIn.hdr.size = std::numeric_limits<decltype(m_msgTempIn.hdr.size)>::max();
                            return 0;
                        }
                    }

                    m_msgTempIn.hdr.size = len;
                    lenIndex = 0;
                    len = 0;

                    return 0;
                }

            private:
                message<T>& m_msgTempIn;
            };

            inline decode_len_t decode_len(message<T>& m_msgTempIn)
            {
                return decode_len_t(m_msgTempIn);
            }

            // ASYNC
            void read_header(server_interface<T>* server)
            {
                if (m_timer)
                    m_timer->first.async_wait([this](const std::error_code& ec)
                    {
                        if (!ec)
                            m_socket.cancel();
                    });

                asio::async_read(m_socket, asio::buffer(&m_msgTempIn.hdr, sizeof(m_msgTempIn.hdr)+1), decode_len(m_msgTempIn),
                    [this, server](const std::error_code& ec, std::size_t)
                    {
                        bool bValidRemainingField = (m_msgTempIn.hdr.size !=
                            std::numeric_limits<decltype(m_msgTempIn.hdr.size)>::max());
                        if (!ec && bValidRemainingField)
                        {
                            if (m_timer)
                                m_timer->first.expires_from_now(boost::posix_time::millisec(m_timer->second));

                            if (m_msgTempIn.hdr.size > 0)
                            {
                                m_msgTempIn.body.resize(m_msgTempIn.hdr.size);
                                read_body(server);
                            }
                            else
                                add_to_incoming_message_queue(server);
                        }
                        else
                        {
                            std::cout << "[" << m_id << "] Read First Header Fail: " <<
                                    (bValidRemainingField ? ec.message() : "Invalid remaining length") << "\n";
                            shutdown_cleanup(server);
                        }
                    });
            }

            // ASYNC
            void read_body(server_interface<T>* server)
            {
                asio::async_read(m_socket, asio::buffer(m_msgTempIn.body.data(), m_msgTempIn.body.size()),
                    [this, server](const std::error_code& ec, std::size_t len)
                    {
                        if (!ec)
                            add_to_incoming_message_queue(server);
                        else
                        {
                            std::cout << "[" << m_id << "] Read Body Fail\n";
                            shutdown_cleanup(server);
                        }
                    });
            }

            // ASYNC
            void write_header(server_interface<T>* server)
            {
                asio::async_write(m_socket, asio::buffer(&m_qMessageOut.front().hdr,
                                                          m_qMessageOut.front().writeHdrSize),
                    [this, server](const std::error_code& ec, std::size_t)
                    {
                        if (!ec)
                        {
                            if (m_qMessageOut.front().body.size() > 0)
                                write_body(server);
                            else
                            {
                                m_qMessageOut.pop_front();
                                if (!m_qMessageOut.empty())
                                    write_header(server);
                            }
                        }
                        else
                        {
                            std::cout << "[" << m_id << "] Write Header Fail: " << ec.message() << "\n";
                            shutdown_cleanup(server);
                        }
                    });
            }

            // ASYNC
            void write_body(server_interface<T>* server)
            {
                asio::async_write(m_socket, asio::buffer(m_qMessageOut.front().body.data(),
                                                         m_qMessageOut.front().body.size()),
                    [this, server](const std::error_code& ec, std::size_t)
                    {
                        if (!ec)
                        {
                                m_qMessageOut.pop_front();
                                if (!m_qMessageOut.empty())
                                    write_header(server);
                        }
                        else
                        {
                            std::cout << "[" << m_id << "] Write Body Fail\n";
                            shutdown_cleanup(server);
                        }
                    });
            }

            void add_to_incoming_message_queue(server_interface<T>* server)
            {
                if (m_nOwnerType == owner::server)
                    // server has an array of connections, so it needs to know which connection owns incoming message
                    m_qMessageIn.push_back(owned_message<T>({this->shared_from_this(), std::move(m_msgTempIn)}));
                else
                    // client has only 1 connection, this connection will own all of incoming msgs
                    m_qMessageIn.push_back(owned_message<T>({nullptr, std::move(m_msgTempIn)}));

                read_header(server);
            }

            // ASYNC
            void read_first_hdr(server_interface<T>* server)
            {
                asio::async_read(m_socket, asio::buffer(&m_msgTempIn.hdr, sizeof(m_msgTempIn.hdr)+1), decode_len(m_msgTempIn),
                    [this, server](const std::error_code& ec, std::size_t)
                    {
                        bool bValidRemainingField = (m_msgTempIn.hdr.size !=
                                std::numeric_limits<decltype(m_msgTempIn.hdr.size)>::max());
                        if (!ec && bValidRemainingField)
                        {
                            if (m_msgTempIn.hdr.size > 0)
                            {
                                m_msgTempIn.body.resize(m_msgTempIn.hdr.size);
                                read_first_body(server);
                            }
                            else
                            {
                                if (server->on_first_message(this->shared_from_this(), m_msgTempIn))
                                    add_to_incoming_message_queue(server);
                                else
                                {
                                    std::cout << "[" << m_id << "] Invalid First Msg Received\n";
                                    m_socket.shutdown(asio::ip::tcp::socket::shutdown_both);
                                    m_socket.close();
                                    server->delete_client(this->shared_from_this());
                                }
                            }
                        }
                        else
                        {
                            std::cout << "[" << m_id << "] Read First Header Fail: " <<
                                    (bValidRemainingField ? ec.message() : "Invalid remaining length") << "\n";
                            m_socket.shutdown(asio::ip::tcp::socket::shutdown_both);
                            m_socket.close();
                            server->delete_client(this->shared_from_this());
                        }
                    });
            }

            // ASYNC
            void read_first_body(server_interface<T>* server)
            {
                asio::async_read(m_socket, asio::buffer(m_msgTempIn.body.data(), m_msgTempIn.body.size()),
                    [this, server](const std::error_code& ec, std::size_t)
                    {
                        if (!ec)
                        {
                            if (server->on_first_message(this->shared_from_this(), m_msgTempIn))
                                add_to_incoming_message_queue(server);
                            else
                            {
                                std::cout << "[" << m_id << "] Invalid First Msg Received\n";
                                m_socket.shutdown(asio::ip::tcp::socket::shutdown_both);
                                m_socket.close();
                                server->delete_client(this->shared_from_this());
                            }
                        }
                        else
                        {
                            std::cout << "[" << m_id << "] Read First Body Fail\n";
                            m_socket.shutdown(asio::ip::tcp::socket::shutdown_both);
                            m_socket.close();
                            server->delete_client(this->shared_from_this());
                        }
                    });
            }

        private:
            asio::ip::tcp::socket m_socket;

            asio::io_context& m_asioContext;

            tsqueue<message<T>> m_qMessageOut;

            tsqueue<owned_message<T>>& m_qMessageIn;

            message<T> m_msgTempIn;

            owner m_nOwnerType = owner::server;

            uint32_t m_id = 0;

            std::optional<std::pair<asio::deadline_timer, uint32_t>> m_timer;
        };
    }
}

#endif // NET_CONNECTION_H
