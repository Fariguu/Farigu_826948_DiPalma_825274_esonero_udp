/*
 * main.c
 *
 * UDP Server - Template for Computer Networks assignment
 *
 * This file contains the boilerplate code for a UDP server
 * portable across Windows, Linux, and macOS.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include "protocol.h"

#define REQ_SIZE (sizeof(char) + 64)
#define RESP_SIZE (sizeof(unsigned int) + sizeof(char) + sizeof(float))
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

// UTILITY PER LA GESTIONE DEGLI ERRORI
void errorhandler(char *error_message) {
    printf("%s\n", error_message);
}

/* =================================================================
 * FUNZIONI DI GENERAZIONE DEI VALORI METEO SIMULATI
 * ================================================================= */

float get_temperature(void) {
    return ((float)rand() / RAND_MAX) * 30.0f - 5.0f; // range: da -5.0 a +20.0
}

float get_humidity(void) {
    return ((float)rand() / RAND_MAX) * 80.0f + 20.0f; // range: da +20.0 a +100.0
}

float get_wind(void) {
    return ((float)rand() / RAND_MAX) * 100.0f; // range: da +0.0 a +100.0
}

float get_pressure(void) {
    return ((float)rand() / RAND_MAX) * 100.0f + 950.0f; // range: +950.0 to +1050.0
}

/* =================================================================
 * LOGICA DI VALIDAZIONE DELLA RICHIESTA
 * ================================================================= */

int is_city_supported(char* city_name) { // controlla che la città è nell'elenco di quelle supportate
    const char* supported_cities[] = {
        "Bari", "Roma", "Milano", "Napoli", "Torino",
        "Palermo", "Genova", "Bologna", "Firenze", "Venezia", NULL
    };

    for (int i = 0; supported_cities[i] != NULL; i++) {
        if (strcasecmp(city_name, supported_cities[i]) == 0) {
            return 1; // trova la città
        }
    }

    return 0; // non trova la città
}

unsigned int validate_request(weather_request_t* req) { // controlla se l'intera richiesta è valida
    // controllo tipo richiesta
    char t = req->type;
    if (t != 't' && t != 'h' && t != 'w' && t != 'p') {
        return STATUS_INVALID_REQUEST; // 2
    }

    // controllo città
    if (!is_city_supported(req->city)) {
        return STATUS_CITY_NOT_AVAILABLE; // 1
    }

    return STATUS_SUCCESS; // 0
}

/* =================================================================
 * LOGICA DEL SERVER UDP
 * ================================================================= */

