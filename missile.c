#include "missile.h"

/**
 * Funzione che si occupa di gestire l'oggetto misisle
 * @param mappa             Matrice di caselle che costituiscono la mappa
 * @param fd_coordinate_in  Descrittore di file in scrittura della pipe usata dal missile per trasmette la propria posizione
 * @param posizione_iniziale    Contiene la posizione iniziale e l'ID del missile
 * @param direzione_missile     Contiene il valore intero che identifica la direzione del missile (DESTRA, SINISTRA, SU o GIU)
 * */
void missile(Casella mappa [MAX_Y][MAX_X], int fd_coordinate_in, Oggetto posizione_iniziale, int direzione_missile) {
    // Inizializzo il missile coi parametri passati
    Oggetto missile;
    missile.c = SIMBOLO_MISSILE;
    missile.colore =  MISSILE;
    missile.x = posizione_iniziale.x;
    missile.y = posizione_iniziale.y;
    missile.id_oggetto = posizione_iniziale.id_oggetto;
    missile.pid_oggetto = getpid();

    // Scrivo la posizione iniziale del missile
   write(fd_coordinate_in, &missile, sizeof(missile));

    // Tempo pausa in microsecondi: 1 mln di microsecondi = 1 secondo
    int pausa=150000;
    // piccola sleep iniziale del processo
    usleep(pausa/3);
   
    // Contengono rispettivamente l'incremento (o decremento) di x e y del missile
    int dx, dy; 
    dx = dy = 0;
    // indice usato nei loop per N_DIREZIONI, contiene una direzione
    int i;

    // Aggiorna dx e dy in base alle direzioni scelte
    if(direzione_missile == SU) 
        dy = -PASSO;
    else if(direzione_missile == DESTRA) 
        dx = PASSO;
    else if(direzione_missile == GIU) 
        dy = PASSO;
    else 
        dx = -PASSO;
    

    while (true) {
        // Controllo teletrasporto 
        //  Se          il missile è in una porta
        //  E se        la x del missile è 0 OPPURE MAX_X-1
        //
        //  OPPURE se   il missile è a sinistra di una porta (sarà la porta sx)
        //  E se        la x del missile è minore di 0
        //  E se        la direzione del missile è SINISTRA
        //
        //  OPPURE se   il missile è a destra di una porta (sarà la porta dx)
        //  E se        la x del missile è maggiore di MAX_X-1
        //  E se        la direzione del missile è DESTRA
        if( (mappa[missile.y][missile.x].c == SIMBOLO_PORTA && (missile.x == 0 || missile.x == MAX_X-1))
            || (mappa[missile.y][missile.x+1].c == SIMBOLO_PORTA && missile.x < 0 && direzione_missile == SINISTRA)
            || (mappa[missile.y][missile.x-1].c == SIMBOLO_PORTA && missile.x > MAX_X-1 && direzione_missile == DESTRA) ) {
            // Se       x del missile <= 0
            // E se     la direzione è SINISTRA
            if(missile.x <= 0 && direzione_missile == SINISTRA) 
                // x del missile = MAX_X (a destra di 1 della porta dx)
                missile.x = MAX_X;
            // Se       x del missile >= MAX_X-1
            // E se     la direzione è DESTRA
            else if (missile.x >= MAX_X-1 && direzione_missile == DESTRA)
                // x del missile = -1 (a sinistra di 1 della porta sx)
                missile.x = -1;   
        }

        // Aggiorna le coordinate aggiugendo dx e dy
        missile.x += dx;
        missile.y += dy;

        // invia la pos aggiornata al controllore
        write(fd_coordinate_in, &missile, sizeof(missile));

        // sleep del processo
        usleep(pausa); 
    } 
}