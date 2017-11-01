#pragma once
#include <experimental/coroutine>

namespace net {

struct task {
  struct promise_type {
    constexpr task get_return_object() noexcept {
      return {};
    }

    constexpr auto initial_suspend() noexcept {
      return std::experimental::suspend_never{};
    }

    constexpr auto final_suspend() noexcept {
      return std::experimental::suspend_never{};
    }

    constexpr void return_void() noexcept {
    }

    void unhandled_exception() noexcept {
      std::terminate();
    }
  };
};

}  // namespace net
