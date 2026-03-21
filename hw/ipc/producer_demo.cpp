#include "producer_node.h"

#include <iostream>
#include <string>

int main(int argc, char *argv[]) {
  const char *path = "/mpsc_demo";
  size_t size = 65536;

  if (argc >= 2) {
    path = argv[1];
  }
  if (argc >= 3) {
    size = std::stoull(argv[2]);
  }

  std::cout << "Producer: creating queue '" << path << "' (" << size
            << " bytes)\n";

  ipc::ProducerNode producer(path, size);

  std::cout << "Type messages (Ctrl+D to quit):\n";

  std::string line;
  while (std::getline(std::cin, line)) {
    if (!producer.SendText(line)) {
      std::cerr << "Queue full, message dropped!\n";
      continue;
    }
    std::cout << "Sent: '" << line << "' (" << line.size() << " bytes)\n";
  }

  // Send quit command
  producer.SendCommand("QUIT");
  std::cout << "Sent QUIT command.\n";

  std::cout << "Press Enter to cleanup shared memory...";
  std::cin.clear();
  std::getline(std::cin, line);

  producer.Unlink();
  std::cout << "Done.\n";

  return 0;
}
