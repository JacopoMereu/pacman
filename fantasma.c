#include "fantasma.h"

/**
 * Funzione che si occupa di gestire l'oggetto fantasma
 * @param mappa              Matrice di caselle che costituiscono la mappa
 * @param fd_coordinate_in   Descrittore di file in scrittura della pipe usata dal fantasma per trasmette la propria posizione
 * @param fd_stato_vita_in   Descrittore di file in scrittura della pipe usata dal fantasma (o meglio, dal processo che gestisce la morte del fantasma)
 *                           per informare gC che è di nuovo operativo
 * @param fd_stato_morto_out Descrittore di file in lettura (non bloccante) della pipe usata da gC per informare il fantasma che è morto e, quindi, di avviare
 *                           il processo per la gestione della sua morte
 * @param fd_rimbalzo_out    Descrittore di file in lettura (non bloccante) della pipe usata da gC per informare il fantasma che ha colliso con un altro fantasma e,
 *                           quindi, invertire la direzione
 * @param fd_attivazione_out Descrittore di file in lettura della pipe usata da gC per dire quando il fantasma deve avviarsi
 * @param modalita_d_avvio   intero che rappresenta la modalità di avvio del fantasma, 
 *                              se il valore è   AVVIA_NASCOSTO,    il processo aspetterà a gC per avviarsi
 *                              se il valore è   AVVIA_SUBITO,      il processo NON aspetterà a gC
 * @param posizione_iniziale    Contiene la posizione iniziale e l'ID del fantasma
 * */