int main(int argc, char *argv[]) {

    srand((unsigned int)time(NULL)); // inizializza rand

#if defined WIN32
    // Initialize Winsock
    WSADATA wsa_data;
    int result = WSAStartup(MAKEWORD(2,2), &wsa_data);
    if (result != NO_ERROR) {
        printf("Error at WSAStartup()\n");
        return 0;
    }
#endif

    // CREAZIONE DELLA SOCKET UDP
    int serverSocket; // nome carino per la socket
    serverSocket = socket(PF_INET, SOCK_DGRAM, 0);
    if (serverSocket < 0) {
        errorhandler("Server socket creation failed");
        clearwinsock();
        return -1;
    }

    // CONFIGURAZIONE DELL'INDIRIZZO DEL SERVER E BIND
    struct sockaddr_in serverAddress;
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = inet_addr("127.0.0.1");
    serverAddress.sin_port = htons(DEFAULT_PORT); // porta da protocol.h

    if (bind(serverSocket, (struct sockaddr*) &serverAddress,
             sizeof(serverAddress)) < 0) {
        errorhandler("bind() failed.");
        closesocket(serverSocket);
        clearwinsock();
        return -1;
    }

    printf("Weather UDP Server started. Waiting for datagrams on port '%d'...\n",
           DEFAULT_PORT);

    // CICLO PRINCIPALE DI RICEZIONE/RISPOSTA
    while (1) {
        struct sockaddr_in clientAddress;
        socklen_t client_len = sizeof(clientAddress);

        weather_request_t  request;
        weather_response_t response;

    // 1. Definizione di un buffer per il datagramma grezzo
    char recv_buffer[REQ_SIZE]; 
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

    // Deserializzazione Manuale
    int offset = 0;

    // Deserializzazione type
    memcpy(&request.type, recv_buffer + offset, sizeof(char));
    offset += sizeof(char);

    // Deserializzazione city
    size_t city_len = (bytes_received - offset > sizeof(request.city)) ? sizeof(request.city) : bytes_received - offset;
    memcpy(request.city, recv_buffer + offset, city_len);

    // Assicura il null-terminator
    request.city[sizeof(request.city) - 1] = '\0';

        // LOGGING 
        char client_host[NI_MAXHOST];
        char client_ip[NI_MAXHOST];

        // 1. Ottieni l'Hostname
        int res_host = getnameinfo((struct sockaddr*)&clientAddress,
                                   client_len,
                                   client_host, NI_MAXHOST,
                                   NULL, 0,
                                   NI_NAMEREQD);

        if (res_host != 0) {
            // Se fallisce, usiamo l'IP sia per il nome host che per l'IP
            getnameinfo((struct sockaddr*)&clientAddress, client_len,
                         client_ip, NI_MAXHOST,
                         NULL, 0,
                         NI_NUMERICHOST);
            // Sostituiamo il nome host con l'IP (come fallback)
            strncpy(client_host, client_ip, NI_MAXHOST); 
            client_host[NI_MAXHOST - 1] = '\0';
        } else {
             // 2. Ottieni l'IP numerico
             getnameinfo((struct sockaddr*)&clientAddress, client_len,
                         client_ip, NI_MAXHOST,
                         NULL, 0,
                         NI_NUMERICHOST);
        }

        printf("Richiesta ricevuta da %s (ip %s): type='%c', city='%s'\n",
               client_host, client_ip, request.type, request.city);

        // valida la richiesta e prepara la risposta
        unsigned int status = validate_request(&request);
        response.status = status;

        if (status == STATUS_SUCCESS) {
            response.type = request.type; // echo del tipo richiesto

            // genera il valore meteo richiesto
            switch (request.type) {
                case 't':
                    response.value = get_temperature();
                    break;
                case 'h':
                    response.value = get_humidity();
                    break;
                case 'w':
                    response.value = get_wind();
                    break;
                case 'p':
                    response.value = get_pressure();
                    break;
                default:
                    response.type  = '\0';
                    response.value = 0.0f;
                    break;
            }
        } else {
            // richiesta NON valida (status = 1 o 2)
            response.type  = '\0';
            response.value = 0.0f;
        }

    // Invio della risposta

    // 1. Definisci un buffer per il datagramma da inviare
    char send_buffer[RESP_SIZE];
    offset = 0;

    // 2. Serializza status
    uint32_t net_status = htonl(response.status);
    memcpy(send_buffer + offset, &net_status, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    // 3. Serializza type
    memcpy(send_buffer + offset, &response.type, sizeof(char));
    offset += sizeof(char);

    // 4. Serializza value
    uint32_t net_value_int;

    memcpy(&net_value_int, &response.value, sizeof(float)); 

    net_value_int = htonl(net_value_int);

    memcpy(send_buffer + offset, &net_value_int, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    // 5. Invia il buffer serializzato
    if (sendto(serverSocket,
            send_buffer, // Invia il buffer, non la struct
            offset,      // Invia la dimensione esatta serializzata
            0,
            (struct sockaddr*)&clientAddress,
            client_len) < 0) {
        errorhandler("sendto() failed");
    }
    }

    // non raggiunto, ma corretto 
    printf("Server terminated.\n");
    closesocket(serverSocket);
    clearwinsock();
    return 0;
}