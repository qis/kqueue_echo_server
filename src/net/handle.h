#pragma once
#include <type_traits>
#include <utility>

namespace net {

template <typename I, typename Handle, Handle Invalid>
class handle {
public:
  using handle_type = Handle;
  constexpr static handle_type invalid_handle_value = Invalid;

  constexpr handle() noexcept = default;

  constexpr explicit handle(handle_type handle) noexcept : handle_(std::move(handle)) {
  }

  handle(handle&& other) noexcept : handle_(other.release()) {
  }

  handle& operator=(handle&& other) noexcept {
    reset(other.release());
    return *this;
  }

  handle(const handle& other) = delete;
  handle& operator=(const handle& other) = delete;

  virtual ~handle() {
    if (handle_ != invalid_handle_value) {
      static_cast<I*>(this)->close();
    }
  }

  constexpr explicit operator bool() const noexcept {
    return handle_ != invalid_handle_value;
  }

  constexpr explicit operator handle_type() const noexcept {
    return handle_;
  }

  constexpr handle_type get() const noexcept {
    return handle_;
  }

  void reset(handle_type value = invalid_handle_value) noexcept {
    if (handle_ != invalid_handle_value) {
      static_cast<I*>(this)->close();
    }
    handle_ = value;
  }

  handle_type release() noexcept {
    return std::exchange(handle_, invalid_handle_value);
  }

protected:
  handle_type handle_ = invalid_handle_value;
};

}  // namespace net
