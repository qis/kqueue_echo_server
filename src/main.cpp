#include <net/kqueue.h>
#include <net/tcp/socket.h>
#include <net/task.h>
#include <iostream>
#include <vector>

net::task handle(net::tcp::socket client) noexcept {
  std::vector<char> buffer;
  buffer.resize(40960);
  const auto buffer_data = buffer.data();
  const auto buffer_size = buffer.size();
  while (true) {
    const auto data = co_await client.recv(buffer_data, buffer_size);
    if (data.empty()) {
      std::cout << "recv error\n";
      break;
    }
    if (!co_await client.send(data)) {
      std::cout << "send error\n";
      break;
    }
  }
  client.shutdown();
  co_return;
}

net::task accept(net::tcp::socket server) noexcept {
  while (true) {
    auto client = co_await server.accept();
    if (!client) {
      std::cout << "accept error\n";
      continue;
    }
    client.keepalive(true);
    client.nodelay(true);
    handle(std::move(client));
  }
  co_return;
}

int main(int argc, char* argv[]) {
  net::kqueue kqueue;
  if (const auto ec = kqueue.create()) {
    std::cout << ec << ": could not create kqueue" << std::endl;
    return ec.value();
  }

  net::tcp::socket server;
  if (const auto ec = server.create(kqueue, "0.0.0.0", "8080")) {
    std::cout << ec << ": could not create server socket" << std::endl;
    return ec.value();
  }
  if (const auto ec = server.reuseaddr(true)) {
    std::cout << ec << ": could not set reuseaddr option on server socket" << std::endl;
    return ec.value();
  }

  if (const auto ec = server.listen()) {
    std::cout << ec << ": could not listen on server socket" << std::endl;
    return ec.value();
  }

  accept(std::move(server));

  if (const auto ec = kqueue.run(0)) {
    std::cout << ec << ": service failed" << std::endl;
    return ec.value();
  }
}