void fantasma(Casella mappa [MAX_Y][MAX_X], int fd_coordinate_in ,int fd_stato_vita_in, int fd_stato_morto_out,
    int fd_rimbalzo_out, int fd_attivazione_out, int modalita_d_avvio, Oggetto posizione_iniziale) {

    // Inizializzo il fantasma coi parametri passati
    Oggetto fantasma;
    fantasma.c = SIMBOLO_FANTASMA;
    fantasma.colore = restituisciColore(posizione_iniziale.id_oggetto   );
    fantasma.x = posizione_iniziale.x;
    fantasma.y = posizione_iniziale.y;
    fantasma.id_oggetto = posizione_iniziale.id_oggetto;
    fantasma.pid_oggetto = getpid();

    // Scrivo la posizione iniziale del fantasma
    write(fd_coordinate_in, &fantasma, sizeof(fantasma));

    // Tempo pausa in microsecondi: 1 mln di microsecondi = 1 secondo
    int pausa=300000;
    // piccola sleep iniziale del processo
    usleep(pausa/3);

    // Se       modalità di avvio corrisponde al valore intero usato nell'avvio dei fantasmi iniziali (nascosti)
    if(modalita_d_avvio == AVVIA_NASCOSTO) {
        // definisco la variabile fantasmaBloccato
        bool fantasmaAttivo = false;
        // finché tale variabile è vera
        while(!fantasmaAttivo) {
            // Leggo dalla pipe attivazione_out per vedere se il processo si deve attivare
            read(fd_attivazione_out, &fantasmaAttivo, sizeof(fantasmaAttivo));
            // Scrivo le coordinate del processo sulla pipe per la trasmissione delle coordinate reali (che è fermo)
            write(fd_coordinate_in, &fantasma, sizeof(fantasma));
            // Piccola pausa
            usleep(pausa/3);
        }
        // Chiudo il descrittore di file, non servirà più
        close(fd_attivazione_out);
    }

    int movimento_letto;    // Contiene l'ultimo valore letto della read di stato_morto_out
    int tmp;         // Contiene il valore temporaneo della read di stato_morto_out

    // Contengono rispettivamente l'incremento (o decremento) di x e y del missile
    int dx, dy; 
    dx = dy = 0;

    
    int i;    // indice usato nei loop per N_DIREZIONI, contiene una direzione
    int direzione_scelta = -1;      // contiene la direzione del fantasma
    bool direzioni[N_DIREZIONI] = {false};  // tiene traccia di quali direizoni sono percorribili

    pid_t pidTmp;             // Conterrà il valore restituito dalla waitpid
    pid_t pid_missile[N_DIREZIONI];             // Vettore che conterrà i pid dei missili
    bool missiliAttivo[N_DIREZIONI] = {false};  // Vettore che tiene traccia di quali missili sono attualmente attivi (sono stati "sparati")
    Oggetto missileTmp;       // Conterrà temporaneamente l'ID del missile e le sue coordinate iniziale 

    pid_t pid_sposta_fantasma;      // Conterrà il pid del processo generato per gestire la morte del fantasma
    int stato_vitale;               // Contiene lo stato vitale attuale del fantasma

    // Timer dopo il quale un fantasma spara un missile
    int tempo_sparo_missili = TEMPO_SPARO_CASUALE_MIN + rand() % (TEMPO_SPARO_CASUALE_MAX - TEMPO_SPARO_CASUALE_MIN + 1);
    // Contiene il tempo dell'ultima volta che il fantasma a sparato
    time_t ultimo_tempo_sparo = time((time_t*)NULL);
    // Contiene il tempo reale
    time_t tempo_reale;

    while (true) {
            // Salvo il tempo reale
            tempo_reale = time((time_t*)NULL);

            // Inizializzo tutte le direzioni percorribili a false
            for(i = 0; i < N_DIREZIONI; i++)
                direzioni[i] = false;

            // Inizializza stato_vitale
            stato_vitale = FANTASMA_VIVO;
            // Legge da stato_vitale_out
            //      (se c'è un valore, questo sicuramente è FANTASMA_MORTO)
            if((read(fd_stato_morto_out, &tmp, sizeof(tmp))) > 0) 
                //if( == EAGAIN)
                    stato_vitale = tmp;

            // Se   lo stato vitale del fantasma è diverso da FANTASMA_VIVO (il fantasma è morto)   
            if(stato_vitale != FANTASMA_VIVO) {
                // fork
                pid_sposta_fantasma = fork();
                // caso ERRORE
                if (pid_sposta_fantasma == ERRORE) {
                    perror("Errore fork (Fantasma -> gestioneMorte)");
                    exit(ERRORE);
                // caso FIGLIO
                } else if (pid_sposta_fantasma == FIGLIO)
                    // Avvia la procedura per gestire la morte del fantasma, passandogli la mappa, se stesso (l'oggetto fantasma) 
                    // e tutti i descrittori di file di cui necessiterà
                    gestisciMorte(mappa, fantasma, fd_coordinate_in, fd_stato_vita_in, fd_stato_morto_out, fd_rimbalzo_out);
                // Il padre chiude i descrittori
                else {
                    // Chiudo i descrittori dei file usati dal padre
                    close(fd_attivazione_out);
                    close(fd_coordinate_in);
                    close(fd_rimbalzo_out);
                    close(fd_stato_morto_out);
                    // Il processo termina
                    exit(NORMALE);
                }
            }

            // Inizializza movimento_letto
            movimento_letto = OK_MOVIMENTO;
            // Legge dalla pipe rimbalzo_out finché non è vuota
            while((read(fd_rimbalzo_out, &tmp, sizeof(tmp))) > 0) 
                    // Assegna tmp a movimento_letto
                    movimento_letto = tmp;

            // Se movimento contiene il valore che codifica l'inversione della direzione
            if(movimento_letto == INVERTI_MOVIMENTO) {
                // SU diventa GIU'
                if (direzione_scelta == SU) {
                    direzione_scelta = GIU;
                    dy = PASSO;
                    dx = 0;
                }
                // GIU' diventa SU
                else if (direzione_scelta == GIU) {
                    direzione_scelta = SU;
                    dy = -PASSO;
                    dx = 0;
                // DESTRA diventa SINISTRA
                } else if (direzione_scelta == DESTRA) {
                    direzione_scelta = SINISTRA;
                    dx = -PASSO;
                    dy = 0;
                }
                // SINISTRA diventa DESTRA
                else if (direzione_scelta == SINISTRA) {
                    direzione_scelta = DESTRA;
                    dx = PASSO;
                    dy = 0;
                }
            }

            // Se non si è mai mosso
            if(dx == 0 && dy == 0) {
                // Controlla in quale direzione può muoversi

                //                  |        casella in alto      |.attraversabile
                if (fantasma.y>0 && mappa[fantasma.y-1][fantasma.x].attraversabile)
                    direzioni[SU] = true;           // Aggiunge SU alla direzioni percorribili

                //                  |      casella a sinistra    |.attraversabile
                if (fantasma.x>0 && mappa[fantasma.y][fantasma.x-1].attraversabile)
                    direzioni[SINISTRA] = true;     // Aggiunge SINISTRA alla direzioni percorribili 

                //                        |       casella a destra      |.attraversabile
                if (fantasma.x<MAX_X-1 && mappa[fantasma.y][fantasma.x+1].attraversabile)
                    direzioni[DESTRA] = true;       // Aggiunge DESTRA alla direzioni percorribili

                //                        |       casella in basso      |.attraversabile
                if (fantasma.y<MAX_Y-1 && mappa[fantasma.y+1][fantasma.x].attraversabile)
                    direzioni[GIU] = true;          // Aggiunge GIU alla direzioni percorribili

                // Sceglie una direzione casuale finché la direzione scelta è una direzione 
                // fra quelle percorribili
                do {
                    direzione_scelta = rand()%N_DIREZIONI;
                } while (!direzioni[direzione_scelta]);

            // Altrimenti se    non è il primo movimento, 
            // E se             si trova in un angolo OPPURE in un incrocio
            // E se             NON deve invertire il movimento
            } else if( (mappa[fantasma.y][fantasma.x].angolo || mappa[fantasma.y][fantasma.x].incrocio)
                && movimento_letto != INVERTI_MOVIMENTO) {
                // Controlla in quale direzione può muoversi

                //                  |        casella in alto      |.attraversabile
                if (fantasma.y>0 && mappa[fantasma.y-1][fantasma.x].attraversabile 
                    && dy != PASSO)     // Se la direzione precedente (quella da cui proviene) non è GIU
                    direzioni[SU] = true;       // Aggiunge SU alle direzioni percorribili

                //                  |      casella a sinistra     |.attraversabile
                if (fantasma.x>0 && mappa[fantasma.y][fantasma.x-1].attraversabile 
                    && dx != PASSO)     // Se la direzione precedente (quella da cui proviene) non è DESTRA
                    direzioni[SINISTRA] = true;     // Aggiunge SINISTRA alle direzioni percorribili

                //                       |        casella a destra      |.attraversabile
                if (fantasma.x<MAX_X-1 && mappa[fantasma.y][fantasma.x+1].attraversabile 
                    && dx != -PASSO)    // Se la direzione precedente (quella da cui proviene) non è SINISTRA
                    direzioni[DESTRA] = true;       // Aggiunge DESTRA alle direzioni percorribili

                //                        |       casella in basso      |.attraversabile
                if (fantasma.y<MAX_Y-1 && mappa[fantasma.y+1][fantasma.x].attraversabile && 
                    dy != -PASSO)   // Se la direzione precedente (quella da cui proviene) non è SU
                    direzioni[GIU] = true;          // Aggiunge GIU alle direzioni percorribili
                
                
                // (in 1 angolo ci sono 2 direzioni, la direzione da cui viene deve essere esclusa, rimane 1 direzione percorribile)
                // Partendo dalla direzione su (valore = 0), cambia direzione finché la direzione scelta è l'unica percorribile
                if(mappa[fantasma.y][fantasma.x].angolo) {
                    direzione_scelta=SU;
                    while (!direzioni[direzione_scelta])
                        direzione_scelta++;
                }

                // (in 1 incrocio ci sono almeno 3 direzioni, la direzione da cui viene deve essere esclusa,
                //      rimangono almeno 2 direzioni percorribili, la direzione finale deve essere scelta casualmente)
                else if(mappa[fantasma.y][fantasma.x].incrocio) {            
                    do
                        direzione_scelta = rand()%N_DIREZIONI;
                    while (!direzioni[direzione_scelta]);
                }
            }  
            
            // Aggiorna dx e dy in base alle direzioni scelte
            if(direzione_scelta == SU) {
                dy = -PASSO;
                dx = 0;
            } else if(direzione_scelta == DESTRA) {
                dx = PASSO;
                dy = 0;
            }    else if(direzione_scelta == GIU) {
                dy = PASSO;
                dx = 0;
            }   else {
                dx = -PASSO;
                dy = 0;
            }

            // Controlla se è in una porta e la direzione scelta lo può "teletrasportare"
            if(mappa[fantasma.y][fantasma.x].c == SIMBOLO_PORTA && (fantasma.x == 0 || fantasma.x == MAX_X-1)) {
                // Se        il fantasma è sulla porta sinistra 
                // E se      la direzione è sinistra
                if(fantasma.x == 0 && direzione_scelta == SINISTRA)
                    // il fantasma andrà a destra di 1 casella della porta destra
                    fantasma.x = MAX_X;
                // Se        il fantasma è sulla porta destra
                // E se      la direzione è destra
                else if (fantasma.x == MAX_X-1 && direzione_scelta == DESTRA)
                    // il fantasma andrà a sinistra di 1 casella della porta sinistra
                    fantasma.x = -1;            
            }

        // Aggiorna le coordinate aggiugendo dx e dy
        fantasma.x += dx;
        fantasma.y += dy;

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
            
        // Se è passato il tempo random per sparare
        if(tempo_reale - ultimo_tempo_sparo >= tempo_sparo_missili) {
            // Ricalcolo il nuovo tempo random
            tempo_sparo_missili = TEMPO_SPARO_CASUALE_MIN + rand() % (TEMPO_SPARO_CASUALE_MAX - TEMPO_SPARO_CASUALE_MIN + 1);
            // Aggiorno il tempo dell'ultimo sparo col tempo attuale
            ultimo_tempo_sparo = tempo_reale;

            // Controllo in tutte le direzioni
            for(i=0; i<N_DIREZIONI;i++)
                // Se il missili i non è attivo
                if(!missiliAttivo[i]) {
                    // Lo attivo
                    missiliAttivo[i] = true;
                    // fork 
                    switch (pid_missile[i] = fork()) {
                        // caso ERRORE
                        case ERRORE:
                            perror("Errore fork (Fantasma[?] -> Missile[?])");
                            exit(ERRORE);
                        // caso FIGLIO
                        case FIGLIO:
                            // switch per la direzione
                            switch(i) {

                                // direzione DESTRA
                                case DESTRA:
                                    // Inizializza le coordinate del missile destro assegnandogli la x del fantasma incrementata di 1 e la stessa y
                                    missileTmp.x = fantasma.x + 1;
                                    missileTmp.y = fantasma.y;
                                    // Esce dallo switch
                                    break;
                                
                                // direzione SINISTRA
                                case SINISTRA:
                                    // Inizializza le coordinate del missile sinistro assegnandogli la x del fantasma decrementata di 1 e la stessa y
                                    missileTmp.x = fantasma.x - 1;
                                    missileTmp.y = fantasma.y;
                                    // Esce dallo switch
                                    break;

                                // direzione SU
                                case SU:
                                    // Inizializza le coordinate del missile in alto assegnandogli la y del fantasma decrementata di 1 e la stessa x
                                    missileTmp.x = fantasma.x;
                                    missileTmp.y = fantasma.y - 1;
                                    // Esce dallo switch
                                    break;

                                // direzione GIU
                                case GIU:
                                    // Inizializza le coordinate del missile destro assegnandogli la y del fantasma incrementata di 1 e la stessa x
                                    missileTmp.x = fantasma.x;
                                    missileTmp.y = fantasma.y + 1;
                                    // Esce dallo switch
                                    break;
                            }
                            // Inizializza l'ID del missile
                            missileTmp.id_oggetto = (fantasma.id_oggetto * OFFSET_ID_MISSILE) + i;
                            // Invoca la funzione missile
                            missile(mappa, fd_coordinate_in, missileTmp, i);

                            // Esce dallo switch
                            break;
                        // caso PADRE
                        default:
                            break;
                    }
                }

        }

        // invia la pos aggiornata al controllore
        write(fd_coordinate_in, &fantasma, sizeof(fantasma));

        // pausa 
        usleep(pausa); 
    } 
}


