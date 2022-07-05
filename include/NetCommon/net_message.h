#ifndef NET_MESSAGE_H
#define NET_MESSAGE_H

#include "net_common.h"
#include "mqtt.h"

namespace tps
{
    namespace net
    {
        #pragma pack(push,1)
        struct message_header
        {
            mqtt_header byte;
            uint32_t size = 0;
        };
        #pragma pack(pop)

        struct message
        {
            message_header hdr{};
            std::vector<uint8_t> body;

            size_t size() const
            {
                return body.size();
            }

            // PUSH
            template <typename DataType>
            message& operator<<(const DataType& data)
            {
                static_assert(std::is_standard_layout<DataType>::value, "Data is too complex to be pushed");

                if (m_end+sizeof(data) > body.size())
                    body.resize(m_end+sizeof(data));

                std::memcpy(&body[m_end], &data, sizeof(data));

                m_end += sizeof(data);

                return *this;
            }

            message& operator<<(const std::string& data)
            {
                if (m_end+data.size() > body.size())
                    body.resize(m_end+data.size());

                std::memcpy(&body[m_end], data.data(), data.size());
                m_end += data.size();

                return *this;
            }

            message& operator<<(const std::vector<uint8_t>& data)
            {
                if (m_end+data.size() > body.size())
                    body.resize(m_end+data.size());

                std::memcpy(&body[m_end], data.data(), data.size());
                m_end += data.size();

                return *this;
            }

            // POP
            template <typename DataType>
            message& operator>>(DataType& data)
            {
                static_assert(std::is_standard_layout<DataType>::value, "Data is too complex to be poped");

                std::memcpy(&data, &body[m_start], sizeof(data));

                m_start += sizeof(data);

                if (m_start == m_end)
                    m_start = m_end = 0;

                return *this;
            }

            message& operator>>(std::string& data)
            {
                // Request to pop an amount of data that exceeds the size of the message
                if (body.size() - m_start < data.size())
                    return *this;

                std::memcpy(&data[0], &body[m_start], data.size());

                m_start += data.size();

                if (m_start == m_end)
                    m_start = m_end = 0;

                return *this;
            }

            message& operator>>(std::vector<uint8_t>& data)
            {
                // Request to pop an amount of data that exceeds the size of the message
                if (body.size() - m_start < data.size())
                    return *this;

                std::memcpy(data.data(), &body[m_start], data.size());

                m_start += data.size();

                if (m_start == m_end)
                    m_start = m_end = 0;

                return *this;
            }

        private:
            uint32_t m_start = 0, m_end = 0;
        };

        class connection;

        struct owned_message
        {
            std::shared_ptr<connection> owner = nullptr;
            message msg;
        };
    }
}

#endif // NET_MESSAGE_H
