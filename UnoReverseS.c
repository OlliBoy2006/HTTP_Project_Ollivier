#ifdef _WIN32

#define _WIN32_WINNT _WIN32_WINNT_WIN7

#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void OSInit(void) {}
void OSCleanup(void) {}

#define SD_RECEIVE SHUT_RD

#endif

#define SERVER_PORT "22"
#define LOG_FILE_NAME "UnoReverseLog.txt"
#define BUFFER_SIZE 1000
#define GEO_BUFFER_SIZE 6000
#define GEO_HOST "ip-api.com"
#define GEO_PORT "80"
#define RESPONSE_SIZE (1024 * 1024)

int initialization(void);

int connection(
    int internet_socket,
    char *client_ip,
    size_t client_ip_size,
    char *client_port,
    size_t client_port_size
);

void execution(
    int client_internet_socket,
    char *client_ip,
    char *client_port
);

void cleanup(
    int internet_socket,
    int client_internet_socket
);

void get_current_time_string(
    char *time_buffer,
    size_t time_buffer_size
);

void get_geolocation(
    char *ip_address,
    char *geolocation_buffer,
    size_t geolocation_buffer_size
);

int send_test_data(
    int client_internet_socket,
    int *bytes_sent
);

void write_log(
    char *client_ip,
    char *client_port,
    char *received_data,
    int bytes_received,
    int bytes_sent,
    char *geolocation_data
);

int main(int argc, char *argv[])
{
    OSInit();

    int internet_socket = initialization();

    printf("UnoReverse server started on port %s\n", SERVER_PORT);
    printf("Logging to file: %s\n", LOG_FILE_NAME);

    while (1)
    {
        char client_ip[NI_MAXHOST];
        char client_port[NI_MAXSERV];

        int client_internet_socket = connection(
            internet_socket,
            client_ip,
            sizeof(client_ip),
            client_port,
            sizeof(client_port)
        );

        execution(
            client_internet_socket,
            client_ip,
            client_port
        );

        cleanup(
            internet_socket,
            client_internet_socket
        );
    }

    OSCleanup();

    return 0;
}

