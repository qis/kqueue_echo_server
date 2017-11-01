#pragma once
#include <net/handle.h>
#include <system_error>

namespace net {

class kqueue final : public handle<kqueue, int, -1> {
public:
  std::error_code create() noexcept;
  std::error_code run(int processor = -1) noexcept;
  std::error_code close() noexcept;
};

enum class filter : short {
  recv = -1,
  send = -2,
};

class event {
public:
  virtual ~event() = default;
  virtual void operator()(int kqueue, int socket, char* data, std::size_t size) noexcept = 0;
};

bool queue(int kqueue, int socket, filter filter, event* event, const char* data, std::size_t size) noexcept;

}  // namespace net
