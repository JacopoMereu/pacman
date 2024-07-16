#include <sys/types.h>
#include <stdbool.h>
#include <curses.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include "fantasma.h"
#include "pacman.h"
#include <fcntl.h>  // file control

///////////////////////////
#ifndef CONDIVISI_TUTTI
#define CONDIVISI_TUTTI

    // DIMENSIONI MAPPA
    #define MAX_X 35    // LARGEZZA     (35 = 28 + 7 = dim.originale + adattamento)
    #define MAX_Y 31    // ALTEZZA

    // SIMBOLI
    #define SIMBOLO_PORTA ACS_CKBOARD       // simbolo porta
    #define SIMBOLO_MISSILE ACS_BULLET      // simbolo missile

    #define OFFSET_ID_MISSILE 10     // Valore usato per la creazione dell'ID missile

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
#ifndef CONDIVISI_PACMAN
#define CONDIVISI_PACMAN
    #define SIMBOLO_PACMAN '@'    // simbolo pacman
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


///MACRO SOLO DI GESTORE///

    // DESCRITTORI PIPE
    #define DESCRITTORE_LETTURA 0
    #define DESCRITTORE_SCRITTURA 1
    #define NUMERO_FD 2
    
    #define N_FANTASMI 4    // Numero di fantasmi

    #define SIMBOLO_SEME ACS_DEGREE

    // Tempo che un fantasma deve aspettare per muoversi dopo essere "scoperto"
    #define TEMPO_CASUALE_MINIMO 1      // Tempo minimo
    #define TEMPO_CASUALE_MASSIMO 3     // Tempo massimo
   
    #define MAX_VITE 3          // Numero massimo di vite 
    #define MAX_COLPI_MISSILI 10

    #define VITTORIA 1      // Codice vittoria
    #define SCONFITTA 0     // Codice sconfitta

    #define PROPRIETARIO_PACMAN  0
    #define PROPRIETARIO_FANTASMA 1
///////////////////////////


void avviaGioco();

void inizializzaMappa(Casella mappa[MAX_Y][MAX_X]);

void inizializzaColori();
void sfondoTerminale();

void generaCoordinate(Casella mappa[MAX_Y][MAX_X], int fd_in, int nCoordinateDaGenerare);

int controlli(Casella mappa [MAX_Y][MAX_X], Oggetto coordinateOggettiIniziali[N_FANTASMI + 1], 
    int fd_coordinate_out, int fd_rimbalzi_in[N_FANTASMI], int fd_vita_fantasma_out[N_FANTASMI], int fd_morte_fantasma_in[N_FANTASMI],
    int fd_attiva_fantasmi_in[N_FANTASMI]) ;
