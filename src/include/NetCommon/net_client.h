#ifndef NET_CLIENT_H
#define NET_CLIENT_H

#include "net_server.h"

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

            std::future<bool> connect(const std::string& host, uint16_t port)
            {
                std::promise<bool> connectPromise;
                std::future<bool> futConnect = connectPromise.get_future();

                try
                {
                    asio::ip::tcp::resolver resolver(m_context);
                    asio::ip::tcp::resolver::results_type endpoints = resolver.resolve(host, std::to_string(port));

                    m_connection = std::make_shared<connection<T>>(connection<T>::owner::client, nullptr, m_context,
                                                                   asio::ip::tcp::socket(m_context), m_qMessageIn);

                    m_connection->connect_to_server(endpoints, connectPromise);

                    thrContext = std::thread([this](){m_context.run();});
                } catch (...) {
                    connectPromise.set_exception(std::current_exception());
                }

                return futConnect;
            }

            void disconnect()
            {
                m_connection->disconnect();

                if (thrContext.joinable())
                    thrContext.join();

//                m_connection.reset();
            }

            bool is_connected()
            {
                return (m_connection->is_connected()) ? true : false;
            }

            template <typename Type>
            void send(Type&& msg)
            {
                m_connection->send(std::forward<Type>(msg));
            }

            tsqueue<owned_message<T>>& incoming()
            {
                return m_qMessageIn;
            }

        protected:
            asio::io_context m_context;

            std::thread thrContext;

            asio::ip::tcp::socket m_socket;

            std::shared_ptr<connection<T>> m_connection;

        private:
            tsqueue<owned_message<T>> m_qMessageIn;

        };
    }
}

#endif // NET_CLIENT_H
