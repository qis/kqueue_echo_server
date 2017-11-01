#include <net/tcp/socket.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>

namespace net::tcp {
namespace {

using info_handle = struct ::addrinfo;

class info final : public handle<info, info_handle*, nullptr> {
public:
  handle_type operator->() const noexcept {
    return handle_;
  }

  std::error_code create(const char* host, const char* port, int socktype, int flags) noexcept {
    struct addrinfo* handle = nullptr;
    struct addrinfo hints = {};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = socktype;
    hints.ai_flags = flags;
    if (const auto rv = ::getaddrinfo(host, port, &hints, &handle)) {
      return { rv, std::system_category() };
    }
    reset(handle);
    return {};
  }

  void close() noexcept {
    ::freeaddrinfo(handle_);
  }
};

}  // namespace

std::error_code socket::create(kqueue& kqueue, const char* host, const char* port) noexcept {
  info info;
  if (const auto ec = info.create(host, port, SOCK_STREAM, AI_PASSIVE)) {
    return ec;
  }
  socket socket(kqueue.get(), ::socket(info->ai_family, info->ai_socktype | SOCK_NONBLOCK, info->ai_protocol));
  if (!socket) {
    return { errno, std::system_category() };
  }
  if (::bind(socket.get(), info->ai_addr, info->ai_addrlen) < 0) {
    return { errno, std::system_category() };
  }
  reset(socket.release());
  kqueue_ = kqueue.get();
  return {};
}

std::error_code socket::listen(std::size_t backlog) noexcept {
  if (!backlog) {
    int value = 0;
    size_t size = sizeof(value);
    if (sysctlbyname("kern.ipc.somaxconn", &value, &size, nullptr, 0) == 0 && value > 1) {
      backlog = static_cast<std::size_t>(value);
    } else {
      backlog = SOMAXCONN;
    }
  }
  if (::listen(handle_, static_cast<int>(backlog)) < 0) {
    return { errno, std::system_category() };
  }
  return {};
}

std::error_code socket::reuseaddr(bool enable) noexcept {
  int value = enable ? 1 : 0;
  if (::setsockopt(handle_, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value)) < 0) {
    return { errno, std::system_category() };
  }
  return {};
}

std::error_code socket::reuseport(bool enable) noexcept {
  int value = enable ? 1 : 0;
  if (::setsockopt(handle_, SOL_SOCKET, SO_REUSEPORT, &value, sizeof(value)) < 0) {
    return { errno, std::system_category() };
  }
  return {};
}

std::error_code socket::keepalive(bool enable) noexcept {
  int value = enable ? 1 : 0;
  if (::setsockopt(handle_, SOL_SOCKET, SO_KEEPALIVE, &value, sizeof(value)) < 0) {
    return { errno, std::system_category() };
  }
  return {};
}

std::error_code socket::nodelay(bool enable) noexcept {
  int value = enable ? 1 : 0;
  if (::setsockopt(handle_, SOL_SOCKET, TCP_NODELAY, &value, sizeof(value)) < 0) {
    return { errno, std::system_category() };
  }
  return {};
}

async<socket> socket::accept() noexcept {
  return { kqueue_, handle_ };
}

async<std::string_view> socket::recv(char* data, std::size_t size) noexcept {
  return { kqueue_, handle_, data, size };
}

async<bool> socket::send(std::string_view data) noexcept {
  return { kqueue_, handle_, data.data(), data.size() };
}

std::error_code socket::shutdown() noexcept {
  if (::shutdown(handle_, SHUT_RDWR) < 0) {
    return { errno, std::system_category() };
  }
  return {};
}

std::error_code socket::close() noexcept {
  if (::close(handle_) < 0) {
    return { errno, std::system_category() };
  }
  return {};
}

async<socket>::async(int kqueue, int socket) noexcept {
  struct sockaddr_storage storage;
  socklen_t socklen = sizeof(storage);
  auto addr = reinterpret_cast<struct sockaddr*>(&storage);
  const auto rv = ::accept4(socket, addr, &socklen, SOCK_NONBLOCK);
  if (rv != -1) {
    result_ = { kqueue, rv };
    ready_ = true;
    return;
  }
  if (errno == EWOULDBLOCK) {
    queue(kqueue, socket, filter::recv, this, nullptr, 0); 
  } else {
    ready_ = true;
  }
}

void async<socket>::operator()(int kqueue, int socket, char* data, std::size_t size) noexcept {
  struct sockaddr_storage storage;
  socklen_t socklen = sizeof(storage);
  auto addr = reinterpret_cast<struct sockaddr*>(&storage);
  if (const auto rv = ::accept4(socket, addr, &socklen, SOCK_NONBLOCK); rv != -1) {
    result_ = { kqueue, rv };
  }
  handle_.resume();
}

async<std::string_view>::async(int kqueue, int socket, char* data, std::size_t size) noexcept {
  const auto rv = ::read(socket, data, size);
  if (rv > 0) {
    result_ = { data, static_cast<std::size_t>(rv) };
    ready_ = true;
    return;
  }
  if (errno == EWOULDBLOCK) {
    queue(kqueue, socket, filter::recv, this, data, size);
  } else {
    ready_ = true;
  }
}

void async<std::string_view>::operator()(int kqueue, int socket, char* data, std::size_t size) noexcept {
  if (const auto rv = ::read(socket, data, size); rv > 0) {
    result_ = { data, static_cast<std::size_t>(rv) };
  }
  handle_.resume();
}

async<bool>::async(int kqueue, int socket, const char* data, std::size_t size) noexcept {
  const auto rv = ::write(socket, data, size);
  if (rv > 0) {
    const auto send = static_cast<std::size_t>(rv);
    if (send < size) {
      ready_ = !queue(kqueue, socket, filter::send, this, data + send, size - send);
    } else {
      result_ = true;
      ready_ = true;
    }
    return;
  }
  if (errno == EWOULDBLOCK) {
    queue(kqueue, socket, filter::send, this, data, size);
  } else {
    ready_ = true;
  }
}

void async<bool>::operator()(int kqueue, int socket, char* data, std::size_t size) noexcept {
  if (const auto rv = ::write(socket, data, size); rv > 0) {
    if (const auto send = static_cast<std::size_t>(rv); send < size) {
      if (!queue(kqueue, socket, filter::send, this, data + send, size - send)) {
        handle_.resume();
        return;
      }
    }
    result_ = true;
  }
  handle_.resume();
}

}  // namespace net::tcp
