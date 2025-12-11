/*
 * main.c
 *
 * UDP Server - Template for Computer Networks assignment
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <ctype.h>
#include "protocol.h"

#define REQ_SIZE  (sizeof(char) + 64)                       // 1 + 64
#define RESP_SIZE (sizeof(uint32_t) + sizeof(char) + sizeof(float)) // 4+1+4
#define NO_ERROR 0

#if defined WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    #define strcasecmp stricmp
#else
    #include <unistd.h>
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <arpa/inet.h>
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
    fprintf(stderr, "SERVER ERROR: %s\n", error_message);
}

/* =================================================================
 * FUNZIONI DI GENERAZIONE DEI VALORI METEO SIMULATI
 * ================================================================= */

float get_temperature(void) {
    return ((float)rand() / RAND_MAX) * 50.0f - 10.0f; // -10 .. +40
}

float get_humidity(void) {
    return ((float)rand() / RAND_MAX) * 80.0f + 20.0f; // 20 .. 100
}

float get_wind(void) {
    return ((float)rand() / RAND_MAX) * 100.0f;        // 0 .. 100
}

float get_pressure(void) {
    return ((float)rand() / RAND_MAX) * 100.0f + 950.0f; // 950 .. 1050
}

/* =================================================================
 * LOGICA DI VALIDAZIONE DELLA RICHIESTA
 * ================================================================= */

int is_city_supported(char* city_name) {
    char clean_city[64];
    strncpy(clean_city, city_name, 63);
    clean_city[63] = '\0';

    for (int i = 0; i < 64; i++) {
        if (clean_city[i] == '\0' || isspace((unsigned char)clean_city[i])) {
            clean_city[i] = '\0';
            break;
        }
    }

    const char* supported_cities[] = {
        "Bari", "Roma", "Milano", "Napoli", "Torino",
        "Palermo", "Genova", "Bologna", "Firenze", "Venezia", NULL
    };

    for (int i = 0; supported_cities[i] != NULL; i++) {
        if (strcasecmp(clean_city, supported_cities[i]) == 0) {
            return 1;
        }
    }

    return 0;
}

unsigned int validate_request(weather_request_t* req) {
    char t = req->type;
    if (t != 't' && t != 'h' && t != 'w' && t != 'p') {
        return STATUS_INVALID_REQUEST;
    }

    if (!is_city_supported(req->city)) {
        return STATUS_CITY_NOT_AVAILABLE;
    }

    return STATUS_SUCCESS;
}

/* =================================================================
 * LOGICA DEL SERVER UDP
 * ================================================================= */

int main(int argc, char *argv[]) {
    int port = DEFAULT_PORT;
    if (argc == 3 && strcmp(argv[1], "-p") == 0) {
        port = atoi(argv[2]);
    }

    srand((unsigned int)time(NULL));

#if defined WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2,2), &wsa_data) != NO_ERROR) {
        printf("Error at WSAStartup()\n");
        return 0;
    }
#endif

    int serverSocket = socket(PF_INET, SOCK_DGRAM, 0);
    if (serverSocket < 0) {
        errorhandler("Server socket creation failed");
        clearwinsock();
        return -1;
    }

    struct sockaddr_in serverAddress;
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family      = AF_INET;
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddress.sin_port        = htons(port);

    if (bind(serverSocket,
             (struct sockaddr*)&serverAddress,
             sizeof(serverAddress)) < 0) {
        errorhandler("bind() failed.");
        closesocket(serverSocket);
        clearwinsock();
        return -1;
    }

    printf("Weather UDP Server started. Waiting for datagrams on port '%d'...\n",
           port);

    while (1) {
        struct sockaddr_in clientAddress;
        socklen_t client_len = sizeof(clientAddress);
        weather_request_t  request;
        weather_response_t response;
        char recv_buffer[REQ_SIZE];

        memset(&request, 0, sizeof(request));

        int bytes_received = recvfrom(serverSocket,
                                      recv_buffer,
                                      REQ_SIZE,
                                      0,
                                      (struct sockaddr*)&clientAddress,
                                      &client_len);
        if (bytes_received <= 0) {
            errorhandler("recvfrom() failed or empty datagram");
            continue;
        }

        int offset = 0;
        memcpy(&request.type, recv_buffer + offset, sizeof(char));
        offset += sizeof(char);

        size_t city_copy_size = bytes_received - offset;
        if (city_copy_size > sizeof(request.city)) {
            city_copy_size = sizeof(request.city);
        }
        memcpy(request.city, recv_buffer + offset, city_copy_size);
        request.city[city_copy_size < sizeof(request.city)
                     ? city_copy_size
                     : sizeof(request.city) - 1] = '\0';

        char client_host[NI_MAXHOST] = "N/A";
        char client_ip[NI_MAXHOST]   = "N/A";

        getnameinfo((struct sockaddr*)&clientAddress, client_len,
                    client_ip, NI_MAXHOST,
                    NULL, 0,
                    NI_NUMERICHOST);

        if (getnameinfo((struct sockaddr*)&clientAddress, client_len,
                        client_host, NI_MAXHOST,
                        NULL, 0,
                        NI_NAMEREQD) != 0) {
            strncpy(client_host, client_ip, NI_MAXHOST - 1);
            client_host[NI_MAXHOST - 1] = '\0';
        }

        printf("Richiesta ricevuta da %s (ip %s): type='%c', city='%s'\n",
               client_host, client_ip, request.type, request.city);

        unsigned int status = validate_request(&request);
        response.status = status;

        if (status == STATUS_SUCCESS) {
            response.type = request.type;
            switch (request.type) {
                case 't': response.value = get_temperature(); break;
                case 'h': response.value = get_humidity();    break;
                case 'w': response.value = get_wind();        break;
                case 'p': response.value = get_pressure();    break;
                default:
                    response.type  = '\0';
                    response.value = 0.0f;
                    break;
            }
        } else {
            response.type  = '\0';
            response.value = 0.0f;
        }

        char send_buffer[RESP_SIZE];
        offset = 0;

        uint32_t net_status = htonl(response.status);
        memcpy(send_buffer + offset, &net_status, sizeof(uint32_t));
        offset += sizeof(uint32_t);

        memcpy(send_buffer + offset, &response.type, sizeof(char));
        offset += sizeof(char);

        uint32_t net_value_int;
        memcpy(&net_value_int, &response.value, sizeof(float));
        net_value_int = htonl(net_value_int);
        memcpy(send_buffer + offset, &net_value_int, sizeof(uint32_t));
        offset += sizeof(uint32_t);

        if (sendto(serverSocket,
                   send_buffer,
                   offset,
                   0,
                   (struct sockaddr*)&clientAddress,
                   client_len) < 0) {
            errorhandler("sendto() failed");
        }
    }

    printf("Server terminated.\n");
    closesocket(serverSocket);
    clearwinsock();
    return 0;
}