/**
 * Dato un ID fantasma, restituisce il colore associato
 * @param   i     intero positivo che rappresenta l'ID del fantasma
 * @return      Il colore associato a quel ID
 * */
Colore restituisciColore(int i) {
    if((i % 4) == 0)
        return FANTASMA_CIANO;
    else if ((i % 4) == 1)
        return FANTASMA_MAGENTA;
    else if ((i % 4) == 3)
        return FANTASMA_ROSSO;
    else 
        return FANTASMA_VERDE;  
    }

/**
 * Dato un Colore fantasma, restituisce la stringa del nome del colore
 * @param   colore  Colore del fantasma
 * @return  il nome del colore
 * */
char* stampaColore(Colore colore) {
    char* c;
        if(colore == FANTASMA_CIANO)
            c = "ciano  ";
        else if (colore == FANTASMA_MAGENTA)
            c = "magenta";
        else if (colore == FANTASMA_ROSSO)
            c = "rosso  ";
        else if (colore == FANTASMA_VERDE)
            c = "verde  ";
        
        return c;
}

/**
 * Gestisce la morte del fantasma:
 *      trova un percorso per portare il fantasma nel rettangolo/prigione
 *      esegue il percorso
 *      aspetta un tempo random
 *      esce da una direzione casuale del rettangolo
 *      avvisa gC di essere di nuovo operativo
 *      invoca la funzione fantasma passando i fd presi dal vecchio fantasma
 * @param mappa              Matrice di caselle che costituiscono la mappa
 * @param fantasmaMorto      Contiene i dati del fantasma ucciso
 * @param fd_coordinate_in   Descrittore di file in scrittura della pipe usata dal fantasma per trasmette la propria posizione
 * @param fd_stato_vita_in   Descrittore di file in scrittura della pipe usata dal fantasma (o meglio, dal processo che gestisce la morte del fantasma)
 *                           per informare gC che è di nuovo operativo
 * @param fd_stato_morto_out Descrittore di file in lettura (non bloccante) della pipe usata da gC per informare il fantasma che è morto e, quindi, di avviare
 *                           il processo per la gestione della sua morte
 * @param fd_rimbalzo_out    Descrittore di file in lettura (non bloccante) della pipe usata da gC per informare il fantasma che ha colliso con un altro fantasma e,
 *                           quindi, invertire la direzione
 * */
