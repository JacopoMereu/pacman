#include "pacman.h"

/**
 * Funzione che si occupa di gestire l'oggetto pacman
 * @param mappa             Matrice di caselle che costituiscono la mappa
 * @param fd_coordinate_in  Descrittore di file in scrittura della pipe usata da pacman per trasmette la propria posizione
 * @param posizione_iniziale    Contiene la posizione iniziale e l'ID di pacman
 * */
void pacman(Casella mappa [MAX_Y][MAX_X], int fd_coordinate_in, Oggetto posizione_iniziale) {
    
    // Inizializzo pacman coi parametri passati
    Oggetto pacman;
    pacman.c = SIMBOLO_PACMAN;
    pacman.colore = PACMAN;
    pacman.x = posizione_iniziale.x;
    pacman.y = posizione_iniziale.y;
    pacman.id_oggetto = posizione_iniziale.id_oggetto;
    pacman.pid_oggetto = getpid();

    // Scrivo la posizione iniziale del pacman
    write(fd_coordinate_in, &pacman, sizeof(pacman));


    pid_t pidTmp;       // Conterrà il valore restituito dalla waitpid
    pid_t pid_missile[N_DIREZIONI];     // Vettore che conterrà i pid dei missili
    bool missiliAttivo[N_DIREZIONI] = {false};  // Vettore che tiene traccia di quali missili sono attualmente attivi (sono stati "sparati")
    Oggetto missileTmp;     // Conterrà temporaneamente l'ID del missile e le sue coordinate iniziale 

    int i;          // indice usato nei loop per N_DIREZIONI, contiene una direzione
    int dx, dy;     // Contengono rispettivamente l'incremento (o decremento) di x e y di pacman
    int direzione_scelta;   // Contiene la direzione scelta (freccia della tastiera premuta dall'utente)
    char c;     // Contiene il tasto inserito dall'utente
 
    while (true) {
        // Inizializzo dx e dy a 0 a ogni ciclo
        dx = dy = 0;

        // Rilevo il tasto premuto e lo salvo in c
        switch (c = getch()) {

            // rilevata freccia su
            case FRECCIA_SU:
                // Se       pacman è sotto il bordo (della mappa) orizzontale in alto
                // E se     la casella in cui vuole andare è attraversabile
                if (pacman.y > 0 
                && mappa[pacman.y-1][pacman.x].attraversabile)
                    // Assegno dy
                    dy = -PASSO;
                // Esce dallo switch
                break;

            // rilevata freccia giù
            case FRECCIA_GIU:
                // Se       pacman è sopra il bordo (della mappa) orizzontale in basso
                // E se     la casella in cui vuole andare è attraversabile
                if (pacman.y<MAX_Y-1 
                && mappa[pacman.y+1][pacman.x].attraversabile) 
                    dy = PASSO;
                // Esce dallo switch
                break;

            // rilevata freccia sinistra
            case FRECCIA_SINISTRA:
                // Se           pacman è a destra del bordo (della mappa) verticale sinistro
                // E se         la casella in cui vuole andare è attraversabile
                // OPPURE se    pacman è su una porta
                if ((pacman.x>0 
                && mappa[pacman.y][pacman.x-1].attraversabile) ||
                    (mappa[pacman.y][pacman.x].c == SIMBOLO_PORTA))
                    // Assegna dx
                    dx = -PASSO;
                // Esce dallo switch
                break;

            // rilevata freccia destra
            case FRECCIA_DESTRA:
                // Se           pacman è a sinistra del bordo (della mappa) verticale destro
                // E se         la casella in cui vuole andare è attraversabile
                // OPPURE se    pacman è su una porta
                if ((pacman.x<MAX_X-1 && mappa[pacman.y][pacman.x+1].attraversabile) ||
                    (mappa[pacman.y][pacman.x].c == SIMBOLO_PORTA))
                    // Assegno dx
                    dx = PASSO;
                // Esce dallo switch
                break;
            default:
                dx = dy = 0;
        }

        // Controllo missili esplosi
        // Per tutte le direzioni
        for(i=0; i<N_DIREZIONI;i++) 
            // Se il misisle i è attivo
            if(missiliAttivo[i]) 
                // Controllo se è esploso (se il processo missile è stato ucciso dal processo gC)
                if((pidTmp = waitpid(pid_missile[i], NULL, WNOHANG)) == pid_missile[i]) {
                    // Assegno un valore di default al pid i-esimo
                    pid_missile[i] = 0;
                    // Imposto il missile i come NON attivo
                    missiliAttivo[i] = false;
                }

        // Utente preme lo spazio della tastiera: spara missili
        if(c == ' ')
            // Controllo per tutte le direzioni
            for(i=0; i<N_DIREZIONI;i++)
                // Se il missile i NON è attivo
                if(!missiliAttivo[i]) {
                    // Lo attivo
                    missiliAttivo[i] = true;
                    // Fork
                    switch (pid_missile[i] = fork()) {
                        // caso ERRORE
                        case ERRORE:
                            perror("Errore fork (Pacman -> Missile[?])");
                            exit(ERRORE);
                            break;
                        // caso FIGLIO
                        case FIGLIO:
                            // switch per la direzione
                            switch(i) {

                                // direzione DESTRA
                                case DESTRA:
                                    // Inizializza le coordinate del missile destro assegnandogli la x di pacman incrementata di 1 e la stessa y
                                    missileTmp.x = pacman.x + 1;
                                    missileTmp.y = pacman.y;
                                    // Esce dallo switch
                                    break;
                                
                                // direzione SINISTRA
                                case SINISTRA:
                                    // Inizializza le coordinate del missile sinistro assegnandogli la x di pacman decrementata di 1 e la stessa y
                                    missileTmp.x = pacman.x - 1;
                                    missileTmp.y = pacman.y;
                                    // Esce dallo switch
                                    break;
                                
                                // direzione SU
                                case SU:
                                    // Inizializza le coordinate del missile in alto assegnandogli la y di pacman decrementata di 1 e la stessa x
                                    missileTmp.x = pacman.x;
                                    missileTmp.y = pacman.y - 1;
                                    // Esce dallo switch
                                    break;

                                // direzione GIU
                                case GIU:
                                    // Inizializza le coordinate del missile destro assegnandogli la y di pacman incrementata di 1 e la stessa x
                                    missileTmp.x = pacman.x;
                                    missileTmp.y = pacman.y + 1;
                                    // Esce dallo switch
                                    break;
                            }
                            // Inizializza l'ID del missile
                            missileTmp.id_oggetto = (pacman.id_oggetto * OFFSET_ID_MISSILE) + i ;
                            // Invoca la funzione missile
                            missile(mappa, fd_coordinate_in, missileTmp, i);

                            // Esce dallo switch
                            break;
                        default:
                            break;
                    }
                }

        // Se       pacman è su una porta
        // E        la x di pacman è o 0 o MAX_X-1
        if(mappa[pacman.y][pacman.x].c == SIMBOLO_PORTA 
        && (pacman.x == 0 || pacman.x == MAX_X-1)) {
            // Se       la x di pacman == 0  (pacman è sulla porta sinistra)
            // E se     il tasto premuto dall'utente è la freccia sinistra della tastiera
            if(pacman.x == 0 && c == FRECCIA_SINISTRA)
                // La x di pacman diventa MAX_X  (una casella a destra della porta a destra)
                pacman.x = MAX_X;
            // Altrimenti se    la x di pacman == MAX_X-1  (pacman è sulla porta destra)
            // E se             il tasto premuto dall'utente è la freccia destra della tastiera       
            else if (pacman.x == MAX_X-1 && c == FRECCIA_DESTRA)
                // La x di pacman diventa -1  (una casella a sinistra della porta a sinistra)
                pacman.x = -1;            
        }

        // Aggiorno le coordiante di pacman aggiungendo dx e dy
        pacman.x += dx;
        pacman.y += dy;

        // invio la pos aggiornata al controllore
        write(fd_coordinate_in, &pacman, sizeof(pacman));
    }
}