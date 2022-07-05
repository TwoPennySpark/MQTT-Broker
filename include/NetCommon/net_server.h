#ifndef NET_SERVER_H
#define NET_SERVER_H

#include "NetCommon/net_common.h"
#include "NetCommon/net_connection.h"
#include "NetCommon/net_message.h"
#include "NetCommon/net_tsqueue.h"

namespace tps
{
    namespace net
    {
        class server
        {
        public:
            server(uint16_t port) :
                m_asioAcceptor(m_asioContext, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port))
            {

            }

            ~server()
            {
                stop();
            }

            bool start()
            {
                try
                {
                    wait_for_client_connection();

                    m_threadContext = std::thread([this](){m_asioContext.run();});
                } catch (std::exception& e)
                {
                    std::cout << "[SERVER]ERROR:" << e.what() << std::endl;
                    return false;
                }

                std::cout << "[SERVER]Started\n";
                return true;
            }

            void stop()
            {
                m_asioContext.stop();

                if (m_threadContext.joinable())
                    m_threadContext.join();

                std::cout << "[SERVER]Stopped\n";
            }

            // ASYNC
            void wait_for_client_connection()
            {
                m_asioAcceptor.async_accept([this](std::error_code ec, asio::ip::tcp::socket socket)
                {
                    if (!ec)
                    {
                        std::cout << "[SERVER] New connection: " << socket.remote_endpoint() << std::endl;
                        std::shared_ptr<connection> newconn = std::make_shared<connection>(
                                    connection::owner::server, m_asioContext, std::move(socket), m_qMessagesIn);

                        if (on_client_connect(newconn))
                        {
                            m_deqConnections.push_back(std::move(newconn));

                            m_deqConnections.back()->connect_to_client(nIDCounter++);

                            std::cout << "[" << m_deqConnections.back()->get_ID() << "] Connection approved\n";
                        }
                        else
                        {
                            std::cout << "[-]Connection Denied\n";
                        }
                    }
                    else
                    {
                        std::cout << "[-]Accept error: " << ec.message() << std::endl;
                    }

                    wait_for_client_connection();
                });
            }

            void message_client(std::shared_ptr<connection> client, const message& msg)
            {
                if (client && client->is_connected()) // Check every time?
                {
                    client->send(msg);
                }
                else
                {
                    on_client_disconnect(client);
                    client.reset();
                    m_deqConnections.erase(
                                std::remove(m_deqConnections.begin(), m_deqConnections.end(), client), m_deqConnections.end());
                }
            }

            void message_all_clients(const message& msg, std::shared_ptr<connection> pIgnoreClient = nullptr)
            {
                bool bInvalidClientExists = false;

                for (auto& client: m_deqConnections)
                {
                    if (client && client->is_connected())
                    {
                        if (client != pIgnoreClient)
                            client->send(msg);
                    }
                    else
                    {
                        on_client_disconnect(client);
                        client.reset();
                        bInvalidClientExists = true;
                    }
                }

                if (bInvalidClientExists)
                    m_deqConnections.erase(
                                std::remove(m_deqConnections.begin(), m_deqConnections.end(), nullptr), m_deqConnections.end());
            }

            void update(size_t nMaxMessages = std::numeric_limits<size_t>::max())
            {
                size_t nMessageCount = 0;
                while (nMessageCount <= nMaxMessages)
                {
                    if (m_qMessagesIn.empty())
                        m_qMessagesIn.wait();
                    auto msg = m_qMessagesIn.pop_front();
                    on_message(msg.owner, msg.msg);
                    nMessageCount++;
                }
            }

        protected:
            bool on_client_connect(std::shared_ptr<connection> client)
            {
                return true;
            }

            void on_client_disconnect(std::shared_ptr<connection> client)
            {

            }

            void on_message(std::shared_ptr<connection> client, message& msg)
            {

            }

            tsqueue<owned_message> m_qMessagesIn;

            asio::io_context m_asioContext;
            std::thread m_threadContext;

            asio::ip::tcp::acceptor m_asioAcceptor;

            std::deque<std::shared_ptr<connection>> m_deqConnections;

            uint32_t nIDCounter = 10000;
        };
    }
}

#endif // NET_SERVER_H
