#include <net/kqueue.h>

#ifndef WIN32
#include <sys/types.h>
#include <sys/param.h>
#include <unistd.h>
#include <pwd.h>
#else
#include <windows.h>
#endif

#include <sys/event.h>
#include <array>

#if __FreeBSD__
#include <sys/cpuset.h>
#endif

namespace net {

std::error_code kqueue::create() noexcept {
  kqueue kqueue;
  kqueue.reset(::kqueue());
  if (!kqueue) {
    return { errno, std::system_category() };
  }
  struct ::kevent sig;
  EV_SET(&sig, SIGINT, EVFILT_SIGNAL, EV_ADD, 0, 0, nullptr);
  if (::kevent(kqueue.get(), &sig, 1, nullptr, 0, nullptr) < 0) {
    return { errno, std::system_category() };
  }
  ::signal(SIGINT, SIG_IGN);
  reset(kqueue.release());
  return {};
}

std::error_code kqueue::run(int processor) noexcept {
  if (processor >= 0) {
    cpuset_t set;
    CPU_ZERO(&set);
    CPU_SET(processor, &set);
    if (cpuset_setaffinity(CPU_LEVEL_WHICH, CPU_WHICH_TID, -1, sizeof(set), &set) < 0) {
      return { errno, std::system_category() };
    }
  }
  std::error_code ec;
  bool running = true;
  std::array<struct ::kevent, 32> events;
  const auto size = static_cast<int>(events.size());
  while (running) {
    const auto count = ::kevent(handle_, nullptr, 0, events.data(), size, nullptr);
    if (count <= 0) {
      ec = { errno, std::system_category() };
      break;
    }
    for (std::size_t i = 0, max = static_cast<std::size_t>(count); i < max; i++) {
      const auto& ev = events[i];
      if (ev.filter == EVFILT_SIGNAL) {
        running = false;
        break;
      }
      if (ev.udata) {
        const auto data = reinterpret_cast<char*>(ev.ext[2]);
        const auto size = static_cast<std::size_t>(ev.flags & EV_EOF ? 0 : ev.ext[3]);
        auto& cb = *reinterpret_cast<event*>(ev.udata);
        cb(handle_, static_cast<int>(ev.ident), data, size);
      }
    }
  }
  return ec;
}

std::error_code kqueue::close() noexcept {
  signal(SIGINT, nullptr);
  if (::close(handle_) < 0) {
    return { errno, std::system_category() };
  }
  return {};
}

bool queue(int kqueue, int socket, filter filter, event* event, const char* data, std::size_t size) noexcept {
  struct ::kevent ev;
  EV_SET(&ev, socket, static_cast<short>(filter), EV_ADD | EV_ONESHOT, 0, 0, event);
  ev.ext[2] = reinterpret_cast<decltype(ev.ext[2])>(data);
  ev.ext[3] = static_cast<decltype(ev.ext[3])>(size);
  return ::kevent(kqueue, &ev, 1, nullptr, 0, nullptr) == 0;
}

}  // namespace net
