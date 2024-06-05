#include <iostream>
#include <array>
#include <cstring>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <system_error>

constexpr const char* SOCKET_PATH = "/tmp/netlink_broadcast.sock";
constexpr size_t BUFFER_SIZE = 256;

class UnixSocketClient {
public:
    UnixSocketClient(const std::string& socketPath) : socketPath(socketPath) {
        setupSocket();
    }

    ~UnixSocketClient() {
        if (sock != -1) {
            close(sock);
        }
    }

    void listenForChanges() {
        std::array<char, BUFFER_SIZE> buffer{};
        while (true) {
            ssize_t len = recv(sock, buffer.data(), buffer.size() - 1, 0);
            if (len > 0) {
                buffer[len] = '\0';
                std::cout << "Received update: " << buffer.data() << std::endl;
            } else if (len == 0) {
                std::cerr << "Connection closed by server." << std::endl;
                break;
            } else {
                std::cerr << "Failed to receive message: " << std::strerror(errno) << std::endl;
                break;
            }
        }
    }

private:
    std::string socketPath;
    int sock = -1;

    void setupSocket() {
        sock = socket(AF_UNIX, SOCK_DGRAM, 0);
        if (sock < 0) {
            throw std::system_error(errno, std::system_category(), "Failed to create socket");
        }

        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        std::strncpy(addr.sun_path, socketPath.c_str(), sizeof(addr.sun_path) - 1);

        if (connect(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
            throw std::system_error(errno, std::system_category(), "Failed to connect to broadcast socket");
        }
    }
};

int main() {
    try {
        UnixSocketClient client(SOCKET_PATH);
        client.listenForChanges();
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }
    return 0;
}
