/**
    asio_http: http client library for boost asio
    Copyright (c) 2017 Julio Becerra Gomez
    See COPYING for license information.
*/

#ifndef ASIO_HTTP_SOCKET_H
#define ASIO_HTTP_SOCKET_H

#include "asio_http/http_request_interface.h"

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <memory>
#include <string>
#include <vector>

namespace asio_http
{
namespace internal
{
class transport_layer
{
public:
  transport_layer(transport_layer* upper)
      : upper_layer(upper)
  {
  }
  virtual ~transport_layer() {}
  virtual void on_connected(const boost::system::error_code& ec) {}
  virtual void read() {}
  virtual void on_read(const std::uint8_t* data, std::size_t size, boost::system::error_code ec) {}
  virtual void write(std::vector<std::uint8_t> data) {}
  virtual void on_write(const boost::system::error_code& ec) {}
  virtual void close() {}
  virtual bool is_open() { return false; }
  virtual void set_upper(transport_layer* upper) { upper_layer = upper; }

protected:
  transport_layer* upper_layer;
  void             call_on_read(const std::uint8_t* data, std::size_t size, boost::system::error_code ec)
  {
    if (upper_layer != nullptr)
    {
      upper_layer->on_read(data, size, ec);
    }
  }
  void call_on_connected(boost::system::error_code ec)
  {
    if (upper_layer != nullptr)
    {
      upper_layer->on_connected(ec);
    }
  }
  void call_on_write(boost::system::error_code ec)
  {
    if (upper_layer != nullptr)
    {
      upper_layer->on_write(ec);
    }
  }
};

template<typename Socket>
class generic_stream
    : public transport_layer
    , public std::enable_shared_from_this<generic_stream<Socket>>
{
public:
  static std::shared_ptr<transport_layer>
  connect(transport_layer* upper, boost::asio::io_context& context, const std::string& host, const std::string& port)
  {
    auto socket = std::shared_ptr<generic_stream<Socket>>(new generic_stream<Socket>(upper, context));
    socket->connect(host, port);
    return socket;
  }

  virtual bool is_open() override { return m_socket.is_open(); }

  virtual void write(std::vector<std::uint8_t> data) override
  {
    std::swap(m_write_buffer, data);
    boost::asio::async_write(
      m_socket,
      boost::asio::const_buffer(m_write_buffer.data(), m_write_buffer.size()),
      [ptr = this->shared_from_this()](auto&& ec, auto&& bytes) { ptr->write_handler(ec, bytes); });
  }

  virtual void read() override
  {
    m_socket.async_read_some(
      boost::asio::buffer(m_read_buffer),
      [ptr = this->shared_from_this()](auto&& ec, auto&& bytes) { ptr->read_handler(ec, bytes); });
  }

  virtual void close() override { m_socket.close(); }

private:
  Socket                         m_socket;
  std::vector<std::uint8_t>      m_write_buffer;
  std::vector<std::uint8_t>      m_read_buffer;
  boost::asio::ip::tcp::resolver m_resolver;

  generic_stream(transport_layer* upper, boost::asio::io_context& context)
      : transport_layer(upper)
      , m_read_buffer(1024)
      , m_socket(context)
      , m_resolver(context)
  {
  }

  void connect(const std::string& host, const std::string& port)
  {
    boost::asio::ip::tcp::resolver::query q(host, port);

    m_resolver.async_resolve(q,
                             [ptr = this->shared_from_this()](auto&& ec, auto&& it) { ptr->resolve_handler(ec, it); });
  }

  void resolve_handler(const boost::system::error_code& ec, boost::asio::ip::tcp::resolver::iterator it)
  {
    if (ec)
    {
      call_on_connected(ec);
      return;
    }
    boost::asio::ip::tcp::endpoint ep = *it;

    m_socket.async_connect(ep, [ptr = this->shared_from_this(), it](auto&& ec) { ptr->connect_handler(ec, it); });
  }

  void connect_handler(const boost::system::error_code& ec, boost::asio::ip::tcp::resolver::iterator it)
  {
    if (!ec)
    {
      call_on_connected(ec);
    }
    else if (it != boost::asio::ip::tcp::resolver::iterator())
    {
      boost::asio::ip::tcp::endpoint ep = *it;
      m_socket.async_connect(ep, [ptr = this->shared_from_this(), it](auto&& ec) { ptr->connect_handler(ec, it); });
    }
    else
    {
      call_on_connected(ec);
    }
  }

  void write_handler(const boost::system::error_code& ec, std::size_t bytes_transferred) { call_on_write(ec); }

  void read_handler(const boost::system::error_code& ec, std::size_t bytes_transferred)
  {
    call_on_read(m_read_buffer.data(), bytes_transferred, ec);
  }
};

using tcp_socket = generic_stream<boost::asio::ip::tcp::socket>;

class ssl_socket
    : public transport_layer
    , public std::enable_shared_from_this<ssl_socket>
{
public:
  static std::shared_ptr<transport_layer> connect(transport_layer*         upper,
                                                  boost::asio::io_context& context,
                                                  const std::string&       host,
                                                  const std::string&       port,
                                                  const ssl_settings&      ssl)
  {
    auto socket = std::shared_ptr<ssl_socket>(new ssl_socket(upper, context, host, ssl));
    socket->connect(host, port);
    return socket;
  }

  virtual bool is_open() override { return m_socket.lowest_layer().is_open(); }

  virtual void write(std::vector<std::uint8_t> data) override
  {
    std::swap(m_write_buffer, data);
    boost::asio::async_write(
      m_socket,
      boost::asio::const_buffer(m_write_buffer.data(), m_write_buffer.size()),
      [ptr = this->shared_from_this()](auto&& ec, auto&& bytes) { ptr->write_handler(ec, bytes); });
  }

  virtual void read() override
  {
    m_socket.async_read_some(
      boost::asio::buffer(m_read_buffer),
      [ptr = this->shared_from_this()](auto&& ec, auto&& bytes) { ptr->read_handler(ec, bytes); });
  }

  virtual void close() override { m_socket.lowest_layer().close(); }

private:
  boost::asio::ssl::context                              m_context;
  boost::asio::ssl::stream<boost::asio::ip::tcp::socket> m_socket;
  std::vector<std::uint8_t>                              m_write_buffer;
  std::vector<std::uint8_t>                              m_read_buffer;
  boost::asio::ip::tcp::resolver                         m_resolver;

  ssl_socket(transport_layer* upper, boost::asio::io_context& context, const std::string& host, const ssl_settings& ssl)
      : transport_layer(upper)
      , m_context(boost::asio::ssl::context::sslv23)
      , m_socket(context, m_context)
      , m_read_buffer(1024)
      , m_resolver(context)
  {
    m_context.set_default_verify_paths();
    if (!ssl.client_certificate_file.empty())
    {
      m_context.use_certificate_file(ssl.client_certificate_file, boost::asio::ssl::context_base::pem);
    }
    if (!ssl.client_private_key_file.empty())
    {
      m_context.use_private_key_file(ssl.client_private_key_file, boost::asio::ssl::context_base::pem);
    }
    if (!ssl.certificate_authority_bundle_file.empty())
    {
      m_context.use_certificate_chain_file(ssl.certificate_authority_bundle_file);
    }
    m_socket.set_verify_mode(boost::asio::ssl::verify_peer);
    m_socket.set_verify_callback(boost::asio::ssl::rfc2818_verification(host));
  }

  void connect(const std::string& host, const std::string& port)
  {
    boost::asio::ip::tcp::resolver::query q(host, port);

    m_resolver.async_resolve(q, [ptr = shared_from_this()](auto&& ec, auto&& it) { ptr->resolve_handler(ec, it); });
  }

  void resolve_handler(const boost::system::error_code& ec, boost::asio::ip::tcp::resolver::iterator it)
  {
    if (ec)
    {
      call_on_connected(ec);
      return;
    }
    boost::asio::ip::tcp::endpoint ep = *it;

    m_socket.lowest_layer().async_connect(
      ep, [ptr = this->shared_from_this(), it](auto&& ec) { ptr->connect_handler(ec, it); });
  }

  void connect_handler(const boost::system::error_code& ec, boost::asio::ip::tcp::resolver::iterator it)
  {
    if (!ec)
    {
      m_socket.async_handshake(boost::asio::ssl::stream<boost::asio::ip::tcp::socket>::handshake_type::client,
                               [ptr = this->shared_from_this()](auto&& ec) { ptr->handshake_handler(ec); });
    }
    else if (it != boost::asio::ip::tcp::resolver::iterator())
    {
      boost::asio::ip::tcp::endpoint ep = *it;
      m_socket.lowest_layer().async_connect(
        ep, [ptr = this->shared_from_this(), it](auto&& ec) { ptr->connect_handler(ec, it); });
    }
    else
    {
      call_on_connected(ec);
    }
  }

  void handshake_handler(const boost::system::error_code& ec) { call_on_connected(ec); }

  void write_handler(const boost::system::error_code& ec, std::size_t bytes_transferred) { call_on_write(ec); }

  void read_handler(const boost::system::error_code& ec, std::size_t bytes_transferred)
  {
    call_on_read(m_read_buffer.data(), bytes_transferred, ec);
  }
};
}  // namespace internal
}  // namespace asio_http

#endif
