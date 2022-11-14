#ifndef NET_SERVER_H
#define NET_SERVER_H

#include "net_common.h"
#include "net_connection.h"
#include "net_message.h"
#include "net_tsqueue.h"

namespace tps
{
    namespace net
    {
        template <typename T>
        class server_interface
        {
        public:
            server_interface(uint16_t port) :
                m_asioAcceptor(m_asioContext, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port))
            {

            }

            virtual ~server_interface()
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
//                        std::cout << "[SERVER] New connection: " << socket.remote_endpoint() << std::endl;
                        std::shared_ptr<connection<T>> newconn = std::make_shared<connection<T>>(
                                    connection<T>::owner::server, this, m_asioContext, std::move(socket), m_qMessagesIn);

                        if (on_client_connect(newconn))
                        {
                            m_deqConnections.push_back(std::move(newconn));

                            m_deqConnections.back()->connect_to_client(m_nIDCounter++);

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

            template <typename Type>
            void message_client(std::shared_ptr<connection<T>>& client, Type&& msg)
            {
                client->send(std::forward<Type>(msg));
            }

            void message_all_clients(const message<T>& msg, std::shared_ptr<connection<T>> pIgnoreClient = nullptr)
            {
                for (auto& client: m_deqConnections)
                {
                    if (client != pIgnoreClient)
                        client->send(msg);
                }
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

            void delete_client(std::shared_ptr<connection<T>> client)
            {
                m_deqConnections.erase(
                            std::remove(m_deqConnections.begin(), m_deqConnections.end(), client), m_deqConnections.end());
            }

        protected:
            virtual bool on_client_connect(std::shared_ptr<connection<T>>)
            {
                return true;
            }

            virtual void on_message(std::shared_ptr<connection<T>>, message<T>&)
            {

            }

        public:
            virtual bool on_first_message(std::shared_ptr<connection<T>>, message<T>&)
            {
                return true;
            }

            virtual void on_client_disconnect(std::shared_ptr<connection<T>>)
            {

            }

        protected:
            tsqueue<owned_message<T>> m_qMessagesIn;

            asio::io_context m_asioContext;
            std::thread m_threadContext;

            asio::ip::tcp::acceptor m_asioAcceptor;

            std::deque<std::shared_ptr<connection<T>>> m_deqConnections;

            uint32_t m_nIDCounter = 10000;
        };
    }
}

#endif // NET_SERVER_H
