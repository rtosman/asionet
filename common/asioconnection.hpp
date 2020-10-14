#ifndef _ASIOCONNECTION_HPP_INCLUDED
#define _ASIOCONNECTION_HPP_INCLUDED
#include "asionet.hpp"
#include "asioqueue.hpp"
#include "asiomsg.hpp"

namespace asionet 
{
    template <typename T>
    struct connection: public std::enable_shared_from_this<connection<T>>
    {
        connection(asio::io_context& ctxt, 
                   asio::ip::tcp::socket socket,
                   protqueue<owned_message<T>>& inbound): 
                    m_context(ctxt),
                    m_socket(std::move(socket)),
                    m_inbound(inbound)
        {
        }
        virtual ~connection()
        {}
        
        void connect(const asio::ip::tcp::resolver::results_type& endpoints)
        {
            // Request asio attempts to connect to an endpoint
            asio::async_connect(m_socket, endpoints,
                [this](std::error_code ec, asio::ip::tcp::endpoint endpoint)
                {
                    if (!ec)
                    {
                        handle_rd_header();
                    }
                });
        };

        bool disconnect()
        {
            m_socket.close();
            return true; // TODO
        };

        [[nodiscard]] bool is_connected() const
        {
            return m_socket.is_open();
        };

        void send(const message<T>& msg)
        {
            asio::post(m_context,
                [this, msg]()
                {
                    // If the queue has a message in it, then we must 
                    // assume that it is in the process of asynchronously being written.
                    // Either way add the message to the queue to be output. If no messages
                    // were available to be written, then start the process of writing the
                    // message at the front of the queue.
                    bool writing_msg = !m_outbound.empty();
                    m_outbound.push_back(msg);
                    if (!writing_msg)
                    {
                        handle_wr_header();
                    }
                });
        }

/*
        [[nodiscard]] std::shared_ptr<message<T>> response()
        {
            asio::error_code            ec;
            std::shared_ptr<message<T>> resp(std::make_shared<message<T>>());

            size_t n = m_socket.read_some(asio::buffer(&resp->m_header, sizeof(resp->m_header)), ec);
            if(!ec && n == sizeof(resp->m_header))
            {
                if(resp->m_header.m_size > 0) {
                    resp->m_body.resize(resp->m_header.m_size);
                    m_socket.read_some(asio::buffer(resp->m_body.data(), resp->m_body.size()), ec);
                } 
            }

            return resp;
        }
*/
    private:
        asio::io_context&               m_context;
        protqueue<owned_message<T>>&    m_inbound;
        asio::ip::tcp::socket           m_socket;
        
        message<T>              m_temp;
        protqueue<message<T>>   m_outbound;

        void handle_wr_header()
        {
            // If this function is called, we know the outgoing message queue must have 
            // at least one message to send. So allocate a transmission buffer to hold
            // the message, and issue the work - asio, send these bytes
            asio::async_write(m_socket, 
                              asio::buffer(&m_outbound.front().m_header, 
                                           sizeof(m_outbound.front().m_header)),
                [this](std::error_code ec, std::size_t length)
                {
                    // asio has now sent the bytes - if there was a problem
                    // an error would be available...
                    if (!ec)
                    {
                        // ... no error, so check if the message header just sent also
                        // has a message body...
                        if (m_outbound.front().m_body.size() > 0)
                        {
                            // ...it does, so issue the task to write the body bytes
                            handle_wr_body();
                        }
                        else
                        {
                            // ...it didnt, so we are done with this message. Remove it from 
                            // the outgoing message queue
                            m_outbound.pop_front();

                            // If the queue is not empty, there are more messages to send, so
                            // make this happen by issuing the task to send the next header.
                            if (!m_outbound.empty())
                            {
                                handle_wr_header();
                            }
                        }
                    }
                    else
                    {
                        // ...asio failed to write the message, we could analyse why but 
                        // for now simply assume the connection has died by closing the
                        // socket. When a future attempt to write to this client fails due
                        // to the closed socket, it will be tidied up.
                        std::cout << "[" << (uint32_t)m_outbound.front().m_header.m_id << "] Write Header Fail.\n";
                        m_socket.close();
                    }
                });
			}

			void handle_wr_body()
			{
                // If this function is called, a header has just been sent, and that header
				// indicated a body existed for this message. Fill a transmission buffer
				// with the body data, and send it!
				asio::async_write(m_socket, 
                                  asio::buffer(m_outbound.front().m_body.data(), 
                                               m_outbound.front().m_body.size()),
					[this](std::error_code ec, std::size_t length)
					{
						if (!ec)
						{
							// Sending was successful, so we are done with the message
							// and remove it from the queue
							m_outbound.pop_front();

							// If the queue still has messages in it, then issue the task to 
							// send the next messages' header.
							if (!m_outbound.empty())
							{
								handle_wr_header();
							}
						}
						else
						{
							// Sending failed, see WriteHeader() equivalent for description :P
                            std::cout << "[" << (uint32_t)m_outbound.front().m_header.m_id << "] Write Body Fail.\n";
							m_socket.close();
						}
					});
			}

			// ASYNC - Prime context ready to read a message header
			void handle_rd_header()
			{
				// If this function is called, we are expecting asio to wait until it receives
				// enough bytes to form a header of a message. We know the headers are a fixed
				// size, so allocate a transmission buffer large enough to store it. In fact, 
				// we will construct the message in a "temporary" message object as it's 
				// convenient to work with.
				asio::async_read(m_socket, 
                                 asio::buffer(&m_temp.m_header, 
                                              sizeof(message_header<T>)),
					[this](std::error_code ec, std::size_t length)
					{						
						if (!ec)
						{
							// A complete message header has been read, check if this message
							// has a body to follow...
							if (m_temp.m_header.m_size > 0)
							{
								// ...it does, so allocate enough space in the messages' body
								// vector, and issue asio with the task to read the body.
								m_temp.m_body.resize(m_temp.m_header.m_size);
								handle_rd_body();
							}
							else
							{
								// it doesn't, so add this bodyless message to the connections
								// incoming message queue
								add_to_incoming();
							}
						}
						else
						{
							// Reading form the client went wrong, most likely a disconnect
							// has occurred. Close the socket and let the system tidy it up later.
							std::cout << "[" << (uint32_t)m_temp.m_header.m_id << "] Read Header Fail.\n";
							m_socket.close();
						}
					});
			}

			// ASYNC - Prime context ready to read a message body
			void handle_rd_body()
			{
				// If this function is called, a header has already been read, and that header
				// request we read a body, The space for that body has already been allocated
				// in the temporary message object, so just wait for the bytes to arrive...
				asio::async_read(m_socket, 
                                 asio::buffer(m_temp.m_body.data(), 
                                              m_temp.m_body.size()),
					[this](std::error_code ec, std::size_t length)
					{						
						if (!ec)
						{
							// ...and they have! The message is now complete, so add
							// the whole message to incoming queue
							add_to_incoming();
						}
						else
						{
							// As above!
							std::cout << "[" << (uint32_t)m_temp.m_header.m_id << "] Read Body Fail.\n";
							m_socket.close();
						}
					});
			}

			void add_to_incoming()
			{				
				// Shove it in queue, converting it to an "owned message", by initialising
				// with the a shared pointer from this connection object
				m_inbound.push_back({ m_temp, nullptr });

				// We must now prime the asio context to receive the next message. It 
				// wil just sit and wait for bytes to arrive, and the message construction
				// process repeats itself. Clever huh?
				handle_rd_header();
			}

    };
}

#endif
