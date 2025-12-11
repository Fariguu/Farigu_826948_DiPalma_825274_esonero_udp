/*
 * main.c
 *
 * UDP Client - Template for Computer Networks assignment
 *
 * This file contains the boilerplate code for a UDP client
 * portable across Windows, Linux, and macOS.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h> // Necessario per strncpy e strchr
#include <ctype.h> // Necessario per la funzione toupper()
#include <stdint.h>
#include "protocol.h"

#define REQ_SIZE (sizeof(char) + sizeof(request.city))
#define RESP_SIZE (sizeof(unsigned int) + sizeof(char) + sizeof(float))
#define NO_ERROR 0

#if defined WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <unistd.h>
    #include <sys/types.h>
    #include <netinet/in.h>
    #include <netdb.h>
    #define closesocket close
#endif
#include <arpa/inet.h>
#include <sys/socket.h>

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

    // --- GESTIONE ARGOMENTI RIGA DI COMANDO ---
    // valori di default
    char *server_ip = DEFAULT_SERVER_ADDRESS;
    int port = DEFAULT_PORT;
    char *request_arg = NULL; // Conterrà la stringa
    
    // variabili per DNS Lookup e output
    char server_ip_str[16]; // stringa per l'indirizzo IP numerico finale (es. "127.0.0.1")
    char *server_name_canonico = NULL; // nome canonico per l'output (es. "localhost")
    struct hostent *host_info; // struttura per i risultati DNS

    // parsing degli argomenti: -s server, -p port, -r request
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
            server_ip = argv[++i];
        } else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-r") == 0 && i + 1 < argc) {
            request_arg = argv[++i];
        }
    }

    // se manca la richiesta obbligatoria (-r), mostra l'uso ed esce
    if (request_arg == NULL) {
        printf("Uso: %s [-s server] [-p port] -r \"type city\"\n", argv[0]);
        printf("Esempio: %s -r \"t bari\"\n", argv[0]);
        return -1;
    }

    // parsing della stringa richiesta
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

    /* tenta di convertire l'input 'server_ip' in indirizzo IP numerico.
     se fallisce, 'server_ip' è un nome simbolico (hostname). */

    if ((serverAddress.sin_addr.s_addr = inet_addr(server_ip)) == INADDR_NONE) {

        // caso 1: L'input è un NOME SIMBOLICO
        // gethostbyname può essere chiamata solo con AF_INET 
        host_info = gethostbyname(server_ip);
        
        if (host_info == NULL) {
            errorhandler("Risoluzione nome server fallita (gethostbyname).");
        }
        
        memcpy(&serverAddress.sin_addr, host_info->h_addr_list[0], host_info->h_length); // copia l'indirizzo risolto
        
        server_name_canonico = host_info->h_name; // nome canonico dal DNS
        
    } else {

        // caso 2: L'input è un INDIRIZZO IP NUMERICO
        // l'indirizzo è già in serverAddress.sin_addr
        struct in_addr addr;
        addr.s_addr = serverAddress.sin_addr.s_addr;
        
        host_info = gethostbyaddr((char *) &addr, sizeof(addr), AF_INET); // Reverse lookup
        
        if (host_info == NULL) {
            server_name_canonico = server_ip; // usa l'IP come nome
        } else {
            server_name_canonico = host_info->h_name; // nome canonico dal DNS
        }
    }

    // salva la stringa IP finale (per l'output)
    strncpy(server_ip_str, inet_ntoa(serverAddress.sin_addr), 15);
    server_ip_str[15] = '\0'; // Assicura la terminazione

    // LOGICA DI COMUNICAZIONE CON IL SERVER (REQUEST/RESPONSE)

    // SERIALIZZAZIONE MANUALE DELLA RICHIESTA

    char buffer_request[REQ_SIZE];
    int offset = 0;

    memcpy(buffer_request + offset, &request.type, sizeof(char));
    offset += sizeof(char);

    memcpy(buffer_request + offset, request.city, sizeof(request.city));
    offset += sizeof(request.city);

    // Invio della richiesta al server usando sendto()
    // usiamo l'indirizzo del server precedentemente risolto in serverAddress

    if (sendto(clientSocket, buffer_request, offset, 0,
            (struct sockaddr*)&serverAddress, sizeof(serverAddress)) != offset) { 
        errorhandler("sendto() fallita: inviato numero di byte diverso da quello atteso.");
    }

    // RICEZIONE DELLA RISPOSTA DAL SERVER (recvfrom)
    // definiamo un buffer per ricevere il datagramma di risposta (9 byte)

    char buffer_response[RESP_SIZE];
    struct sockaddr_in fromAddr;
    unsigned int fromSize = sizeof(fromAddr);

    // ricezione del datagramma dal server
    int bytes_received = recvfrom(clientSocket, buffer_response, RESP_SIZE, 0,
                                (struct sockaddr*)&fromAddr, &fromSize);

    if (bytes_received <= 0) {
        errorhandler("recvfrom() fallita o nessun datagramma ricevuto.");
    }

    // controllo di sicurezza: Verifica che il pacchetto provenga dal server atteso
    if (serverAddress.sin_addr.s_addr != fromAddr.sin_addr.s_addr) {
        printf("ERRORE DI SICUREZZA: Pacchetto ricevuto da sorgente sconosciuta. IP: %s\n", inet_ntoa(fromAddr.sin_addr));
        closesocket(clientSocket);
        clearwinsock();
        exit(1);
    }

    // DESERIALIZZAZIONE E CONVERSIONE NBO DELLA RISPOSTA
    weather_response_t response;
    offset = 0;

    uint32_t net_status;
    memcpy(&net_status, buffer_response + offset, sizeof(uint32_t));
    response.status = ntohl(net_status); // conversione da Network a Host
    offset += sizeof(uint32_t);

    memcpy(&response.type, buffer_response + offset, sizeof(char));
    offset += sizeof(char);

    uint32_t net_value;
    memcpy(&net_value, buffer_response + offset, sizeof(uint32_t));
    net_value = ntohl(net_value); // conversione da Network a Host

    memcpy(&response.value, &net_value, sizeof(float)); 

    // stampa iniziale: Ricevuto risultato dal server <nomeserver> (ip <IP>).
    printf("Ricevuto risultato dal server %s (ip %s). ", server_name_canonico, server_ip_str);

    // visualizzazione della risposta in base allo Status
    if (response.status == STATUS_SUCCESS) {
        
        // reinseriamo la logica toupper per la città
        request.city[0] = toupper(request.city[0]);

        switch(response.type) {
            case 't': 
                // NomeCittà: Temperatura = XX.X°C
                printf("%s: Temperatura = %.1f°C\n", request.city, response.value); 
                break;
            case 'h': 
                // NomeCittà: Umidità = XX.X%
                printf("%s: Umidità = %.1f%%\n", request.city, response.value); 
                break;
            case 'w': 
                // NomeCittà: Vento = XX.X km/h
                printf("%s: Vento = %.1f km/h\n", request.city, response.value); 
                break;
            case 'p': 
                // NomeCittà: Pressione = XXXX.X hPa
                printf("%s: Pressione = %.1f hPa\n", request.city, response.value); 
                break;
            default:
                printf("Dati meteo ricevuti (Valore: %.1f)\n", response.value);
        }

    } else if (response.status == STATUS_CITY_NOT_AVAILABLE) {
        // città non disponibile
        printf("Città non disponibile\n");
    } else if (response.status == STATUS_INVALID_REQUEST) {
        // richiesta non valida
        printf("Richiesta non valida\n");
    } else {
        // errore generico
        printf("Errore sconosciuto dal server (Status: %d)\n", response.status);
    }

    // CHIUSURA DELLA CONNESSIONE
    closesocket(clientSocket);
    clearwinsock();
    return 0;
} // main end