int initialization(void)
{
    struct addrinfo internet_address_setup;
    struct addrinfo *internet_address_result;

    memset(
        &internet_address_setup,
        0,
        sizeof(internet_address_setup)
    );

    internet_address_setup.ai_family = AF_UNSPEC;
    internet_address_setup.ai_socktype = SOCK_STREAM;
    internet_address_setup.ai_flags = AI_PASSIVE;

    int getaddrinfo_return = getaddrinfo(
        NULL,
        SERVER_PORT,
        &internet_address_setup,
        &internet_address_result
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

    struct addrinfo *iterator = internet_address_result;

    while (iterator != NULL)
    {
        internet_socket = socket(
            iterator->ai_family,
            iterator->ai_socktype,
            iterator->ai_protocol
        );

        if (internet_socket == -1)
        {
            perror("socket");
        }
        else
        {
            int bind_return = bind(
                internet_socket,
                iterator->ai_addr,
                iterator->ai_addrlen
            );

            if (bind_return == -1)
            {
                perror("bind");
                close(internet_socket);
                internet_socket = -1;
            }
            else
            {
                int listen_return = listen(
                    internet_socket,
                    10
                );

                if (listen_return == -1)
                {
                    perror("listen");
                    close(internet_socket);
                    internet_socket = -1;
                }
                else
                {
                    break;
                }
            }
        }

        iterator = iterator->ai_next;
    }

    freeaddrinfo(internet_address_result);

    if (internet_socket == -1)
    {
        fprintf(
            stderr,
            "Could not create a valid server socket\n"
        );

        exit(EXIT_FAILURE);
    }

    return internet_socket;
}

int connection(
    int internet_socket,
    char *client_ip,
    size_t client_ip_size,
    char *client_port,
    size_t client_port_size
)
{
    struct sockaddr_storage client_address;

    socklen_t client_address_length =
        sizeof(client_address);

    int client_socket = accept(
        internet_socket,
        (struct sockaddr *)&client_address,
        &client_address_length
    );

    if (client_socket == -1)
    {
        perror("accept");
        return -1;
    }

    int getnameinfo_return = getnameinfo(
        (struct sockaddr *)&client_address,
        client_address_length,
        client_ip,
        (socklen_t)client_ip_size,
        client_port,
        (socklen_t)client_port_size,
        NI_NUMERICHOST | NI_NUMERICSERV
    );

    if (getnameinfo_return != 0)
    {
        fprintf(
            stderr,
            "getnameinfo: %s\n",
            gai_strerror(getnameinfo_return)
        );

        snprintf(
            client_ip,
            client_ip_size,
            "unknown"
        );

        snprintf(
            client_port,
            client_port_size,
            "unknown"
        );
    }

    printf(
        "New connection from %s:%s\n",
        client_ip,
        client_port
    );

    return client_socket;
}

void execution(
    int client_internet_socket,
    char *client_ip,
    char *client_port
)
{
    char buffer[BUFFER_SIZE];

    memset(
        buffer,
        0,
        sizeof(buffer)
    );

    int bytes_received = recv(
        client_internet_socket,
        buffer,
        sizeof(buffer) - 1,
        0
    );

    if (bytes_received == -1)
    {
        perror("recv");

        strcpy(
            buffer,
            "[recv failed]"
        );

        bytes_received = 0;
    }
    else if (bytes_received == 0)
    {
        strcpy(
            buffer,
            "[client disconnected]"
        );
    }
    else
    {
        buffer[bytes_received] = '\0';

        printf(
            "Received from %s:%s : %s\n",
            client_ip,
            client_port,
            buffer
        );
    }

    char geolocation_data[GEO_BUFFER_SIZE];

    memset(
        geolocation_data,
        0,
        sizeof(geolocation_data)
    );

    get_geolocation(
        client_ip,
        geolocation_data,
        sizeof(geolocation_data)
    );

    int bytes_sent = 0;

    int send_return = send_test_data(
        client_internet_socket,
        &bytes_sent
    );

    if (send_return == -1)
    {
        perror("send");
    }

    printf(
        "Test data sent to %s:%s: %d bytes\n",
        client_ip,
        client_port,
        bytes_sent
    );

    write_log(
        client_ip,
        client_port,
        buffer,
        bytes_received,
        bytes_sent,
        geolocation_data
    );
}

int send_test_data(
    int client_internet_socket,
    int *bytes_sent
)
{
    char *data = malloc(RESPONSE_SIZE);

    if (data == NULL)
    {
        fprintf(
            stderr,
            "Could not allocate response buffer\n"
        );

        return -1;
    }

    memset(
        data,
        'U',
        RESPONSE_SIZE
    );

    int total_sent = 0;

    while (total_sent < RESPONSE_SIZE)
    {
        int remaining =
            RESPONSE_SIZE - total_sent;

        int sent = send(
            client_internet_socket,
            data + total_sent,
            remaining,
            0
        );

        if (sent == -1)
        {
            free(data);
            return -1;
        }

        if (sent == 0)
        {
            break;
        }

        total_sent += sent;
    }

    free(data);

    *bytes_sent = total_sent;

    return 0;
}

void cleanup(
    int internet_socket,
    int client_internet_socket
)
{
    int shutdown_return = shutdown(
        client_internet_socket,
        SD_RECEIVE
    );

    if (shutdown_return == -1)
    {
        perror("shutdown");
    }

    close(client_internet_socket);
}

void get_current_time_string(
    char *time_buffer,
    size_t time_buffer_size
)
{
    time_t current_time = time(NULL);

    struct tm *local_time =
        localtime(&current_time);

    if (local_time == NULL)
    {
        snprintf(
            time_buffer,
            time_buffer_size,
            "unknown time"
        );

        return;
    }

    strftime(
        time_buffer,
        time_buffer_size,
        "%Y-%m-%d %H:%M:%S",
        local_time
    );
}

void get_geolocation(
    char *ip_address,
    char *geolocation_buffer,
    size_t geolocation_buffer_size
)
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
        GEO_HOST,
        GEO_PORT,
        &address_setup,
        &address_result
    );

    if (getaddrinfo_return != 0)
    {
        snprintf(
            geolocation_buffer,
            geolocation_buffer_size,
            "Geolocation error: %s",
            gai_strerror(getaddrinfo_return)
        );

        return;
    }

    int geo_socket = -1;

    struct addrinfo *iterator = address_result;

    while (iterator != NULL)
    {
        geo_socket = socket(
            iterator->ai_family,
            iterator->ai_socktype,
            iterator->ai_protocol
        );

        if (geo_socket != -1)
        {
            int connect_return = connect(
                geo_socket,
                iterator->ai_addr,
                iterator->ai_addrlen
            );

            if (connect_return == 0)
            {
                break;
            }

            close(geo_socket);
            geo_socket = -1;
        }

        iterator = iterator->ai_next;
    }

    freeaddrinfo(address_result);

    if (geo_socket == -1)
    {
        snprintf(
            geolocation_buffer,
            geolocation_buffer_size,
            "Geolocation error: connection failed"
        );

        return;
    }

    char http_request[1000];

    snprintf(
        http_request,
        sizeof(http_request),
        "GET /json/%s?fields=status,message,query,country,regionName,city,zip,lat,lon,timezone,isp,org,as,proxy,hosting HTTP/1.1\r\n"
        "Host: ip-api.com\r\n"
        "User-Agent: UnoReverse/1.0\r\n"
        "Connection: close\r\n"
        "\r\n",
        ip_address
    );

    int request_length =
        (int)strlen(http_request);

    int request_sent = send(
        geo_socket,
        http_request,
        request_length,
        0
    );

    if (request_sent == -1)
    {
        perror("send");

        snprintf(
            geolocation_buffer,
            geolocation_buffer_size,
            "Geolocation error: HTTP request failed"
        );

        close(geo_socket);

        return;
    }

    int total_received = 0;

    while (total_received <
           (int)geolocation_buffer_size - 1)
    {
        int received = recv(
            geo_socket,
            geolocation_buffer + total_received,
            (int)geolocation_buffer_size -
            total_received - 1,
            0
        );

        if (received == -1)
        {
            perror("recv");
            break;
        }

        if (received == 0)
        {
            break;
        }

        total_received += received;
    }

    geolocation_buffer[total_received] = '\0';

    close(geo_socket);

    char *json_body =
        strstr(
            geolocation_buffer,
            "\r\n\r\n"
        );

    if (json_body != NULL)
    {
        json_body += 4;

        memmove(
            geolocation_buffer,
            json_body,
            strlen(json_body) + 1
        );
    }

    if (strlen(geolocation_buffer) == 0)
    {
        snprintf(
            geolocation_buffer,
            geolocation_buffer_size,
            "Geolocation error: empty response"
        );
    }
}

