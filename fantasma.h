#include <curses.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include "missile.h"

///////////////////////////
#ifndef CONDIVISI_TUTTI
#define CONDIVISI_TUTTI

    // DIMENSIONI MAPPA
    #define MAX_X 35    // LARGEZZA     (35 = 28 + 7 = dim.originale + adattamento)
    #define MAX_Y 31    // ALTEZZA

    // SIMBOLI
    #define SIMBOLO_PORTA ACS_CKBOARD       // simbolo porta
    #define SIMBOLO_MISSILE ACS_BULLET      // simbolo missile

    #define OFFSET_ID_MISSILE 10    // Valore usato per la creazione dell'ID missile

    // CODICI USCITA DA UNA FUNZIONE
    #define ERRORE -1       // errore
    #define NORMALE 0       // normale

    #define FIGLIO 0        // codice figlio (fork)
    
    // Enumerazione che contiene tutti le combinazioni dei colori, tra cui quelli dei fantasmi
    typedef enum {TERMINALE, CASELLA_ATTRAVERSABILE, PORTA, CASELLA_NON_ATTRAVERSABILE, 
                    PACMAN, FANTASMA_ROSSO, FANTASMA_MAGENTA, FANTASMA_CIANO, FANTASMA_VERDE,
                        MISSILE} Colore;


    // struttura casella, in una casella ci sarà al massimo un oggetto
    typedef struct {
        bool fantasmaNascosto;  // Se nella casella c'è un fantasma nascosto (v/f)
        bool seme;              // Se nella casella c'è un semino da mangiare (v/f)
        bool attraversabile;    // Se la casella NON è un muro (o uno spazio interno ai muri) (v/f)
        bool incrocio;          // Se la casella è un incrocio (3 o 4 vie) (v/f)
        bool angolo;            // Se la casella è un angolo (2 vie) (v/f)
        // (se la casella è un incrocio, la variabile angolo sarà impostata a falsa)
        chtype c;           // Carattere della casella
    } Casella;

    // struttura oggetto (missile, fantasma, pacman, ...)
    typedef struct {
        int id_oggetto;     // id oggetto
        int x;              // coordinata x
        int y;              // coordinata y
        pid_t pid_oggetto;  // pid dell'oggetto (processo)
        chtype c;           // simbolo
        Colore colore;      // colore
    } Oggetto;

#endif
///////////////////////////


///////////////////////////
#ifndef CONDIVISI_FANTASMA
#define CONDIVISI_FANTASMA
    // Valori letti dalla pipe relativi al rimbalzo dei fantasmi
    #define INVERTI_MOVIMENTO 0         // Caso rimbalzo: il fantasma deve invertire la direzione
    #define OK_MOVIMENTO 1              // Caso NON rimbalzo: il fantasma NON deve invertire la direzione

    #define SIMBOLO_FANTASMA ACS_DIAMOND    // simbolo fantasma 

    // Valori interi che codificano lo stato vitale del fantasma
    #define FANTASMA_MORTO 0            
    #define FANTASMA_VIVO 1 

    // Valori interi che codificano l'avvio del fantasma
    #define AVVIA_NASCOSTO 0        // Aspetta di ricevere un valore booleano 'false' dalla pipe avvia fantasma per avviarsi, per i primi processi fantasma, quelli nascosto
    #define AVVIA_SUBITO 1          // Non aspetta di ricevere il valore booleano, quando un fantasma muore e ritorna operativo non deve aspettare

    #define SIMBOLO_INGRESSO_PRIGIONE ACS_UARROW    // Simbolo della prigione
#endif
///////////////////////////


///////////////////////////
#ifndef DIREZIONI_OGGETTI
#define DIREZIONI_OGGETTI

    // Numero di direzioni
    #define N_DIREZIONI 4   
    
    // Valori interi che rappresentano le direzioni
    #define SU 0
    #define DESTRA 1 
    #define GIU 2
    #define SINISTRA 3

    // Grandezza del pazzo, numero di caselle che può attraversare con uno spostamento
    #define PASSO 1
#endif
///////////////////////////


//////MACRO SOLO DI FANTASMA//////

// Intervallo di tempo casuale per sparare un missile
#define TEMPO_SPARO_CASUALE_MIN 1       // Tempo minimo
#define TEMPO_SPARO_CASUALE_MAX 3      // Tempo massimo

// Intervallo di tempo casuale in cui il fantasma rimane inattivo in prigione
#define TEMPO_CASUALE_RINASCITA_MIN 5   // Tempo minimo
#define TEMPO_CASUALE_RINASCITA_MAX 10  // Tempo massimo

//////////////////////////////////

typedef struct {
    int x;
    int y;
} Coordinata;


void fantasma(Casella mappa [MAX_Y][MAX_X], int fd_coordinate_in ,int fd_stato_vita_in, int fd_stato_morto_out,
    int fd_rimbalzo_out, int fd_attivazione_out, int modalita_d_avvio, Oggetto posizione_iniziale) ;
    
Colore restituisciColore(int i);

char* stampaColore(Colore colore);

void gestisciMorte(Casella mappa [MAX_Y][MAX_X], Oggetto fantasmaMorto, int fd_coordinate_in,
    int fd_stato_vita_in, int fd_stato_morto_out, int fd_rimbalzo_out);

int cercaPrimoPercorsoDisponibile(Casella mappa [MAX_Y][MAX_X], Coordinata percorso[MAX_Y * MAX_X], 
    Oggetto fantasmaMorto, Coordinata ingressoPrigione);

bool aggiungiNodo(Casella mappa [MAX_Y][MAX_X], Coordinata percorso[MAX_Y * MAX_X], int* indice_vettore, 
    Coordinata attuale, Coordinata fine, int direzione_chiamante);