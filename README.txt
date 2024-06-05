Requirements:
    - Write a linux service that will listen for ipv4 routing table changes using Linux’s netlink interface. 
    - The details of the change should be logged to the journal or syslog.
    - Integrating the service with systemd so that systemctl start <netlink app> works as expected.
    - Use modern C++ best practices


Centralized Listener with Message Broadcasting
We take the approach to create a centralized netlink listener that receives routing table updates and then 
broadcasts these updates to other devices or processes. This approach reduces the number of netlink sockets 
and the load on the kernel.

Architecture
Centralized Listener: A single service listens for netlink messages.
Broadcast Mechanism: The centralized listener uses IPC (Inter-Process Communication) mechanisms, 
message queues, or publish-subscribe systems to broadcast updates to other processes.

***Notes:
In this design we do not need a semaphore for protection because the netlink listener and broadcaster model
inherently handles synchronization issues by default. However, if we plan to extend this model with more 
complexity, such as multi-threading within the netlink_broadcaster or netlink_client, or we need to protect
shared resources accessed  by multiple threads, then we might need synchronization primitives like semaphores,
mutexes, or condition variables.
==============================================================================================================
Compile and Run:

g++ -std=c++17 netlink_broadcaster.cpp -o netlink_broadcaster -lnl-3 -lnl-route-3
g++ -std=c++17 netlink_client.cpp -o netlink_client

sudo cp netlink_broadcaster /usr/local/bin/
sudo cp netlink_client /usr/local/bin/
sudo cp netlink_broadcaster.service /etc/systemd/system/

sudo systemctl daemon-reload
sudo systemctl start netlink_broadcaster
sudo systemctl enable netlink_broadcaster

/usr/local/bin/netlink_client

==============================================================================================================