void write_log(
    char *client_ip,
    char *client_port,
    char *received_data,
    int bytes_received,
    int bytes_sent,
    char *geolocation_data
)
{
    FILE *log_file =
        fopen(LOG_FILE_NAME, "a");

    if (log_file == NULL)
    {
        perror("fopen");
        return;
    }

    char time_buffer[100];

    get_current_time_string(
        time_buffer,
        sizeof(time_buffer)
    );

    fprintf(
        log_file,
        "============================================================\n"
    );

    fprintf(
        log_file,
        "UnoReverse login attempt\n"
    );

    fprintf(
        log_file,
        "Time              : %s\n",
        time_buffer
    );

    fprintf(
        log_file,
        "Remote IP         : %s\n",
        client_ip
    );

    fprintf(
        log_file,
        "Remote port       : %s\n",
        client_port
    );

    fprintf(
        log_file,
        "Local listen port : %s\n",
        SERVER_PORT
    );

    fprintf(
        log_file,
        "Protocol          : TCP\n"
    );

    fprintf(
        log_file,
        "Bytes received    : %d\n",
        bytes_received
    );

    fprintf(
        log_file,
        "Bytes sent        : %d\n",
        bytes_sent
    );

    fprintf(
        log_file,
        "Response type     : bounded test data\n"
    );

    fprintf(
        log_file,
        "\nReceived data:\n%s\n",
        received_data
    );

    fprintf(
        log_file,
        "\nGeo-location data from ip-api.com:\n%s\n",
        geolocation_data
    );

    fprintf(
        log_file,
        "============================================================\n\n"
    );

    fclose(log_file);
}