void gestisciMorte(Casella mappa [MAX_Y][MAX_X], Oggetto fantasmaMorto, int fd_coordinate_in, int fd_stato_vita_in, int fd_stato_morto_out, int fd_rimbalzo_out) {
    // Indici righe e colonne
    int i, j;
    
    // Trovo la coordinata dell'ingresso della prigione (triangolino senza base)
    Coordinata ingressoPrigione;
    // Controllo per tutte le righe
    for(i=0; i<MAX_Y; i++)
        // Controllo per tutte le colonne
        for(j=0; j<MAX_X; j++)
            // Se il simbolo trovato è il simbolo dell'ingresso della prigione
            if(mappa[i][j].c == SIMBOLO_INGRESSO_PRIGIONE) {
                // Memorizzo le coordinate in ingressoPrigione
                ingressoPrigione.x = j;
                ingressoPrigione.y = i;
                // Esco dal ciclio
                break;
            }

    // Ricavo l'altezza della prigione
    int altezza_prigione=1;
    i=ingressoPrigione.y;
    j=ingressoPrigione.x;
    do {
        i++;
        altezza_prigione++;
    } while(mappa[i][j].c != ' ');

    // Definisco un vettore di caselle che conterrà il perocorso che il fantasma dovrà fare per arrivare all'ingresso
    Coordinata percorso[MAX_Y * MAX_X];
    // Calcolo il percorso e salvo l'indice del vettore dell'ultima casella utile + 1 in ind_max
    int ind_max = cercaPrimoPercorsoDisponibile(mappa, percorso, fantasmaMorto, ingressoPrigione);

    // Scrivo le coordinate del fantasma morto
    write(fd_coordinate_in, &fantasmaMorto, sizeof(fantasmaMorto));

    // Faccio eseguire il percorso dal fantasma morto
    // Definisco la pausa
    int pausa = 100000;

    // Finché l'indice non supera quello massimo
    for(i=0; i<ind_max; i++) {

        // Le coordinate del fantasma morto diventa quelle della casella i del percorso
        fantasmaMorto.x = percorso[i].x;
        fantasmaMorto.y = percorso[i].y;

        // Scrivo la nuova posizione sulla pipe dell coordinate reali
        write(fd_coordinate_in, &fantasmaMorto, sizeof(fantasmaMorto));

        // Piccola pausa
        usleep(pausa);
    }
    
    // Si posizione al centro della prigione
    for(i=0; i<=(altezza_prigione/2); i++ ) {
        // Il fantsasma scende di 1
        fantasmaMorto.y++;
        // Scrive la nuova posizione
        write(fd_coordinate_in, &fantasmaMorto, sizeof(fantasmaMorto)); 
        // Pausa
        usleep(pausa);
    }

    // Calcola i secondi che deve attendere in prigione
    int tempo_rinascita_random = TEMPO_CASUALE_RINASCITA_MIN + rand() % (TEMPO_CASUALE_RINASCITA_MAX - TEMPO_CASUALE_RINASCITA_MIN + 1);
    // Dorme per x secondi * 1 (secondo = 1 mln di microsecondi)
    usleep(tempo_rinascita_random * 1000000);

    // Calcola la direzione random di uscita
    int direzione_uscita = rand() % N_DIREZIONI;

    // Definisce i numeri di muri che può attraversare (1 muro, quella della prigione)
    int numero_muri_attraversabili = 1;

    // Finché il fantasma non ha attraversato muri
    while(numero_muri_attraversabili == 1) {

        // Se attraversa un muro
        if(mappa[fantasmaMorto.y][fantasmaMorto.x].c != ' ')
            // Decrementa il contatore di muri attraversabili
            numero_muri_attraversabili--;

        // Aggiorna le coordinate in base alla direzione
        if(direzione_uscita == SU) 
            fantasmaMorto.y += -PASSO;
        else if(direzione_uscita == DESTRA) 
            fantasmaMorto.x += PASSO;
        else if(direzione_uscita == GIU) 
            fantasmaMorto.y += PASSO;
            else 
            fantasmaMorto.x += -PASSO;

        // Scrive la posizione aggiornata
        write(fd_coordinate_in, &fantasmaMorto, sizeof(fantasmaMorto)); 

        // Pausa
        usleep(3*pausa);
    }

    // Invia il valore intero FANTASMA_VIVO a gC
    i = FANTASMA_VIVO;
    write(fd_stato_vita_in, &i, sizeof(i));

    // Invoca la funzione fantasma: il gestore della morte del vecchio fantasma diventa il nuovo fantasma
    fantasma(mappa, fd_coordinate_in, fd_stato_vita_in, fd_stato_morto_out, fd_rimbalzo_out, i, AVVIA_SUBITO, fantasmaMorto);
}

