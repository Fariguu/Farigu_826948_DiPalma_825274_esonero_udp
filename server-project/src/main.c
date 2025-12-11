/*
 * main.c
 *
 * TCP Server - Template for Computer Networks assignment
 *
 * This file contains the boilerplate code for a TCP server
 * portable across Windows, Linux and macOS.
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include "protocol.h"

#define NO_ERROR 0

#if defined WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #define strcasecmp strcmp
#else
    #include <string.h>
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

// FUNZIONI DI GENERAZIONE DEI VALORI METEO SIMULATI
float get_temperature(void) {
    return ((float)rand() / RAND_MAX) * 50.0 - 10.0;
}

float get_humidity(void) {
    return ((float)rand() / RAND_MAX) * 80.0 + 20.0;
}

float get_wind(void) {
    return ((float)rand() / RAND_MAX) * 100.0;
}

float get_pressure(void) {
    return ((float)rand() / RAND_MAX) * 100.0 + 950.0;
}

// LOGICA DI VALIDAZIONE DELLA RICHIESTA
int is_city_supported(char* city_name) {        // controlla che la città è nell'elenco di quelle supportate
    const char* supported_cities[] = {
        "Bari", "Roma", "Milano", "Napoli", "Torino", 
        "Palermo", "Genova", "Bologna", "Firenze", "Venezia", NULL
    };

    for (int i = 0; supported_cities[i] != NULL; i++) {
        if (strcasecmp(city_name, supported_cities[i]) == 0) {
            return 1;   // trova la città
        }
    }
    return 0;   // non trova la città
}

unsigned int validate_request(weather_request_t* req) {     // controlla se l'intera richiesta è valida
    // controllo tipo richiesta
    char t = req->type;
    if (t != 't' && t != 'h' && t != 'w' && t != 'p') {
        return 2;   // richiesta non valida
    }

    // Controllo città
    if (!is_city_supported(req->city)) {
        return 1;   // città non disponibile
    }

    return 0;   // successo
}

// LOGICA DEL SERVER
void handleclientconnection(int client_socket, struct sockaddr_in client_addr) {
    
    weather_request_t request;
    weather_response_t response;
    int bytes_received;

    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);

    // 1. riceve la richiesta dal client
    bytes_received = recv(client_socket, (char*)&request, sizeof(weather_request_t), 0);
    
    if (bytes_received <= 0) {
        errorhandler("recv() fallita o connessione chiusa dal client"); // Errore tradotto (opzionale, ma meglio)
        closesocket(client_socket);
        return;
    }
    
    // assicura che la stringa 'city' sia null-terminated
    request.city[sizeof(request.city) - 1] = '\0';

    // 2. visualizza il messaggio di richiesta (IN ITALIANO COME DA SPECIFICHE)
    printf("Richiesta '%c %s' dal client ip %s\n", 
           request.type, request.city, client_ip);

    // 3. valida la richiesta
    unsigned int status = validate_request(&request);
    response.status = status;

    // 4. se la richiesta è valida (status = 0 / STATUS_SUCCESS)
    if (status == STATUS_SUCCESS) {
        response.type = request.type;

        // genera il valore meteo richiesto
        switch (request.type) {
            case 't': response.value = get_temperature(); break;
            case 'h': response.value = get_humidity(); break;
            case 'w': response.value = get_wind(); break;
            case 'p': response.value = get_pressure(); break;
        }
    } 
    // 5. se la richiesta NON è valida
    else {
        response.type = '\0';
        response.value = 0.0; 
    }

    // 6. invia la risposta
    if (send(client_socket, (char*)&response, sizeof(weather_response_t), 0) < 0) {
        errorhandler("send() fallita");
    }

    // 7. chiude la connessione con il client
    closesocket(client_socket);
}

int main(int argc, char *argv[]) {

    srand(time(NULL)); 

    // GESTIONE ARGOMENTI DA RIGA DI COMANDO
    int port = DEFAULT_PORT;
    // Scorre gli argomenti per cercare il flag -p
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            port = atoi(argv[i + 1]);
            i++; // Salta il prossimo argomento (il numero della porta)
        }
    }

#if defined WIN32
    // Initialize Winsock
    WSADATA wsa_data;
    int result = WSAStartup(MAKEWORD(2,2), &wsa_data);
    if (result != NO_ERROR) {
        printf("Errore in WSAStartup()\n");
        return 0;
    }
#endif

    // CREAZIONE DELLA SOCKET
    int serverSocket; 
    serverSocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket < 0) {
        errorhandler("Creazione socket server fallita");
        clearwinsock();
        return -1;
    }

    // Configuro l'indirizzo del server
    struct sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = inet_addr("127.0.0.1");
    serverAddress.sin_port = htons(port); // USA LA VARIABILE port, NON LA COSTANTE

    if (bind(serverSocket, (struct sockaddr*) &serverAddress, sizeof(serverAddress)) < 0) {
        errorhandler("bind() fallita.\n");
        closesocket(serverSocket);
        clearwinsock();
        return -1;
    }

    // SETTAGGIO DELLA SOCKET ALL'ASCOLTO
    int qlen = 6;
    if (listen (serverSocket, qlen) < 0) {
        errorhandler("listen() fallita.\n");
        closesocket(serverSocket);
        return -1;
    }

    // ACCETTAZIONE E GESTIONE DI UNA NUOVA CONNESSIONE
    struct sockaddr_in clientAddress; 
    int client_socket; 
    int client_len; 
    
    printf("Server Meteo avviato. In ascolto sulla porta %d...\n", port);

    while (1) {
        client_len = sizeof(clientAddress); 
        if ( (client_socket = accept(serverSocket, (struct sockaddr *)&clientAddress, &client_len)) < 0 ) {
            errorhandler("accept() fallita.\n");
            continue;
        }

        handleclientconnection(client_socket, clientAddress);
    }

    printf("Server terminato.\n");

    closesocket(serverSocket);
    clearwinsock();
    return 0;
}