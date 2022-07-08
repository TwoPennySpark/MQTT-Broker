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
                       m_nOwnerType(parent), m_asioContext(asioContext), m_socket(std::move(socket)), m_qMessageIn(qIn)
            {

            }

            ~connection()
            {

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
                        {
                            read_header();
                        }
                        else
                        {
                            std::cout << "Failed to connect to server\n";
                        }
                    });
                }
            }

            // ASYNC
            void disconnect()
            {
                if (is_connected())
                    asio::post(m_asioContext, [this](){m_socket.close();});
            }

            bool is_connected() const
            {
                return m_socket.is_open();
            }

            uint32_t get_ID() const
            {
                return m_id;
            }

            // ASYNC
            void send(const message<T>& msg)
            {
                asio::post(m_asioContext, [this, msg]()
                {
                    bool bWritingMessage = !m_qMessageOut.empty();
                    m_qMessageOut.push_back(msg);
                    if (!bWritingMessage)
                        write_header();
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
                            return 0; //
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
            void read_header()
            {
                asio::async_read(m_socket, asio::buffer(&m_msgTempIn.hdr, sizeof(m_msgTempIn.hdr)+1), decode_len(m_msgTempIn),
                    [this](const std::error_code& ec, std::size_t)
                    {
                        if (!ec)
                        {
                            if (m_msgTempIn.hdr.size > 0)
                            {
                                printf("HDR:%x SIZE:%x %d\n", m_msgTempIn.hdr.byte.byte, m_msgTempIn.hdr.size, m_msgTempIn.hdr.size);
                                m_msgTempIn.body.resize(m_msgTempIn.hdr.size);
                                read_body();
//                                memset(&m_msgTempIn.hdr, 0, sizeof(m_msgTempIn.hdr));
//                                read_header();
                            }
                            else
                            {
                                add_to_incoming_message_queue();
                            }
                        }
                        else
                        {
                            std::cout << "[" << m_id << "] Read Header Fail: " << ec.message() << "\n";
                            m_socket.close();
                        }
                    });
            }

            // ASYNC
            void read_body()
            {
                asio::async_read(m_socket, asio::buffer(m_msgTempIn.body.data(), m_msgTempIn.body.size()),
                    [this](const std::error_code& ec, std::size_t)
                    {
                        if (!ec)
                        {
                            add_to_incoming_message_queue();
                        }
                        else
                        {
                            std::cout << "[" << m_id << "] Read Body Fail\n";
                            m_socket.close();
                        }
                    });
            }

            // ASYNC
            void write_header()
            {
                asio::async_write(m_socket, asio::buffer(&m_qMessageOut.front().hdr, sizeof(message_header<T>)),
                    [this](const std::error_code& ec, std::size_t)
                    {
                        if (!ec)
                        {
                            if (m_qMessageOut.front().body.size() > 0)
                            {
                                write_body();
                            }
                            else
                            {
                                m_qMessageOut.pop_front();
                                if (!m_qMessageOut.empty())
                                    write_header();
                            }
                        }
                        else
                        {
                            std::cout << "[" << m_id << "] Write Header Fail: " << ec.message() << "\n";
                            m_socket.close();
                        }
                    });
            }

            // ASYNC
            void write_body()
            {
                asio::async_write(m_socket, asio::buffer(m_qMessageOut.front().body.data(), m_qMessageOut.front().body.size()),
                    [this](const std::error_code& ec, std::size_t)
                    {
                        if (!ec)
                        {
                                m_qMessageOut.pop_front();
                                if (!m_qMessageOut.empty())
                                    write_header();
                        }
                        else
                        {
                            std::cout << "[" << m_id << "] Write Body Fail\n";
                            m_socket.close();
                        }
                    });
            }

            void add_to_incoming_message_queue()
            {
                if (m_nOwnerType == owner::server)
                    m_qMessageIn.push_back({this->shared_from_this(), m_msgTempIn}); // server has an array of connections, so it needs to know which connection owns incoming message<T>
                else
                    m_qMessageIn.push_back({nullptr, m_msgTempIn}); // client has only 1 connection, this connection will own all of incoming msgs

                read_header();
            }

            // ASYNC
            void read_first_hdr(server_interface<T>* server)
            {
                asio::async_read(m_socket, asio::buffer(&m_msgTempIn.hdr, sizeof(m_msgTempIn.hdr)+1), decode_len(m_msgTempIn),
                    [this, server](const std::error_code& ec, std::size_t)
                    {
                        if (!ec)
                        {
                            if (m_msgTempIn.hdr.size > 0)
                            {
                                printf("HDR:%x SIZE:%x %d\n", m_msgTempIn.hdr.byte.byte,
                                       m_msgTempIn.hdr.size, m_msgTempIn.hdr.size);
                                m_msgTempIn.body.resize(m_msgTempIn.hdr.size);
                                read_first_body(server);
                            }
                        }
                        else
                        {
                            std::cout << "[" << m_id << "] Read First Header Fail: " << ec.message() << "\n";
                            m_socket.close();
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
                                read_header();
                        }
                        else
                        {
                            std::cout << "[" << m_id << "] Read First Body Fail\n";
                            m_socket.close();
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
        };
    }
}

#endif // NET_CONNECTION_H
