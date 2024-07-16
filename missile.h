#include <curses.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>

///////////////////////////
#ifndef CONDIVISI_TUTTI
#define CONDIVISI_TUTTI

    // DIMENSIONI MAPPA
    #define MAX_X 35    // LARGEZZA     (35 = 28 + 7 = dim.originale + adattamento)
    #define MAX_Y 31    // ALTEZZA

    // SIMBOLI
    #define SIMBOLO_PORTA ACS_CKBOARD       // simbolo porta
    #define SIMBOLO_MISSILE ACS_BULLET      // simbolo missile

    #define OFFSET_ID_MISSILE 10   // Valore usato per la creazione dell'ID missile

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

void missile(Casella mappa [MAX_Y][MAX_X], int fd_coordinate_in, Oggetto posizione_iniziale,
    int direzione_missile);