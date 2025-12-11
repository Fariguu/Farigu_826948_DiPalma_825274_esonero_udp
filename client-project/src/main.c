/*
 * main.c
 *
 * UDP Client - Template for Computer Networks assignment
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include "protocol.h"

#define REQ_SIZE  (sizeof(char) + 64)
#define RESP_SIZE (sizeof(uint32_t) + sizeof(char) + sizeof(float))
#define NO_ERROR 0

#if defined WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <unistd.h>
    #include <arpa/inet.h>
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netdb.h>
    #define closesocket close
#endif

void clearwinsock() {
#if defined WIN32
    WSACleanup();
#endif
}

void errorhandler(char *error_message) {
    fprintf(stderr, "ERROR: %s\n", error_message);
    clearwinsock();
    exit(1);
}

int main(int argc, char *argv[]) {

    char *server_name_input = (char*)DEFAULT_SERVER_ADDRESS;
    int port = DEFAULT_PORT;
    char *request_arg = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
            server_name_input = argv[++i];
        } else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-r") == 0 && i + 1 < argc) {
            request_arg = argv[++i];
        }
    }

    if (request_arg == NULL) {
        printf("Uso: %s [-s server] [-p port] -r \"type city\"\n", argv[0]);
        printf("Esempio: %s -r \"t bari\"\n", argv[0]);
        return -1;
    }

    weather_request_t request;
    memset(&request, 0, sizeof(request));

    char *space_pos = strchr(request_arg, ' ');
    if (space_pos == NULL || (space_pos - request_arg) != 1) {
        printf("ERRORE DI SINTASSI: il tipo deve essere un singolo carattere seguito da spazio.\n");
        return -1;
    }

    request.type = request_arg[0];

    char *city_start = space_pos + 1;
    while (*city_start == ' ') {
        city_start++;
    }

    size_t actual_city_len = strlen(city_start);
    if (actual_city_len == 0 || actual_city_len >= sizeof(request.city)) {
        printf("ERRORE: nome città mancante o troppo lungo (max %zu).\n",
               sizeof(request.city) - 1);
        return -1;
    }

    strncpy(request.city, city_start, sizeof(request.city) - 1);
    request.city[sizeof(request.city) - 1] = '\0';

#if defined WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2,2), &wsa_data) != NO_ERROR) {
        printf("Errore in WSAStartup()\n");
        return 0;
    }
#endif

    int clientSocket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (clientSocket < 0) {
        errorhandler("Creazione socket fallita.");
    }

#ifndef WIN32
    struct timeval tv;
    tv.tv_sec  = 5;
    tv.tv_usec = 0;
    setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO,
               &tv, sizeof(tv));
#endif

    struct addrinfo hints, *server_info, *p;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    char port_str[6];
    sprintf(port_str, "%d", port);

    int rv = getaddrinfo(server_name_input, port_str, &hints, &server_info);
    if (rv != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        errorhandler("Risoluzione nome/IP server fallita.");
    }

    p = server_info;

    char server_ip_str[NI_MAXHOST];
    char server_name_canonico[NI_MAXHOST];

    getnameinfo(p->ai_addr, p->ai_addrlen,
                server_ip_str, NI_MAXHOST,
                NULL, 0,
                NI_NUMERICHOST);

    if (getnameinfo(p->ai_addr, p->ai_addrlen,
                    server_name_canonico, NI_MAXHOST,
                    NULL, 0,
                    NI_NAMEREQD) != 0) {
        strncpy(server_name_canonico, server_ip_str, NI_MAXHOST - 1);
        server_name_canonico[NI_MAXHOST - 1] = '\0';
    }

    char buffer_request[REQ_SIZE];
    int offset = 0;

    memcpy(buffer_request + offset, &request.type, sizeof(char));
    offset += sizeof(char);

    memcpy(buffer_request + offset, request.city, sizeof(request.city));
    offset += sizeof(request.city);

    if (sendto(clientSocket,
               buffer_request,
               offset,
               0,
               p->ai_addr,
               p->ai_addrlen) != offset) {
        freeaddrinfo(server_info);
        errorhandler("sendto() fallita: byte inviati != attesi.");
    }

    char buffer_response[RESP_SIZE];
    struct sockaddr_storage fromAddr;
    socklen_t fromSize = sizeof(fromAddr);

    int bytes_received = recvfrom(clientSocket,
                                  buffer_response,
                                  RESP_SIZE,
                                  0,
                                  (struct sockaddr*)&fromAddr,
                                  &fromSize);

    if (bytes_received <= 0) {
        freeaddrinfo(server_info);
        errorhandler("recvfrom() fallita o nessun datagramma ricevuto.");
    }

    weather_response_t response;
    offset = 0;

    uint32_t net_status;
    memcpy(&net_status, buffer_response + offset, sizeof(uint32_t));
    response.status = ntohl(net_status);
    offset += sizeof(uint32_t);

    memcpy(&response.type, buffer_response + offset, sizeof(char));
    offset += sizeof(char);

    uint32_t net_value;
    memcpy(&net_value, buffer_response + offset, sizeof(uint32_t));
    net_value = ntohl(net_value);
    memcpy(&response.value, &net_value, sizeof(float));

    printf("Ricevuto risultato dal server %s (ip %s). ",
           server_name_canonico, server_ip_str);

    if (response.status == STATUS_SUCCESS) {
        request.city[0] = (char)toupper((unsigned char)request.city[0]);

        switch (response.type) {
            case 't':
                printf("%s: Temperatura = %.1f°C\n", request.city, response.value);
                break;
            case 'h':
                printf("%s: Umidità = %.1f%%\n", request.city, response.value);
                break;
            case 'w':
                printf("%s: Vento = %.1f km/h\n", request.city, response.value);
                break;
            case 'p':
                printf("%s: Pressione = %.1f hPa\n", request.city, response.value);
                break;
            default:
                printf("Dati meteo ricevuti (tipo %c, valore %.1f)\n",
                       response.type, response.value);
        }
    } else if (response.status == STATUS_CITY_NOT_AVAILABLE) {
        printf("Città non disponibile\n");
    } else if (response.status == STATUS_INVALID_REQUEST) {
        printf("Richiesta non valida\n");
    } else {
        printf("Errore sconosciuto dal server (Status: %u)\n", response.status);
    }

    freeaddrinfo(server_info);
    closesocket(clientSocket);
    clearwinsock();
    return 0;
}
