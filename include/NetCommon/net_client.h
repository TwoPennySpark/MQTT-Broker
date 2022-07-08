#ifndef NET_CLIENT_H
#define NET_CLIENT_H

#include "net_common.h"
#include "net_message.h"
#include "net_tsqueue.h"

namespace tps
{
    namespace net
    {
        using namespace boost;

        template <typename T>
        class client_interface
        {
        public:
            client_interface() : m_socket(m_context)
            {

            }

            virtual ~client_interface()
            {
                disconnect();
            }

            bool connect(const std::string& host, uint16_t port)
            {
                try
                {
                    asio::ip::tcp::resolver resolver(m_context);
                    asio::ip::tcp::resolver::results_type endpoints = resolver.resolve(host, std::to_string(port));

                    m_connection = std::make_unique<connection<T>>(connection<T>::owner::client, m_context,
                                                                   asio::ip::tcp::socket(m_context), m_qMessageIn);

                    m_connection->connect_to_server(endpoints);

                    thrContext = std::thread([this](){m_context.run();});
                } catch (std::exception& e)
                {
                    std::cout << "[-]Client exception:" << e.what() << std::endl;
                    return false;
                }
                return true;
            }

            bool disconnect()
            {
                if (is_connected())
                {
                    m_connection->disconnect();
                }

                m_context.stop();
                if (thrContext.joinable())
                    thrContext.join();

                m_connection.release();
            }

            bool is_connected()
            {
                if (m_connection->is_connected())
                    return true;
                else
                    return false;
            }

            void send(const message<T>& msg)
            {
                if (is_connected())
                    m_connection->send(msg);
            }

            tsqueue<owned_message<T>>& incoming()
            {
                return m_qMessageIn;
            }

        protected:
            asio::io_context m_context;

            std::thread thrContext;

            asio::ip::tcp::socket m_socket;

            std::unique_ptr<connection<T>> m_connection;

        private:
            tsqueue<owned_message<T>> m_qMessageIn;

        };
    }
}

#endif // NET_CLIENT_H
