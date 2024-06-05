#include <iostream>
#include <array>
#include <cstring>
#include <netlink/netlink.h>
#include <netlink/socket.h>
#include <netlink/msg.h>
#include <netlink/route/rtnl.h>
#include <syslog.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <system_error>

constexpr const char* SOCKET_PATH = "/tmp/netlink_broadcast.sock";
constexpr size_t BUFFER_SIZE = 256;

class NetlinkBroadcaster {
public:
    NetlinkBroadcaster() {
        openlog("netlink_broadcaster", LOG_PID | LOG_CONS, LOG_USER);
        setupBroadcastSocket();
    }

    ~NetlinkBroadcaster() {
        if (nl_sock) {
            nl_socket_free(nl_sock);
        }
        if (broadcast_sock != -1) {
            close(broadcast_sock);
        }
        closelog();
    }

    void listen() {
        nl_sock = nl_socket_alloc();
        if (!nl_sock) {
            throw std::runtime_error("Failed to allocate netlink socket");
        }

        if (nl_connect(nl_sock, NETLINK_ROUTE) < 0) {
            throw std::system_error(errno, std::system_category(), "Failed to connect to netlink route");
        }

        nl_socket_modify_cb(nl_sock, NL_CB_MSG_IN, NL_CB_CUSTOM, &NetlinkBroadcaster::callback, this);

        if (nl_socket_add_memberships(nl_sock, RTNLGRP_IPV4_ROUTE, 0) < 0) {
            throw std::system_error(errno, std::system_category(), "Failed to join RTNLGRP_IPV4_ROUTE group");
        }

        while (true) {
            nl_recvmsgs_default(nl_sock);
        }
    }

private:
    static int callback(struct nl_msg *msg, void *arg) {
        NetlinkBroadcaster *self = static_cast<NetlinkBroadcaster *>(arg);
        struct nlmsghdr *nlh = nlmsg_hdr(msg);

        if (nlh->nlmsg_type == RTM_NEWROUTE || nlh->nlmsg_type == RTM_DELROUTE) {
            char buffer[BUFFER_SIZE] = {0};
            struct rtmsg *route_entry = static_cast<struct rtmsg *>(NLMSG_DATA(nlh));
            struct nlattr *attr = static_cast<struct nlattr *>(RTM_RTA(route_entry));
            int len = RTM_PAYLOAD(nlh);

            std::string action = (nlh->nlmsg_type == RTM_NEWROUTE) ? "New route" : "Deleted route";
            syslog(LOG_INFO, "%s detected", action.c_str());

            for (; RTA_OK(attr, len); attr = RTA_NEXT(attr, len)) {
                if (attr->nla_type == RTA_DST) {
                    inet_ntop(AF_INET, RTA_DATA(attr), buffer, sizeof(buffer));
                    syslog(LOG_INFO, "Destination: %s", buffer);
                } else if (attr->nla_type == RTA_GATEWAY) {
                    inet_ntop(AF_INET, RTA_DATA(attr), buffer, sizeof(buffer));
                    syslog(LOG_INFO, "Gateway: %s", buffer);
                } else if (attr->nla_type == RTA_PREFSRC) {
                    inet_ntop(AF_INET, RTA_DATA(attr), buffer, sizeof(buffer));
                    syslog(LOG_INFO, "Preferred source: %s", buffer);
                }
            }

            self->broadcastChange(action);
        }

        return NL_OK;
    }

    void broadcastChange(const std::string &message) {
        sendto(broadcast_sock, message.c_str(), message.size(), 0, (struct sockaddr *)&broadcast_addr, sizeof(broadcast_addr));
    }

    void setupBroadcastSocket() {
        broadcast_sock = socket(AF_UNIX, SOCK_DGRAM, 0);
        if (broadcast_sock < 0) {
            throw std::system_error(errno, std::system_category(), "Failed to create broadcast socket");
        }

        memset(&broadcast_addr, 0, sizeof(broadcast_addr));
        broadcast_addr.sun_family = AF_UNIX;
        std::strncpy(broadcast_addr.sun_path, SOCKET_PATH, sizeof(broadcast_addr.sun_path) - 1);

        unlink(SOCKET_PATH);
        if (bind(broadcast_sock, (struct sockaddr *)&broadcast_addr, sizeof(broadcast_addr)) < 0) {
            throw std::system_error(errno, std::system_category(), "Failed to bind broadcast socket");
        }
    }

    struct nl_sock *nl_sock = nullptr;
    int broadcast_sock = -1;
    struct sockaddr_un broadcast_addr;
};

int main() {
    try {
        NetlinkBroadcaster broadcaster;
        broadcaster.listen();
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }
    return 0;
}
