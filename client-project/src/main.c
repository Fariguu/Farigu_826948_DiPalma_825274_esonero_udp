/*
 * main.c
 *
 * TCP Client - Template for Computer Networks assignment
 *
 * This file contains the boilerplate code for a TCP client
 * portable across Windows, Linux and macOS.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h> // Necessario per strncpy e strchr
#include <ctype.h> // Necessario per la funzione toupper()
#include "protocol.h"

#define NO_ERROR 0

#if defined WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
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
    printf("ERROR: %s\n", error_message);
    clearwinsock();
    exit(1); // Termina il programma
}

int main(int argc, char *argv[]) {

    // --- 1. GESTIONE ARGOMENTI RIGA DI COMANDO ---
    // Valori di default
    char *server_ip = DEFAULT_SERVER_ADDRESS;
    int port = DEFAULT_PORT;
    char *request_arg = NULL; // Conterrà la stringa
    
    // Variabili per DNS Lookup e output
    char server_ip_str[16]; // Stringa per l'indirizzo IP numerico finale (es. "127.0.0.1")
    char *server_name_canonico = NULL; // Nome canonico per l'output (es. "localhost")
    struct hostent *host_info; // Struttura per i risultati DNS

    // Parsing degli argomenti: -s server, -p port, -r request
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
            server_ip = argv[++i];
        } else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-r") == 0 && i + 1 < argc) {
            request_arg = argv[++i];
        }
    }

    // Se manca la richiesta obbligatoria (-r), mostra l'uso ed esce
    if (request_arg == NULL) {
        printf("Uso: %s [-s server] [-p port] -r \"type city\"\n", argv[0]);
        printf("Esempio: %s -r \"t bari\"\n", argv[0]);
        return -1;
    }

    // Parsing della stringa richiesta
    weather_request_t request;
    memset(&request, 0, sizeof(request));
    
    // sscanf legge un carattere e una stringa ignorando gli spazi iniziali
    if (sscanf(request_arg, " %c %63s", &request.type, request.city) != 2) {
        printf("Formato richiesta non valido. Formato atteso: \"type city\"\n");
        return -1;
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
 
    int clientSocket;
    clientSocket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (clientSocket < 0) {
        errorhandler("Creazione socket fallita.");
    }

    // CONFIGURAZIONE E RISOLUZIONE DNS DEL SERVER
    struct sockaddr_in serverAddress;
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);

    // Tenta di convertire l'input 'server_ip' in indirizzo IP numerico.
    // Se fallisce, 'server_ip' è un nome simbolico (hostname).

    if ((serverAddress.sin_addr.s_addr = inet_addr(server_ip)) == INADDR_NONE) {

        // Caso 1: L'input è un NOME SIMBOLICO
        // gethostbyname può essere chiamata solo con AF_INET 
        host_info = gethostbyname(server_ip);
        
        if (host_info == NULL) {
            errorhandler("Risoluzione nome server fallita (gethostbyname).");
        }
        
        memcpy(&serverAddress.sin_addr, host_info->h_addr_list[0], host_info->h_length); // Copia l'indirizzo risolto
        
        server_name_canonico = host_info->h_name; // Nome canonico dal DNS
        
    } else {

        // Caso 2: L'input è un INDIRIZZO IP NUMERICO
        // L'indirizzo è già in serverAddress.sin_addr
        struct in_addr addr;
        addr.s_addr = serverAddress.sin_addr.s_addr;
        
        host_info = gethostbyaddr((char *) &addr, sizeof(addr), AF_INET); // Reverse lookup
        
        if (host_info == NULL) {
            server_name_canonico = server_ip; // Usa l'IP come nome
        } else {
            server_name_canonico = host_info->h_name; // Nome canonico dal DNS
        }
    }

    // Salva la stringa IP finale (per l'output)
    strncpy(server_ip_str, inet_ntoa(serverAddress.sin_addr), 15);
    server_ip_str[15] = '\0'; // Assicura la terminazione

    // CONFIGURAZIONE DELL'INDIRIZZO DEL SERVER
    struct sockaddr_in serverAddress;

    memset(&serverAddress, 0, sizeof(serverAddress)); // Pulisce la struttura
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = inet_addr(server_ip);
    serverAddress.sin_port = htons(port);

    // LOGICA DI COMUNICAZIONE CON IL SERVER (REQUEST/RESPONSE)

    // Invio della richiesta al server
    if (send(clientSocket, (char*)&request, sizeof(weather_request_t), 0) < 0) {
        errorhandler("send() fallita.");
    }

    // Ricezione della risposta dal server
    weather_response_t response;
    int bytes_received = recv(clientSocket, (char*)&response, sizeof(weather_response_t), 0);
    
    if (bytes_received <= 0) {
        errorhandler("recv() fallita o connessione chiusa.");
        closesocket(clientSocket);
    }
    
    printf("Ricevuto risultato dal server ip %s. ", server_ip);

    // Visualizzazione della risposta
    if (response.status == STATUS_SUCCESS) {
        request.city[0] = toupper(request.city[0]);
        
        char* type_label;
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
                // Caso di sicurezza
                printf("Dati meteo ricevuti (Valore: %.1f)\n", response.value);
        }

    } else if (response.status == STATUS_CITY_NOT_AVAILABLE) {
        printf("Città non disponibile\n");
    } else if (response.status == STATUS_INVALID_REQUEST) {
        printf("Richiesta non valida\n");
    } else {
        printf("Errore sconosciuto dal server (Status: %d)\n", response.status);
    }

    // CHIUSURA DELLA CONNESSIONE
    closesocket(clientSocket);
    clearwinsock();
    return 0;
} // main end