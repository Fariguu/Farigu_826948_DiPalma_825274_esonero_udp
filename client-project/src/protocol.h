/*
 * protocol.h
 *
 * Server header file
 * Definitions, constants and function prototypes for the server
 */

#ifndef PROTOCOL_H_
#define PROTOCOL_H_

/* =================================================================
 * COSTANTI CONDIVISE (da Assegnazione.md)
 * ================================================================= */

// porta di default (da specifiche Client e Server)
#define DEFAULT_PORT 56700

// indirizzo di default (da specifiche Client)
#define DEFAULT_SERVER_ADDRESS "127.0.0.1"

// codici di stato (da specifiche Risposta)
#define STATUS_SUCCESS 0
#define STATUS_CITY_NOT_AVAILABLE 1
#define STATUS_INVALID_REQUEST 2

/* =================================================================
 * STRUTTURE DEL PROTOCOLLO APPLICATIVO (da Assegnazione.md)
 * ================================================================= */

/**
 * messaggio di Richiesta (Client -> Server)
 */
typedef struct {
    char type;        // 't', 'h', 'w', 'p'
    char city[64];    // nome città (null-terminated)
} weather_request_t;

/**
 * messaggio di Risposta (Server -> Client)
 */
typedef struct {
    unsigned int status;  // 0, 1, o 2
    char type;            // echo del tipo richiesto
    float value;          // valore meteo
} weather_response_t;


/* =================================================================
 * PROTOTIPI FUNZIONI (da Assegnazione.md)
 * ================================================================= */

float get_temperature(void);
float get_humidity(void);
float get_wind(void);
float get_pressure(void);

#endif /* PROTOCOL_H_ */
