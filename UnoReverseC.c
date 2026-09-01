#ifdef _WIN32

#define _WIN32_WINNT _WIN32_WINNT_WIN7

#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void OSInit(void)
{
    WSADATA wsaData;

    if (WSAStartup(MAKEWORD(2, 0), &wsaData) != 0)
    {
        fprintf(stderr, "WSAStartup failed\n");
        exit(EXIT_FAILURE);
    }
}

void OSCleanup(void)
{
    WSACleanup();
}

#define close closesocket
#define perror(string) fprintf(stderr, string ": WSA errno = %d\n", WSAGetLastError())

#else

#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void OSInit(void) {}
void OSCleanup(void) {}

#define SD_SEND SHUT_WR

#endif

#define SERVER_ADDRESS "::1"
#define SERVER_PORT "22"
#define BUFFER_SIZE 4096

int initialization(void);
void execution(int internet_socket);
void cleanup(int internet_socket);

int main(int argc, char *argv[])
{
    OSInit();

    int internet_socket = initialization();

    execution(internet_socket);

    cleanup(internet_socket);

    OSCleanup();

    return 0;
}

int initialization(void)
{
    struct addrinfo address_setup;
    struct addrinfo *address_result;

    memset(
        &address_setup,
        0,
        sizeof(address_setup)
    );

    address_setup.ai_family = AF_UNSPEC;
    address_setup.ai_socktype = SOCK_STREAM;

    int getaddrinfo_return = getaddrinfo(
        SERVER_ADDRESS,
        SERVER_PORT,
        &address_setup,
        &address_result
    );

    if (getaddrinfo_return != 0)
    {
        fprintf(
            stderr,
            "getaddrinfo: %s\n",
            gai_strerror(getaddrinfo_return)
        );

        exit(EXIT_FAILURE);
    }

    int internet_socket = -1;

    struct addrinfo *iterator = address_result;

    while (iterator != NULL)
    {
        internet_socket = socket(
            iterator->ai_family,
            iterator->ai_socktype,
            iterator->ai_protocol
        );

        if (internet_socket != -1)
        {
            int connect_return = connect(
                internet_socket,
                iterator->ai_addr,
                iterator->ai_addrlen
            );

            if (connect_return == 0)
            {
                break;
            }

            perror("connect");

            close(internet_socket);

            internet_socket = -1;
        }

        iterator = iterator->ai_next;
    }

    freeaddrinfo(address_result);

    if (internet_socket == -1)
    {
        fprintf(
            stderr,
            "Could not connect to UnoReverse server\n"
        );

        exit(EXIT_FAILURE);
    }

    return internet_socket;
}

void execution(int internet_socket)
{
    char login_attempt[] =
        "SSH-2.0-UnoReverseTestClient\r\n"
        "username=admin; password=admin123\r\n";

    int bytes_sent = send(
        internet_socket,
        login_attempt,
        (int)strlen(login_attempt),
        0
    );

    if (bytes_sent == -1)
    {
        perror("send");
        return;
    }

    printf(
        "Login attempt sent (%d bytes)\n",
        bytes_sent
    );

    int total_received = 0;

    char buffer[BUFFER_SIZE];

    while (1)
    {
        int bytes_received = recv(
            internet_socket,
            buffer,
            sizeof(buffer),
            0
        );

        if (bytes_received == -1)
        {
            perror("recv");
            break;
        }

        if (bytes_received == 0)
        {
            break;
        }

        total_received += bytes_received;

        printf(
            "\rReceived: %d bytes",
            total_received
        );

        fflush(stdout);
    }

    printf("\n");

    printf(
        "Total test data received: %d bytes\n",
        total_received
    );
}

void cleanup(int internet_socket)
{
    int shutdown_return = shutdown(
        internet_socket,
        SD_SEND
    );

    if (shutdown_return == -1)
    {
        perror("shutdown");
    }

    close(internet_socket);
}
