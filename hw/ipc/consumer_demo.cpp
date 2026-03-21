#include "consumer_node.h"

#include <chrono>
#include <csignal>
#include <iostream>
#include <string_view>
#include <thread>

static volatile sig_atomic_t g_running = 1;

void SignalHandler(int) { g_running = 0; }

int main(int argc, char *argv[]) {
  const char *path = "/mpsc_demo";
  size_t size = 65536;

  if (argc >= 2) {
    path = argv[1];
  }
  if (argc >= 3) {
    size = std::stoull(argv[2]);
  }

  std::cout << "Consumer: opening queue '" << path << "' (" << size
            << " bytes)\n";

  ipc::ConsumerNode consumer(path, size);
  std::cout << "Waiting for messages (Ctrl+C to quit)...\n";

  std::signal(SIGINT, SignalHandler);

  while (g_running) {
    auto msg = consumer.Receive();
    if (!msg) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      continue;
    }

    std::string_view payload(msg->payload.data(), msg->payload.size());

    switch (msg->type) {
    case ipc::MessageType::Text:
      std::cout << "[TEXT] " << payload << "\n";
      break;
    case ipc::MessageType::Command:
      std::cout << "[CMD] " << payload << "\n";
      if (payload == "QUIT") {
        std::cout << "Received QUIT, exiting.\n";
        return 0;
      }
      break;
    case ipc::MessageType::Binary:
      std::cout << "[BIN] " << msg->payload.size() << " bytes\n";
      break;
    case ipc::MessageType::Heartbeat:
      std::cout << "[HEARTBEAT]\n";
      break;
    }
  }

  std::cout << "\nInterrupted, exiting.\n";
  return 0;
}
