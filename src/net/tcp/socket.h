#pragma once
#include <net/handle.h>
#include <net/kqueue.h>
#include <experimental/coroutine>
#include <memory>
#include <string_view>
#include <system_error>

namespace net::tcp {

template <typename T>
class async {};

class socket final : public handle<socket, int, -1> {
public:
  constexpr socket() noexcept = default;
  constexpr socket(int kqueue, int socket) noexcept : handle(socket), kqueue_(kqueue) {
  }

  std::error_code create(kqueue& kqueue, const char* host, const char* port) noexcept;

  // Starts accepting connections.
  std::error_code listen(std::size_t backlog = 0) noexcept;

  // Sets reuse address socket option.
  std::error_code reuseaddr(bool enable) noexcept;

  // Sets reuse port socket option.
  std::error_code reuseport(bool enable) noexcept;

  // Sets keep alive socket option.
  std::error_code keepalive(bool enable) noexcept;

  // Sets no delay socket option.
  std::error_code nodelay(bool enable) noexcept;

  // Accepts connection.
  // Returns an invalid client on error.
  async<socket> accept() noexcept;

  // Reads data into buffer.
  // Returns empty string view on error.
  async<std::string_view>& recv(char* data, std::size_t size) noexcept;

  // Writes data.
  // Returns false on error.
  async<bool>& send(std::string_view data) noexcept;

  // Gracefully shuts down connection.
  std::error_code shutdown() noexcept;

  // Closes socket.
  std::error_code close() noexcept;

private:
  int kqueue_ = -1;
  std::unique_ptr<async<std::string_view>> recv_;
  std::unique_ptr<async<bool>> send_;
};

template <>
class async<socket> : public event {
public:
  using handle_type = std::experimental::coroutine_handle<>;
  using result_type = socket;

  async(int kqueue, int socket) noexcept;

  bool await_ready() noexcept {
    return ready_;
  }

  void await_suspend(handle_type handle) noexcept {
    handle_ = handle;
  }

  result_type&& await_resume() noexcept {
    return std::move(result_);
  }

  void operator()(int kqueue, int socket, char* data, std::size_t size) noexcept override;

private:
  handle_type handle_;
  result_type result_;
  bool ready_ = false;
};

template <>
class async<std::string_view> final : public event {
public:
  using handle_type = std::experimental::coroutine_handle<>;
  using result_type = std::string_view;

  async(int kqueue, int socket, char* data, std::size_t size) noexcept;

  bool await_ready() noexcept {
    return ready_;
  }

  void await_suspend(handle_type handle) noexcept {
    handle_ = handle;
  }

  result_type await_resume() noexcept {
    return result_;
  }

  void operator()(int kqueue, int socket, char* data, std::size_t size) noexcept override;

private:
  handle_type handle_;
  result_type result_;
  bool ready_ = false;
};

template <>
class async<bool> final : public event {
public:
  using handle_type = std::experimental::coroutine_handle<>;
  using result_type = bool;

  async(int kqueue, int socket, const char* data, std::size_t size) noexcept;

  bool await_ready() noexcept {
    return ready_;
  }

  void await_suspend(handle_type handle) noexcept {
    handle_ = handle;
  }

  result_type await_resume() noexcept {
    return result_;
  }

  void operator()(int kqueue, int socket, char* data, std::size_t size) noexcept override;

private:
  handle_type handle_;
  result_type result_;
  bool ready_ = false;
};

}  // namespace net::tcp