/**
 * Restituisce il primo percorso che trova per arrivare all'ingresso della prigione a partire dalla posizione del fantasma morto
 * @param   mappa               Matrice di caselle che costituiscono la mappa
 * @param   percorso            Vettore di caselle che conterrà il percorso
 * @param   fantasmaMorto       Coordinate i dati del fantasma morto
 * @param   ingressoPrigione    Coordinate dell'ingresso della prigione
 * @return  L'indice dell'ultima casella utile (la casella finale, l'ingresso della prigione) + 1
 * */
int cercaPrimoPercorsoDisponibile(Casella mappa [MAX_Y][MAX_X], Coordinata percorso[MAX_Y * MAX_X], Oggetto fantasmaMorto, Coordinata ingressoPrigione) {
    // Inizializzo l'indice del vettore a 0
    int i = 0;
    // Definisco le coordinate inizio e fine
    Coordinata inizio, fine;
    // Inizializzo inizio con le coordinate del fantasma morto
    inizio.x = fantasmaMorto.x; inizio.y = fantasmaMorto.y;
    // inizializzo fine con le coordinate dell'ingresso della prigione
    fine = ingressoPrigione;
    // invoco la funzione ricorsiva aggiungi nodo al percorso, passandogli mappa, percorso, indirizzo dell'indice, coord. iniziali, coord. finali e direzione iniziale (-1 = nessuna direzione)
    aggiungiNodo(mappa, percorso, &i, inizio, fine, -1);
    // Restituisco l'indice (il vettore è stato modificato, ora contiene tutte le caselle per arrivare all'ingresso)
    return i;
}

