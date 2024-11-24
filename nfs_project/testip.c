#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

int main() {
    int sock;
    struct sockaddr_in local_address;
    socklen_t address_length = sizeof(local_address);

    // Create a socket
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1) {
        perror("Socket creation failed");
        return 1;
    }

    // Connect to an external IP address (doesn't actually send data)
    const char* google_dns = "8.8.8.8";  // Google's public DNS IP
    int port = 80;
    local_address.sin_family = AF_INET;
    local_address.sin_addr.s_addr = inet_addr(google_dns);
    local_address.sin_port = htons(port);

    if (connect(sock, (struct sockaddr*)&local_address, sizeof(local_address)) == -1) {
        perror("Connection failed");
        close(sock);
        return 1;
    }

    // Get the local IP address
    if (getsockname(sock, (struct sockaddr*)&local_address, &address_length) == -1) {
        perror("getsockname failed");
        close(sock);
        return 1;
    }

    // Convert IP to readable format and print
    char ip_address[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &local_address.sin_addr, ip_address, INET_ADDRSTRLEN);
    printf("Local IP Address: %s\n", ip_address);

    close(sock);
    return 0;
}
