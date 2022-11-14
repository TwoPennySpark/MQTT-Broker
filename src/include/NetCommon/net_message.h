#ifndef NET_MESSAGE_H
#define NET_MESSAGE_H

#include "net_common.h"

namespace tps
{
    namespace net
    {
        #pragma pack(push,1)
        template <typename T>
        struct message_header
        {
            message_header() = default;

            message_header(message_header& _hdr): byte(_hdr.byte), size(_hdr.size) {std::cout << "COPYC\n";}
            message_header(const message_header& _hdr): byte(_hdr.byte), size(_hdr.size) {std::cout << "[" << std::this_thread::get_id() << "]=COPYCC\n";}
            message_header& operator=(const message_header& _hdr) {byte = _hdr.byte; size = _hdr.size; return *this;}

            message_header(message_header&& _hdr): byte(_hdr.byte), size(_hdr.size) {_hdr.byte = 0; _hdr.size = 0;}

            T byte;
            uint32_t size = 0;
        };
        #pragma pack(pop)

        template <typename T>
        struct message
        {
            message_header<T> hdr{};
            uint8_t writeHdrSize = sizeof(T);

            std::vector<uint8_t> body;

            size_t size() const
            {
                return hdr.size;
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
//                hdr.size += sizeof(data);

                return *this;
            }

            message& operator<<(const std::string& data)
            {
                if (m_end+data.size() > body.size())
                    body.resize(m_end+data.size());

                std::memcpy(&body[m_end], data.data(), data.size());
                m_end += data.size();
//                hdr.size += data.size();

                return *this;
            }

            message& operator<<(const std::vector<uint8_t>& data)
            {
                if (m_end+data.size() > body.size())
                    body.resize(m_end+data.size());

                std::memcpy(&body[m_end], data.data(), data.size());
                m_end += data.size();
//                hdr.size += data.size();

                return *this;
            }

            // POP
            template <typename DataType>
            message& operator>>(DataType& data)
            {
                static_assert(std::is_standard_layout<DataType>::value, "Data is too complex to be poped");

                if (body.size() - m_start < sizeof(data))
                    throw std::runtime_error("Request to pop an amount of data that exceeds the size of the message");

                std::memcpy(&data, &body[m_start], sizeof(data));

                m_start += sizeof(data);
//                hdr.size -= sizeof(data);

                if (m_start == m_end)
                    m_start = m_end = 0;

                return *this;
            }

            message& operator>>(std::string& data)
            {
                // Request to pop an amount of data that exceeds the size of the message
                if (body.size() - m_start < data.size())
                    throw std::runtime_error("Request to pop an amount of data that exceeds the size of the message");

                std::memcpy(&data[0], &body[m_start], data.size());

                m_start += data.size();
//                hdr.size -= data.size();

                if (m_start == m_end)
                    m_start = m_end = 0;

                return *this;
            }

            message& operator>>(std::vector<uint8_t>& data)
            {
                // Request to pop an amount of data that exceeds the size of the message
                if (body.size() - m_start < data.size())
                    throw std::runtime_error("Request to pop an amount of data that exceeds the size of the message");

                std::memcpy(data.data(), &body[m_start], data.size());

                m_start += data.size();
//                hdr.size -= data.size();

                if (m_start == m_end)
                    m_start = m_end = 0;

                return *this;
            }

            bool is_data_left()
            {
                return m_start - hdr.size;
            }

        private:
            mutable uint32_t m_start = 0, m_end = 0;
        };

        template <typename T>
        class connection;

        template <typename T>
        struct owned_message
        {
            std::shared_ptr<connection<T>> owner = nullptr;
            message<T> msg;
        };
    }
}

#endif // NET_MESSAGE_H