/**
 * @param   mappa           Matrice di caselle che costituiscono la mappa
 * @param   percorso        Vettore di caselle che conterrà il percorso
 * @param   indice_vettore  Punta all'indice del vettore
 * @param   attuale         Coordinate della casella attuale
 * @param   fine            Coordinate della casella finale
 * @param   direzione_chiamante     direzione della funzione chiamante 
 * (esempio: data la casella in posizione (i,j), se si vuole aggiungere la casella alla sua destra, si deve invocare la funzione 
 *  aggiungiNodo() passando come direzione del chiamante: DESTRA)
 * */
bool aggiungiNodo(Casella mappa [MAX_Y][MAX_X], Coordinata percorso[MAX_Y * MAX_X], int* indice_vettore, Coordinata attuale, Coordinata fine, int direzione_chiamante) {

    // Definisco una coordinata vuota per quando devo cancellare una casella dal vettore
    Coordinata vuota; vuota.x = vuota.y = -1;
    
    // Aggiunge la casella attuale al percorso e aggiorna il contatore
    percorso[*indice_vettore] = attuale;
    (*indice_vettore)++;
    
    // Se la casella attuale è quella finale, restituisce vero
    if(attuale.x == fine.x && attuale.y == fine.y) 
        return true;
    
    // Se la casella attuale è illegale (è un muro), aggiorna il contatore, toglie la casella attuale dal percorso e restituisce falso 
    if(!mappa[attuale.y][attuale.x].attraversabile) {
        (*indice_vettore)--;
        percorso[*indice_vettore] = vuota;
        return false;
    }

    // Definisco dx e dy per gli spostamenti
    int dx, dy; dx = dy = 0;

    // DEF Direzione primaria: direzione che mi avvicina alla casella finale in linea retta
    // DEF Direzione secondaria: direzione che mi allontana dalla casella finale in linea retta
    // Le direzioni primarie e secondarie sono sempre diverse (e opposte)

    int direzione_x_prim, direzione_x_sec;
    // Se la casellla attuale è a destra di quella finale, la direzione principale (x) è SINISTRA, mentre quella secondaria è DESTRA
    if(attuale.x >= fine.x) {
        direzione_x_prim = SINISTRA; 
        direzione_x_sec = DESTRA; 
        dx = -PASSO;
    }
    // Viceversa se la casellla attuale è a sinistra di quella finale, la direzione principale (x) è DESTRA, mentre quella secondaria è SINISTRA
    else {
        direzione_x_prim = DESTRA; 
        direzione_x_sec = SINISTRA; 
        dx = PASSO;
    }

    // Analogo ragionamento per la direzione y
    int direzione_y_prim, direzione_y_sec; 
    if(attuale.y >= fine.y) {
        direzione_y_prim = SU;   direzione_y_sec = GIU; dy = -PASSO;
    } else {
        direzione_y_prim = GIU; direzione_y_sec = SU; dy = PASSO;
    }

    // Se la casella attuale è sotto
    if(attuale.y+dy == fine.y && attuale.x == fine.x ) {
        // Aggiorno le coordinate y del fantasma
        attuale.y += dy;
        // Aggiungo la casella sotto (che è la casella finale)
        aggiungiNodo(mappa, percorso, indice_vettore, attuale, fine, direzione_y_prim);
        // Restituisco true
        return true;
    }
 
    // Direzione x principale
    // Se la direzione che voglio controllare NON è l'opposto della direzione del chiamante (se il chiamante ha invocato la funzione passando la direzione DESTRA, 
    // con la funzione corrente non posso evocare la funzione aggiungendo la casella a SINISTRA)
    if(direzione_chiamante != direzione_x_sec) {
        // Aggiorno la coordinata x del fantasma
        attuale.x += dx;
        // Se la chiamata ricorsiva restituisce true, significa che ha trovato il percorso per arrivare alla casella finale, non c'è altro da fare
        if (aggiungiNodo(mappa, percorso, indice_vettore, attuale, fine, direzione_x_prim))
            // restituisce a sua volta true
            return true;
        // Altrimenti ripristina la coordinata x del fantasma
        attuale.x -= dx;
    }

    // Direzione y principale
    // Se la direzione che voglio controllare NON è l'opposto della direzione del chiamante (se il chiamante ha invocato la funzione passando la direzione SU, 
    // con la funzione corrente non posso evocare la funzione aggiungendo la casella GIU)
    if(direzione_chiamante != direzione_y_sec) {
        // Aggiorno la coordinata y del fantasma
        attuale.y += dy;
        // Se la chiamata ricorsiva restituisce true, significa che ha trovato il percorso per arrivare alla casella finale, non c'è altro da fare
        if(aggiungiNodo(mappa, percorso, indice_vettore, attuale, fine, direzione_y_prim))
            // restituisce a sua volta true
            return true;
        // Altrimenti ripristina la coordinata y del fantasma
        attuale.y -= dy;
    }

    // Direzione x secondaria
    // Se la direzione che voglio controllare NON è l'opposto della direzione del chiamante
    if(direzione_chiamante != direzione_x_prim) {
        // Aggiorno la coordinata x del fantasma
        attuale.x -= dx;
        // Se la chiamata ricorsiva restituisce true, significa che ha trovato il percorso per arrivare alla casella finale, non c'è altro da fare
        if(aggiungiNodo(mappa, percorso, indice_vettore, attuale, fine, direzione_x_sec))
            // restituisce a sua volta true
            return true;
        // Altrimenti ripristina la coordinata x del fantasma
        attuale.x += dx;
    }

    // Direzione y secondaria
    // Se la direzione che voglio controllare NON è l'opposto della direzione del chiamante
    if(direzione_chiamante != direzione_y_prim) {
        // Aggiorno la coordinata y del fantasma
        attuale.y -= dy;
        // Se la chiamata ricorsiva restituisce true, significa che ha trovato il percorso per arrivare alla casella finale, non c'è altro da fare
        if(aggiungiNodo(mappa, percorso, indice_vettore, attuale, fine, direzione_y_sec))
            // restituisce a sua volta true
            return true;
        // Altrimenti ripristina la coordinata y del fantasma
        attuale.x += dy;
    }

    // Se tutte le direzioni sono false (non portano alla casella finale) restituisce false
    return false;
}
