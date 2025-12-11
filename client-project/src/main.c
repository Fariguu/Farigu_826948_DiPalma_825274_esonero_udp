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

#define REQ_SIZE (sizeof(char) + 64)
#define RESP_SIZE (sizeof(uint32_t) + sizeof(char) + sizeof(float)) // 9 byte
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

    // --- GESTIONE ARGOMENTI RIGA DI COMANDO ---
    char *server_name_input = DEFAULT_SERVER_ADDRESS; // Può essere IP o nome host
    int port = DEFAULT_PORT;
    char *request_arg = NULL;
    
    // Parsing degli argomenti: -s server, -p port, -r request
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

    // 1. Validazione Formato e Lunghezza Type (Deve essere 1 carattere)
    if (space_pos == NULL || (space_pos - request_arg) != 1) {
        printf("ERRORE DI SINTASSI: Il tipo di richiesta deve essere un singolo carattere (es. t, h, w, p) seguito da uno spazio.\n");
        return -1;
    }

    // 2. Estrazione del Type
    request.type = request_arg[0];

    // 3. Estrazione della City
    char *city_start = space_pos + 1;
    while (*city_start == ' ') { 
        city_start++;
    }

    // 4. Validazione Lunghezza della City
    size_t actual_city_len = strlen(city_start);
    if (actual_city_len == 0 || actual_city_len >= sizeof(request.city)) {
        printf("ERRORE DI VALIDAZIONE: Il nome della città non è specificato o supera la lunghezza massima di %zu caratteri.\n", sizeof(request.city) - 1);
        return -1;
    }

    // 5. Copia e Null-Terminazione
    strncpy(request.city, city_start, sizeof(request.city) - 1);
    request.city[sizeof(request.city) - 1] = '\0';
    
#if defined WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2,2), &wsa_data) != NO_ERROR) {
        printf("Errore in WSAStartup()\n");
        return 0;
    }
#endif

    // CREAZIONE DELLA SOCKET
    int clientSocket;
    clientSocket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (clientSocket < 0) {
        errorhandler("Creazione socket fallita.");
    }

    struct addrinfo hints, *server_info, *p;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = SOCK_DGRAM; // UDP

    char port_str[6];
    sprintf(port_str, "%d", port);

    // Risoluzione DNS (gestisce sia hostname che IP)
    int rv = getaddrinfo(server_name_input, port_str, &hints, &server_info);
    if (rv != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        errorhandler("Risoluzione nome/IP server fallita.");
    }
    
    p = server_info; // Usa il primo risultato

    // VARIABILI PER OUTPUT: Risoluzione inversa per nome canonico/IP
    char server_ip_str[NI_MAXHOST];
    char server_name_canonico[NI_MAXHOST];
    
    // Ottieni IP numerico per l'output
    getnameinfo(p->ai_addr, p->ai_addrlen,
                server_ip_str, NI_MAXHOST,
                NULL, 0,
                NI_NUMERICHOST);

    // Ottieni nome canonico per l'output (se fallisce, usa l'IP come nome)
    if (getnameinfo(p->ai_addr, p->ai_addrlen,
                    server_name_canonico, NI_MAXHOST,
                    NULL, 0,
                    NI_NAMEREQD) != 0) {
        strncpy(server_name_canonico, server_ip_str, NI_MAXHOST - 1);
        server_name_canonico[NI_MAXHOST - 1] = '\0';
    }


    // SERIALIZZAZIONE MANUALE E INVIO DELLA RICHIESTA

    char buffer_request[REQ_SIZE];
    int offset = 0;

    // Serializza type (1 byte)
    memcpy(buffer_request + offset, &request.type, sizeof(char));
    offset += sizeof(char);

    // Serializza city (64 byte)
    // Invia 64 byte, inclusa la terminazione null, come previsto dalla serializzazione
    memcpy(buffer_request + offset, request.city, sizeof(request.city));
    offset += sizeof(request.city); // offset = 65

    // Invio della richiesta al server
    if (sendto(clientSocket, buffer_request, offset, 0,
               p->ai_addr, p->ai_addrlen) != offset) { 
        freeaddrinfo(server_info);
        errorhandler("sendto() fallita: inviato numero di byte diverso da quello atteso.");
    }

    // RICEZIONE DELLA RISPOSTA DAL SERVER (recvfrom)
    char buffer_response[RESP_SIZE];
    struct sockaddr_storage fromAddr;
    socklen_t fromSize = sizeof(fromAddr);

    int bytes_received = recvfrom(clientSocket, buffer_response, RESP_SIZE, 0,
                                  (struct sockaddr*)&fromAddr, &fromSize);

    if (bytes_received <= 0) {
        freeaddrinfo(server_info);
        // Questo errore indica che il Server non ha risposto o è crashato!
        errorhandler("recvfrom() fallita o nessun datagramma ricevuto.");
    }

    // Deserializzazione e Conversione NBO della Risposta
    weather_response_t response;
    offset = 0;

    // 1. Deserializza status (NBO -> Host)
    uint32_t net_status;
    memcpy(&net_status, buffer_response + offset, sizeof(uint32_t));
    response.status = ntohl(net_status); 
    offset += sizeof(uint32_t);

    // 2. Deserializza type (1 byte)
    memcpy(&response.type, buffer_response + offset, sizeof(char));
    offset += sizeof(char);

    // 3. Deserializza value (uint32_t NBO -> float Host)
    uint32_t net_value;
    memcpy(&net_value, buffer_response + offset, sizeof(uint32_t));
    net_value = ntohl(net_value); 
    memcpy(&response.value, &net_value, sizeof(float)); 
    
    // Stampa iniziale
    printf("Ricevuto risultato dal server %s (ip %s). ", server_name_canonico, server_ip_str);

    // Visualizzazione della risposta
    if (response.status == STATUS_SUCCESS) {
        
        // Correzione Capitalizzazione (solo la prima lettera)
        request.city[0] = toupper(request.city[0]);

        switch(response.type) {
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
                printf("Dati meteo ricevuti (Tipo non riconosciuto: %c, Valore: %.1f)\n", response.type, response.value);
        }

    } else if (response.status == STATUS_CITY_NOT_AVAILABLE) {
        printf("Città non disponibile\n");
    } else if (response.status == STATUS_INVALID_REQUEST) {
        printf("Richiesta non valida\n");
    } else {
        printf("Errore sconosciuto dal server (Status: %u)\n", response.status);
    }

    // CHIUSURA
    freeaddrinfo(server_info);
    closesocket(clientSocket);
    clearwinsock();
    return 0;
}