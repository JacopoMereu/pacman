#include "gestoreGioco.h"

void avviaGioco() {
    // indici per i cicli
    int i, j;

    // Crea la pipe per la trasmissione delle coordinate casuali (iniziali) dei fantasmi e di pacman
    int  fd_trasmette_coordinate_iniziali[NUMERO_FD];
    if (pipe(fd_trasmette_coordinate_iniziali) == ERRORE) {
        // caso ERRORE
        perror("Errore nella creazione della pipe");
        exit(ERRORE);
    }
   
    initscr();          // inizializza lo schermo
    noecho();           // non stampa  caratteri inseriti
    curs_set(0);        // nasconde il cursore

    Casella mappa [MAX_Y][MAX_X];       // Crea la variabile mappa
    inizializzaMappa(mappa);            // Inizializza la mappa

    srand(time(NULL));        // Inizializza il seed per i numeri casuali

    // Generazione delle coordinate casuali 
    pid_t pid_generaCoordinate;     // conterrà il pid del processo che genera le coordinate
    Oggetto coordinateOggettiIniziali[N_FANTASMI + 1];  // Numero fantasmi + numero pacman (1)
    pid_generaCoordinate = fork();     // fork
    // caso ERRORE
    if (pid_generaCoordinate == ERRORE) {
        perror("Errore fork N.1 (Main -> generaCoordinate)");
        exit(ERRORE);
    // caso FIGLIO
    } else if (pid_generaCoordinate == FIGLIO) {
        close(fd_trasmette_coordinate_iniziali[DESCRITTORE_LETTURA]);       // Chiude il lato della pipe che non deve utilizzare
        generaCoordinate(mappa, fd_trasmette_coordinate_iniziali[DESCRITTORE_SCRITTURA], N_FANTASMI+1); // generaCoordinata trasmettera N_FANTASMI+1 coordinate
        // sulla pipe
        close(fd_trasmette_coordinate_iniziali[DESCRITTORE_SCRITTURA]);    // Chiude il lato della pipe che non deve più utilizzare (ha già trasmesso)
        exit(NORMALE);      // Il processo termina
    // caso PADRE
    } else ;
    close(fd_trasmette_coordinate_iniziali[DESCRITTORE_SCRITTURA]); // Chude il descrittore del file in scrittura (il padre deve leggere le coordinate)

    inizializzaColori();      // Inizializza le combinazioni di colori
    sfondoTerminale();        // Terminale nero
    refresh();      // Refresh del terminale

    // Crea la pipe per la trasmissione delle coordinate in tempo reale
    int fd_trasmette_coordinate_reali[NUMERO_FD];
    if (pipe(fd_trasmette_coordinate_reali) == ERRORE) {
        // caso ERRORE
        perror("Errore nella creazione della pipe coordinate reali");
        exit(ERRORE);
    }

    // Aspetto che termini il processo, sono essenziali le coordinate per continuare
    while(wait(NULL) > 0) 
        ;
    // Leggo le coordinate dalla pipe e le salvo nel vettore delle coordinate iniziali
    for(i = 0; i < N_FANTASMI + 1; i++)
        read(fd_trasmette_coordinate_iniziali[DESCRITTORE_LETTURA], &coordinateOggettiIniziali[i], sizeof(coordinateOggettiIniziali[i]));

    // Chiudo il fd in lettura (non serve più la pipe, tutti i fd son stati chiusi)
    close(fd_trasmette_coordinate_iniziali[DESCRITTORE_LETTURA]);

    // Setto la variabile booleana fantasmaNascosto a vero nelle caselle iniziali dei fantasmi
    for(i=0; i<N_FANTASMI; i++)
        mappa[coordinateOggettiIniziali[i+1].y][coordinateOggettiIniziali[i+1].x].fantasmaNascosto = true;

   
    int uscita;         // valore di uscita: vittoria o sconfitta 
    pid_t pid_pacman;   // conterrà il pid del processo pacman
    pid_t pid_fantasmi[N_FANTASMI]; // conterrà il pid dei fantasmi

    // pipes che informano dello "stato vitale" dei fantasmi
    int fd_morte_fantasmi[N_FANTASMI][NUMERO_FD];   // cG informa il fantasma che è morto
    int fd_vita_fantasmi[N_FANTASMI][NUMERO_FD];    // il fantasma informa cG che è "di nuovo operativo" (è andato in prigione,
    // ha aspettato il tempo random, è uscito dal rettangolo ed è di nuovo operativo)

    // pipes per informare i fantasmi di invertire la direzione (caso rimbalzo tra fantasmi)
    int fd_rimbalzo_fantasmi[N_FANTASMI][NUMERO_FD];
    // pipes per gestire l'attivazione dei fantasmi (pacman mangia il semino che nasconde il fantasma, pacman si muove, il fantasma compare,
    // dopo un tempo random tra 1 e 3 secondi il fantasma inizierà a muoversi)
    int fd_attiva_fantasmi[N_FANTASMI][NUMERO_FD]; 

    
     // Creo il processo pacman
    switch (pid_pacman = fork() ) {
        case ERRORE:
            perror("Errore fork (Main -> Pacman)");
            exit(ERRORE);
        case FIGLIO:
            close(fd_trasmette_coordinate_reali[DESCRITTORE_LETTURA]);      // pacman deve SCRIVERE le coordinate sulla pipe, chiudo il fd in LETTURA
            // Salva l'ID di pacman in coordinate        
            coordinateOggettiIniziali[0].id_oggetto = 0;
            // invoca la funzione pacman
            pacman(mappa, fd_trasmette_coordinate_reali[DESCRITTORE_SCRITTURA], coordinateOggettiIniziali[0]); 
            exit(NORMALE);      
            break;
        default:        // CASO PADRE
                // Creo gli N fantasmi               
                for (i = 0; i < N_FANTASMI; i++) {
                    // creazione pipe rimbalzi i-esima
                    if (pipe(fd_rimbalzo_fantasmi[i]) == ERRORE) {
                        //      caso ERRORE
                        perror("Errore nella creazione della pipe rimbalzi fantasmi");
                        exit(ERRORE);
                    }
                    // modifica del fd (lettura) a non bloccante: pipe rimbalzi i-esima
                    if (fcntl(fd_rimbalzo_fantasmi[i][DESCRITTORE_LETTURA], F_SETFL, O_NONBLOCK) < 0) {
                        //      caso ERRORE
                        perror("Errore nel cambiamento del fd in lettura (non bloccante) della pipe rimbalzi fantasmi");
                        exit(ERRORE);
                    }

                    // creazione pipe attiva i-esima
                    if (pipe(fd_attiva_fantasmi[i]) == ERRORE) {
                        // caso ERRORE
                        perror("Errore nella creazione della pipe attiva fantasmi");
                        exit(ERRORE);
                    }
                    
                    // creazione pipe i-esima che informa i fantasmi della loro morte
                    if (pipe(fd_morte_fantasmi[i]) == ERRORE) {
                        // caso ERRORE
                        perror("Errore nella creazione della pipe morte fantasmi");
                        exit(ERRORE);
                    }
                    // modifica del fd (lettura) a non bloccante: pipe che informa i fantasmi della loro morte i-esima 
                    if (fcntl(fd_morte_fantasmi[i][DESCRITTORE_LETTURA], F_SETFL, O_NONBLOCK) < 0) {
                        // caso ERRORE
                        perror("Errore nel cambiamento del fd in lettura (non bloccante) della pipe morte fantasmi");
                        exit(ERRORE);
                    }

                    // creazione pipe i-esima che informa il processo gC della "rinascita" dei fantasma
                    if (pipe(fd_vita_fantasmi[i]) == ERRORE) {
                        // caso ERRORE
                        perror("Errore nella creazione della pipe vita fantasmi");
                        exit(ERRORE);
                    }
                    // modifica del fd (lettura) a non bloccante: pipe che informa gC della "rinascita" del fantasma
                    if (fcntl(fd_vita_fantasmi[i][DESCRITTORE_LETTURA], F_SETFL, O_NONBLOCK) < 0) {
                        // caso ERRORE
                        perror("Errore nel cambiamento del fd in lettura (non bloccante) della pipe vita fantasmi");
                        exit(ERRORE);
                    }

                    // fork per la creazione del fantasma i-esimo
                    pid_fantasmi[i] = fork();
                        // caso ERRORE
                    if (pid_fantasmi[i] == ERRORE) {
                        perror("Errore forkn(Padre -> Fantasma[?])");
                        exit(ERRORE);
                        // caso FIGLIO
                    } else if (pid_fantasmi[i] == FIGLIO) {
                        // il fantasma deve SCRIVERE le coordinate sulla pipe, chiudo il fd in LETTURA
                        close(fd_trasmette_coordinate_reali[DESCRITTORE_LETTURA]);
                        // il fantasma deve LEGGERE l'ordine di invertire le direzioni dalla pipe, chiudo il fd in SCRITTURA
                        close(fd_rimbalzo_fantasmi[i][DESCRITTORE_SCRITTURA]);
                        // il fantasma deve LEGGERE se è morto dalla pipe, chiudo il fd in SCRITTURA
                        close(fd_morte_fantasmi[i][DESCRITTORE_SCRITTURA]);
                        // il fantasma deve SCRIVERE sulla pipe se è vivo, chiudo il descrittore in LETTURA
                        close(fd_vita_fantasmi[i][DESCRITTORE_LETTURA]);
                        // il fantasma deve LEGGERE dalla pipe se si può attivare, chiudo il descrittore in SCRITTURA
                        close(fd_attiva_fantasmi[i][DESCRITTORE_SCRITTURA]);

                        // Chiudo le pipe "precedenti" (a ogni iterazione del for viene creata una pipe)
                        for (j = 0; j < i; j++) {
                            // Chiudo la pipe rimbalzi, 
                            // i lati in LETTURA stato già stati chiusi dal processo PADRE
                            // Chiudo i lati in SCRITTURA
                            close(fd_rimbalzo_fantasmi[j][DESCRITTORE_SCRITTURA]);
                            // Chiudo la pipe morte fantasmi, 
                            // i lati in LETTURA stato già stati chiusi dal processo PADRE
                            // Chiudo i lati in SCRITTURA
                            close(fd_morte_fantasmi[j][DESCRITTORE_SCRITTURA]);
                            // Chiudo la pipe vita fantasmi, 
                            // i lati in SCRITTURA stato già stati chiusi dal processo PADRE
                            // Chiudo i lati in LETTURA
                            close(fd_vita_fantasmi[j][DESCRITTORE_LETTURA]);
                            // Chiudo la pipe attiva fantasmi, 
                            // i lati in LETTURA stato già stati chiusi dal processo PADRE
                            // Chiudo i lati in SCRITTURA
                            close(fd_attiva_fantasmi[j][DESCRITTORE_SCRITTURA]);
                        }
                        
                        // Salvo l'ID del fantasma in coordinate
                        coordinateOggettiIniziali[i+1].id_oggetto = i+1;
                        // Avvio il fantasma i-esimo
                        // Passo la mappa, i fd delle pipe che gli servono, la modalità d'avvio (finché pacman non li scopre non si muovono)
                        // e le coordinate iniziali (+ id fantasma)
                        fantasma(mappa, fd_trasmette_coordinate_reali[DESCRITTORE_SCRITTURA], fd_vita_fantasmi[i][DESCRITTORE_SCRITTURA], 
                        fd_morte_fantasmi[i][DESCRITTORE_LETTURA], fd_rimbalzo_fantasmi[i][DESCRITTORE_LETTURA], 
                        fd_attiva_fantasmi[i][DESCRITTORE_LETTURA], AVVIA_NASCOSTO, coordinateOggettiIniziali[i+1]);
                    } else {
                        // Chiude il descrittore in LETTURA della pipe i-esima usata per trasmettere le informazioni dei rimbalzi,
                        // il PADRE deve poter SCRIVERE sulla pipe se il fantasma deve invertire la direzione (rimbalzo)
                        close(fd_rimbalzo_fantasmi[i][DESCRITTORE_LETTURA]);
                        // Chiude il descrittore in LETTURA della pipe i-esima morte fantasmi,
                        // il PADRE deve SCRIVERE sulla pipe se un fantasma muore
                        close(fd_morte_fantasmi[i][DESCRITTORE_LETTURA]);
                        // Chiude il descrittore in SCRITTURA della pipe i-esima vita fantasmi,
                        // il PADRE deve LEGGERE dalla pipe se un fantasma è di nuovo operativo
                        close(fd_vita_fantasmi[i][DESCRITTORE_SCRITTURA]);
                        // Chiude il descrittore in LETTURA della pipe i-esima attiva fantasmi,
                        // il PADRE deve SCRIVERE sulla pipe quando un fantasma nascosto deve attivarsi
                        close(fd_attiva_fantasmi[i][DESCRITTORE_LETTURA]);
                        }
                }
            // Alla fine del ciclo il PADRE chiude il descrittore in SCRITTURA della pipe usata per trasmettere le coordinate reali degli oggetti
            // il PADRE deve poter leggere le posizioni dalla pipe
            close(fd_trasmette_coordinate_reali[DESCRITTORE_SCRITTURA]);
            
            // Vettori tmp in cui memorizzo i fd delle pipe create per comunicare coi fantasmi
            int vec_fd_rimbalzi_in[N_FANTASMI], vec_fd_morte_fantasma_in[N_FANTASMI], 
                vec_fd_vita_fantasma_out[N_FANTASMI], vec_fd_attiva_in[N_FANTASMI];
            
            for(i=0;i<N_FANTASMI;i++) {
                vec_fd_rimbalzi_in[i] = fd_rimbalzo_fantasmi[i][DESCRITTORE_SCRITTURA];
                vec_fd_morte_fantasma_in[i] = fd_morte_fantasmi[i][DESCRITTORE_SCRITTURA];
                vec_fd_vita_fantasma_out[i] = fd_vita_fantasmi[i][DESCRITTORE_LETTURA];
                vec_fd_attiva_in[i] = fd_attiva_fantasmi[i][DESCRITTORE_SCRITTURA];
            }
            // Chiamo la funzione controlli passandogli la mappa, le coordinate iniziali degli oggetti e i vari fd
            // La funzione restituisce un valore intero diverso a seconda che l'utente abbia vinto o perso la partita
            uscita = controlli(mappa, coordinateOggettiIniziali, fd_trasmette_coordinate_reali[DESCRITTORE_LETTURA], vec_fd_rimbalzi_in, 
                vec_fd_vita_fantasma_out, vec_fd_morte_fantasma_in, vec_fd_attiva_in);
                
        
        // Stampa messaggio vittoria o sconfitta
        if(uscita == VITTORIA)
            mvprintw(getmaxy(stdscr)/2-1,(getmaxx(stdscr)/2)-5, "HAI VINTO!!!");
        else
            mvprintw(getmaxy(stdscr)/2-1,(getmaxx(stdscr)/2)-5, "hAi PeRsO!1!1!");
        
        // Bisogna premere invio per uscire
        mvprintw(getmaxy(stdscr)/2,(getmaxx(stdscr)/2)-11,"premi invio per uscire");
        // Refresh del terminale
        refresh();
        // Finché l'utente non preme invio non uscire
        while(getch() != '\n')
            ;
        // chiude la finestra
        endwin();
    }
}

/**
 * Crea staticamente la mappa e assegna in modo dinamico i valori delle variabili booleane delle caselle
 * 
 * @param  mappa        Matrice di caselle che costituiscono la mappa
 * */
void inizializzaMappa(Casella mappa [MAX_Y][MAX_X]) {
    // Indici righe e colonne
    int i, j;

    //// inizializza la mappa coi "semini" ////
    for(i = 0; i < MAX_Y; i++)
        for(j = 0; j < MAX_X; j++)
            mappa[i][j].c = SIMBOLO_SEME;

    // inizializza i bordi verticali (dx e sx)
    for (i = 0; i < MAX_Y; i++)
            mappa[i][0].c = mappa[i][MAX_X-1].c = ACS_VLINE; 

    // inizializza i bordi orizzontali (su e giù)
    for (i = 0; i < MAX_X; i++) 
        mappa[0][i].c = mappa[MAX_Y-1][i].c = ACS_HLINE; 
    
    //// inizializza gli angoli ////
    // alto a sinistra
    mappa[0][0].c = ACS_ULCORNER;
    // alto a destra
    mappa[0][MAX_X-1].c = ACS_URCORNER;
    // basso a sinistra
    mappa[MAX_Y-1][0].c = ACS_LLCORNER;
    // basso a destra
    mappa[MAX_Y-1][MAX_X-1].c = ACS_LRCORNER;

    //// muri interni ////
    // porta in alto a sx
    mappa[5][0].c = ACS_LLCORNER; mappa[6][0].c = ACS_ULCORNER; mappa[5][1].c = ACS_HLINE;
    mappa[5][2].c = ACS_URCORNER; mappa[6][1].c = ACS_HLINE; mappa[6][2].c = ACS_LRCORNER;
    mappa[7][0].c = SIMBOLO_PORTA;
    mappa[8][0].c = ACS_LLCORNER; mappa[9][0].c = ACS_ULCORNER; mappa[8][1].c = ACS_HLINE;
    mappa[8][2].c = ACS_URCORNER; mappa[9][1].c = ACS_HLINE; mappa[9][2].c = ACS_LRCORNER;
    
    // L in alto a sx
    mappa[2][2].c = ACS_ULCORNER; mappa[2][3].c = ACS_HLINE; mappa[2][4].c = ACS_HLINE; mappa[2][5].c = ACS_HLINE; mappa[2][6].c = ACS_URCORNER;
    mappa[3][2].c = ACS_LLCORNER; mappa[3][3].c = ACS_HLINE; mappa[3][4].c = ACS_URCORNER; mappa[3][5].c = ' '; mappa[3][6].c = ACS_VLINE;
    mappa[4][4].c = ACS_VLINE; mappa[4][5].c = ' '; mappa[4][6].c = ACS_VLINE;
    mappa[5][4].c = ACS_VLINE; mappa[5][5].c = ' '; mappa[5][6].c = ACS_VLINE;
    mappa[6][4].c = ACS_LLCORNER; mappa[6][5].c = ACS_HLINE; mappa[6][6].c = ACS_LRCORNER;

    // pilastro tra L (A_S) e M (A_C)
    mappa[0][8].c = ACS_URCORNER; mappa[0][9].c = ' '; mappa[0][10].c = ACS_ULCORNER;
    mappa[1][8].c = ACS_VLINE; mappa[1][9].c = ' '; mappa[1][10].c = ACS_VLINE;
    mappa[2][8].c = ACS_VLINE; mappa[2][9].c = ' ';  mappa[2][10].c = ACS_VLINE;
    mappa[3][8].c = ACS_LLCORNER; mappa[3][9].c = ACS_HLINE; mappa[3][10].c = ACS_LRCORNER;

    // M in alto al centro
    mappa[2][12].c = ACS_ULCORNER; mappa[2][13].c = ACS_HLINE; mappa[2][14].c = ACS_HLINE; mappa[2][15].c = ACS_HLINE; 
        mappa[2][16].c = ACS_HLINE; mappa[2][17].c = ACS_HLINE; mappa[2][18].c = ACS_HLINE;
        mappa[2][19].c = ACS_HLINE; mappa[2][20].c = ACS_HLINE; mappa[2][21].c = ACS_HLINE; mappa[2][22].c = ACS_URCORNER;
    mappa[3][12].c = ACS_VLINE; mappa[3][13].c = ' '; mappa[3][14].c = ACS_ULCORNER; mappa[3][15].c = ACS_HLINE; 
        mappa[3][16].c = ACS_HLINE; mappa[3][17].c = ACS_HLINE; mappa[3][18].c = ACS_HLINE; mappa[3][19].c = ACS_HLINE;
        mappa[3][20].c = ACS_URCORNER; mappa[3][21].c = ' '; mappa[3][22].c = ACS_VLINE;
    mappa[4][12].c = ACS_VLINE; mappa[4][13].c = ' '; mappa[4][14].c = ACS_VLINE; 
        mappa[4][20].c = ACS_VLINE; mappa[4][21].c = ' '; mappa[4][22].c = ACS_VLINE;
    mappa[5][12].c = ACS_VLINE; mappa[5][13].c = ' '; mappa[5][14].c = ACS_VLINE;
        mappa[5][20].c = ACS_VLINE; mappa[5][21].c = ' '; mappa[5][22].c = ACS_VLINE;
    mappa[6][12].c = ACS_VLINE; mappa[6][13].c = ' '; mappa[6][14].c = ACS_VLINE;
        mappa[6][20].c = ACS_VLINE; mappa[6][21].c = ' '; mappa[6][22].c = ACS_VLINE;
    mappa[7][12].c = ACS_LLCORNER; mappa[7][13].c = ACS_HLINE; mappa[7][14].c = ACS_LRCORNER;
        mappa[7][20].c = ACS_LLCORNER; mappa[7][21].c = ACS_HLINE; mappa[7][22].c = ACS_LRCORNER;

    // pilastro tra L (A_D) e M (A_C)
    mappa[0][24].c = ACS_URCORNER; mappa[0][25].c = ' '; mappa[0][26].c = ACS_ULCORNER;
    mappa[1][24].c = ACS_VLINE; mappa[1][25].c = ' '; mappa[1][26].c = ACS_VLINE;
    mappa[2][24].c = ACS_VLINE; mappa[2][25].c = ' ';  mappa[2][26].c = ACS_VLINE;
    mappa[3][24].c = ACS_LLCORNER; mappa[3][25].c = ACS_HLINE; mappa[3][26].c = ACS_LRCORNER;

    // L in alto a dx
    mappa[2][MAX_X-3].c = ACS_URCORNER; mappa[2][MAX_X-4].c = ACS_HLINE; mappa[2][MAX_X-5].c = ACS_HLINE; 
        mappa[2][MAX_X-6].c = ACS_HLINE; mappa[2][MAX_X-7].c = ACS_ULCORNER;
    mappa[3][MAX_X-3].c = ACS_LRCORNER; mappa[3][MAX_X-4].c = ACS_HLINE; mappa[3][MAX_X-5].c = ACS_ULCORNER; 
        mappa[3][MAX_X-6].c = ' '; mappa[3][MAX_X-7].c = ACS_VLINE;
    mappa[4][MAX_X-5].c = ACS_VLINE; mappa[4][MAX_X-6].c = ' '; mappa[4][MAX_X-7].c = ACS_VLINE;
    mappa[5][MAX_X-5].c = ACS_VLINE; mappa[5][MAX_X-6].c = ' '; mappa[5][MAX_X-7].c = ACS_VLINE;
    mappa[6][MAX_X-5].c = ACS_LRCORNER; mappa[6][MAX_X-6].c = ACS_HLINE; mappa[6][MAX_X-7].c = ACS_LLCORNER;

    // porta in alto a dx
    mappa[5][MAX_X-1].c = ACS_LRCORNER; mappa[6][MAX_X-1].c = ACS_URCORNER; mappa[5][MAX_X-2].c = ACS_HLINE;
    mappa[5][MAX_X-3].c = ACS_ULCORNER; mappa[6][MAX_X-2].c = ACS_HLINE; mappa[6][MAX_X-3].c = ACS_LLCORNER;
    mappa[7][MAX_X-1].c = SIMBOLO_PORTA;
    mappa[8][MAX_X-1].c = ACS_LRCORNER; mappa[9][MAX_X-1].c = ACS_URCORNER; mappa[8][MAX_X-2].c = ACS_HLINE;
    mappa[8][MAX_X-3].c = ACS_ULCORNER; mappa[9][MAX_X-2].c = ACS_HLINE; mappa[9][MAX_X-3].c = ACS_LLCORNER;

    // T in alto a sinistra (tra la porta in alto a sx e la T A_C)
    mappa[5][8].c = ACS_ULCORNER; mappa[5][9].c = ACS_HLINE; mappa[5][10].c = ACS_URCORNER;
    mappa[6][8].c = ACS_VLINE; mappa[6][9].c = ' '; mappa[6][10].c = ACS_VLINE;
    mappa[7][8].c = ACS_VLINE; mappa[7][9].c = ' '; mappa[7][10].c = ACS_VLINE;
    mappa[8][8].c = ACS_LRCORNER; mappa[8][9].c = ' '; mappa[8][10].c = ACS_VLINE;
        mappa[8][4].c = ACS_ULCORNER; mappa[8][5].c = ACS_HLINE; mappa[8][6].c = ACS_HLINE; mappa[8][7].c = ACS_HLINE;
    mappa[9][8].c = ACS_URCORNER; mappa[9][9].c = ' '; mappa[9][10].c = ACS_VLINE;
        mappa[9][4].c = ACS_LLCORNER; mappa[9][5].c = ACS_HLINE; mappa[9][6].c = ACS_HLINE; mappa[9][7].c = ACS_HLINE;
    mappa[10][8].c = ACS_VLINE; mappa[10][9].c = ' '; mappa[10][10].c = ACS_VLINE;
    mappa[11][8].c = ACS_VLINE; mappa[11][9].c = ' '; mappa[11][10].c = ACS_VLINE;
    mappa[12][8].c = ACS_VLINE; mappa[12][9].c = ' '; mappa[12][10].c = ACS_VLINE;
    mappa[13][8].c = ACS_LLCORNER; mappa[13][9].c = ACS_HLINE; mappa[13][10].c = ACS_LRCORNER;

    // T in alto al centro
    mappa[5][16].c = ACS_ULCORNER; mappa[5][17].c = ACS_HLINE;  mappa[5][18].c = ACS_URCORNER; 
    mappa[6][16].c = ACS_VLINE; mappa[6][17].c = ' ';  mappa[6][18].c = ACS_VLINE; 
    mappa[7][16].c = ACS_VLINE; mappa[7][17].c = ' ';  mappa[7][18].c = ACS_VLINE; 
    mappa[8][16].c = ACS_VLINE; mappa[8][17].c = ' ';  mappa[8][18].c = ACS_VLINE; 
    mappa[9][12].c = ACS_ULCORNER; mappa[9][13].c = ACS_HLINE; mappa[9][14].c = ACS_HLINE; mappa[9][15].c = ACS_HLINE;
        mappa[9][16].c = ACS_LRCORNER; mappa[9][17].c = ' '; mappa[9][18].c = ACS_LLCORNER; 
        mappa[9][19].c = ACS_HLINE; mappa[9][20].c = ACS_HLINE; mappa[9][21].c = ACS_HLINE; mappa[9][22].c = ACS_URCORNER;
    mappa[10][12].c = ACS_LLCORNER; mappa[10][13].c = ACS_HLINE; mappa[10][14].c = ACS_HLINE; mappa[10][15].c = ACS_HLINE;
        mappa[10][16].c = ACS_HLINE; mappa[10][17].c = ACS_HLINE; mappa[10][18].c = ACS_HLINE; 
        mappa[10][19].c = ACS_HLINE; mappa[10][20].c = ACS_HLINE; mappa[10][21].c = ACS_HLINE; mappa[10][22].c = ACS_LRCORNER;

   
    // T in alto a destra
    mappa[5][MAX_X-11].c = ACS_ULCORNER; mappa[5][MAX_X-10].c = ACS_HLINE; mappa[5][MAX_X-9].c = ACS_URCORNER;
    mappa[6][MAX_X-11].c = ACS_VLINE; mappa[6][MAX_X-10].c = ' '; mappa[6][MAX_X-9].c = ACS_VLINE;
    mappa[7][MAX_X-11].c = ACS_VLINE; mappa[7][MAX_X-10].c = ' '; mappa[7][MAX_X-9].c = ACS_VLINE;
    mappa[8][MAX_X-11].c = ACS_VLINE; mappa[8][MAX_X-10].c = ' '; mappa[8][MAX_X-9].c = ACS_LLCORNER;
        mappa[8][MAX_X-5].c = ACS_URCORNER; mappa[8][MAX_X-6].c = ACS_HLINE; mappa[8][MAX_X-7].c = ACS_HLINE; mappa[8][MAX_X-8].c = ACS_HLINE;
    mappa[9][MAX_X-11].c = ACS_VLINE; mappa[9][MAX_X-10].c = ' '; mappa[9][MAX_X-9].c = ACS_ULCORNER;
        mappa[9][MAX_X-5].c = ACS_LRCORNER; mappa[9][MAX_X-6].c = ACS_HLINE; mappa[9][MAX_X-7].c = ACS_HLINE; mappa[9][MAX_X-8].c = ACS_HLINE;
    mappa[10][MAX_X-11].c = ACS_VLINE; mappa[10][MAX_X-10].c = ' '; mappa[10][MAX_X-9].c = ACS_VLINE;
    mappa[11][MAX_X-11].c = ACS_VLINE; mappa[11][MAX_X-10].c = ' '; mappa[11][MAX_X-9].c = ACS_VLINE;
    mappa[12][MAX_X-11].c = ACS_VLINE; mappa[12][MAX_X-10].c = ' '; mappa[12][MAX_X-9].c = ACS_VLINE;
    mappa[13][MAX_X-11].c = ACS_LLCORNER; mappa[13][MAX_X-10].c = ACS_HLINE; mappa[13][MAX_X-9].c = ACS_LRCORNER;

    // L centro sinistra 1
    mappa[11][2].c = ACS_ULCORNER; mappa[11][3].c = ACS_HLINE; mappa[11][4].c = ACS_HLINE; 
        mappa[11][5].c = ACS_HLINE; mappa[11][6].c = ACS_URCORNER;
    mappa[12][2].c = ACS_LLCORNER; mappa[12][3].c = ACS_HLINE; mappa[12][4].c = ACS_URCORNER; 
        mappa[12][5].c = ' '; mappa[12][6].c = ACS_VLINE;
    mappa[13][4].c = ACS_VLINE; mappa[13][5].c = ' '; mappa[13][6].c = ACS_VLINE;
    mappa[14][4].c = ACS_VLINE; mappa[14][5].c = ' '; mappa[14][6].c = ACS_VLINE;
    mappa[15][4].c = ACS_VLINE; mappa[15][5].c = ' '; mappa[15][6].c = ACS_VLINE;
    mappa[16][4].c = ACS_LLCORNER; mappa[16][5].c = ACS_HLINE; mappa[16][6].c = ACS_LRCORNER;

    // Prigione
    mappa[12][12].c = ACS_ULCORNER; mappa[12][13].c = ACS_HLINE; mappa[12][14].c = ACS_HLINE; mappa[12][15].c = ACS_HLINE;
        mappa[12][16].c = ACS_HLINE; mappa[12][17].c = SIMBOLO_INGRESSO_PRIGIONE; mappa[12][18].c = ACS_HLINE; 
        mappa[12][19].c = ACS_HLINE; mappa[12][20].c = ACS_HLINE; mappa[12][21].c = ACS_HLINE; mappa[12][22].c = ACS_URCORNER;
    mappa[13][12].c = ACS_VLINE; mappa[13][22].c = ACS_VLINE;     
    mappa[14][12].c = ACS_VLINE; mappa[14][22].c = ACS_VLINE;
    mappa[15][12].c = ACS_VLINE; mappa[15][22].c = ACS_VLINE;
    for(i=13; i<22;i++)
        mappa[13][i].c = mappa[14][i].c =mappa[15][i].c = ' ';
    mappa[16][12].c = ACS_LLCORNER; mappa[16][13].c = ACS_HLINE; mappa[16][14].c = ACS_HLINE;
        mappa[16][15].c = ACS_HLINE; mappa[16][16].c = ACS_HLINE; mappa[16][17].c = ACS_HLINE; 
        mappa[16][18].c = ACS_HLINE; mappa[16][19].c = ACS_HLINE; mappa[16][20].c = ACS_HLINE; 
        mappa[16][21].c = ACS_HLINE; mappa[16][22].c = ACS_LRCORNER;

    // L centro destra 1
    mappa[11][MAX_X-3].c = ACS_URCORNER; mappa[11][MAX_X-4].c = ACS_HLINE; mappa[11][MAX_X-5].c = ACS_HLINE; 
        mappa[11][MAX_X-6].c = ACS_HLINE; mappa[11][MAX_X-7].c = ACS_ULCORNER;
    mappa[12][MAX_X-3].c = ACS_LRCORNER; mappa[12][MAX_X-4].c = ACS_HLINE; mappa[12][MAX_X-5].c = ACS_ULCORNER; 
        mappa[12][MAX_X-6].c = ' '; mappa[12][MAX_X-7].c = ACS_VLINE;
    mappa[13][MAX_X-5].c = ACS_VLINE; mappa[13][MAX_X-6].c = ' '; mappa[13][MAX_X-7].c = ACS_VLINE;
    mappa[14][MAX_X-5].c = ACS_VLINE; mappa[14][MAX_X-6].c = ' '; mappa[14][MAX_X-7].c = ACS_VLINE;
    mappa[15][MAX_X-5].c = ACS_VLINE; mappa[15][MAX_X-6].c = ' '; mappa[15][MAX_X-7].c = ACS_VLINE;
    mappa[16][MAX_X-5].c = ACS_LRCORNER; mappa[16][MAX_X-6].c = ACS_HLINE; mappa[16][MAX_X-7].c = ACS_LLCORNER;

    // Rientro centro sinistra
    mappa[14][0].c = ACS_LLCORNER; mappa[14][1].c = ACS_HLINE; mappa[14][2].c = ACS_URCORNER;
    mappa[15][0].c = ' '; mappa[15][1].c = ' '; mappa[15][2].c = ACS_VLINE;
    mappa[16][0].c = ' '; mappa[16][1].c = ' '; mappa[16][2].c = ACS_VLINE;
    mappa[17][0].c = ' '; mappa[17][1].c = ' '; mappa[17][2].c = ACS_VLINE;
    mappa[18][0].c = ' '; mappa[18][1].c = ' '; mappa[18][2].c = ACS_VLINE;
    mappa[19][0].c = ACS_ULCORNER; mappa[19][1].c = ACS_HLINE; mappa[19][2].c = ACS_LRCORNER;

    // L centro sinistra 2
    mappa[15][8].c = ACS_ULCORNER; mappa[15][9].c = ACS_HLINE; mappa[15][10].c = ACS_URCORNER;
    mappa[16][8].c = ACS_VLINE; mappa[16][9].c = ' '; mappa[16][10].c = ACS_VLINE;
    mappa[17][8].c = ACS_VLINE; mappa[17][9].c = ' '; mappa[17][10].c = ACS_VLINE;
    mappa[18][4].c = ACS_ULCORNER; mappa[18][5].c = ACS_HLINE; mappa[18][6].c = ACS_HLINE; mappa[18][7].c = ACS_HLINE; 
        mappa[18][8].c = ACS_LRCORNER; mappa[18][9].c = ' '; mappa[18][10].c = ACS_VLINE;
    mappa[19][4].c = ACS_LLCORNER; mappa[19][5].c = ACS_HLINE; mappa[19][6].c = ACS_HLINE; mappa[19][7].c = ACS_HLINE;
        mappa[19][8].c = ACS_HLINE; mappa[19][9].c = ACS_HLINE; mappa[19][10].c = ACS_LRCORNER;

    // T in basso al centro
    mappa[MAX_Y-9][16].c = ACS_LLCORNER; mappa[MAX_Y-9][17].c = ACS_HLINE;  mappa[MAX_Y-9][18].c = ACS_LRCORNER; 
    mappa[MAX_Y-10][16].c = ACS_VLINE; mappa[MAX_Y-10][17].c = ' ';  mappa[MAX_Y-10][18].c = ACS_VLINE; 
    mappa[MAX_Y-11][16].c = ACS_VLINE; mappa[MAX_Y-11][17].c = ' ';  mappa[MAX_Y-11][18].c = ACS_VLINE; 
    mappa[MAX_Y-12][12].c = ACS_LLCORNER; mappa[MAX_Y-12][13].c = ACS_HLINE; mappa[MAX_Y-12][14].c = ACS_HLINE; mappa[MAX_Y-12][15].c = ACS_HLINE;
        mappa[MAX_Y-12][16].c = ACS_URCORNER; mappa[MAX_Y-12][17].c = ' '; mappa[MAX_Y-12][18].c = ACS_ULCORNER; 
        mappa[MAX_Y-12][19].c = ACS_HLINE; mappa[MAX_Y-12][20].c = ACS_HLINE; mappa[MAX_Y-12][21].c = ACS_HLINE; mappa[MAX_Y-12][22].c = ACS_LRCORNER;
    mappa[MAX_Y-13][12].c = ACS_ULCORNER; mappa[MAX_Y-13][13].c = ACS_HLINE; mappa[MAX_Y-13][14].c = ACS_HLINE; mappa[MAX_Y-13][15].c = ACS_HLINE;
        mappa[MAX_Y-13][16].c = ACS_HLINE; mappa[MAX_Y-13][17].c = ACS_HLINE; mappa[MAX_Y-13][18].c = ACS_HLINE; 
        mappa[MAX_Y-13][19].c = ACS_HLINE; mappa[MAX_Y-13][20].c = ACS_HLINE; mappa[MAX_Y-13][21].c = ACS_HLINE; mappa[MAX_Y-13][22].c = ACS_URCORNER;

    // L centro sinistra 2
    mappa[15][MAX_X-9].c = ACS_URCORNER; mappa[15][MAX_X-10].c = ACS_HLINE; mappa[15][MAX_X-11].c = ACS_ULCORNER;
    mappa[16][MAX_X-9].c = ACS_VLINE; mappa[16][MAX_X-10].c = ' '; mappa[16][MAX_X-11].c = ACS_VLINE;
    mappa[17][MAX_X-9].c = ACS_VLINE; mappa[17][MAX_X-10].c = ' '; mappa[17][MAX_X-11].c = ACS_VLINE;
    mappa[18][MAX_X-5].c = ACS_URCORNER; mappa[18][MAX_X-6].c = ACS_HLINE; mappa[18][MAX_X-7].c = ACS_HLINE; mappa[18][MAX_X-8].c = ACS_HLINE; 
        mappa[18][MAX_X-9].c = ACS_LLCORNER; mappa[18][MAX_X-10].c = ' '; mappa[18][MAX_X-11].c = ACS_VLINE;
    mappa[19][MAX_X-5].c = ACS_LRCORNER; mappa[19][MAX_X-6].c = ACS_HLINE; mappa[19][MAX_X-7].c = ACS_HLINE; mappa[19][MAX_X-8].c = ACS_HLINE;
        mappa[19][MAX_X-9].c = ACS_HLINE; mappa[19][MAX_X-10].c = ACS_HLINE; mappa[19][MAX_X-11].c = ACS_LLCORNER;

    // Rientro centro destra
    mappa[14][MAX_X-1].c = ACS_LRCORNER; mappa[14][MAX_X-2].c = ACS_HLINE; mappa[14][MAX_X-3].c = ACS_ULCORNER;
    mappa[15][MAX_X-1].c = ' '; mappa[15][MAX_X-2].c = ' '; mappa[15][MAX_X-3].c = ACS_VLINE;
    mappa[16][MAX_X-1].c = ' '; mappa[16][MAX_X-2].c = ' '; mappa[16][MAX_X-3].c = ACS_VLINE;
    mappa[17][MAX_X-1].c = ' '; mappa[17][MAX_X-2].c = ' '; mappa[17][MAX_X-3].c = ACS_VLINE;
    mappa[18][MAX_X-1].c = ' '; mappa[18][MAX_X-2].c = ' '; mappa[18][MAX_X-3].c = ACS_VLINE;
    mappa[19][MAX_X-1].c = ACS_URCORNER; mappa[19][MAX_X-2].c = ACS_HLINE; mappa[19][MAX_X-3].c = ACS_LLCORNER;

    // Trattino in basso a sinistra
    mappa[21][2].c = ACS_ULCORNER; mappa[21][3].c = ACS_HLINE; mappa[21][4].c = ACS_HLINE; mappa[21][5].c = ACS_HLINE; mappa[21][6].c = ACS_URCORNER;
    mappa[22][2].c = ACS_LLCORNER; mappa[22][3].c = ACS_HLINE; mappa[22][4].c = ACS_HLINE; mappa[22][5].c = ACS_HLINE; mappa[22][6].c = ACS_LRCORNER;

    // L in basso a sinistra 1
    mappa[21][8].c = ACS_ULCORNER; mappa[21][9].c = ACS_HLINE; mappa[21][10].c = ACS_HLINE; 
        mappa[21][11].c = ACS_HLINE; mappa[21][12].c = ACS_HLINE; mappa[21][13].c = ACS_HLINE; mappa[21][14].c = ACS_URCORNER; 
    mappa[22][8].c = ACS_VLINE; mappa[22][9].c = ' '; mappa[22][10].c = ACS_ULCORNER;
        mappa[22][11].c = ACS_HLINE; mappa[22][12].c = ACS_HLINE; mappa[22][13].c = ACS_HLINE; mappa[22][14].c = ACS_LRCORNER; 
    mappa[23][8].c = ACS_VLINE; mappa[23][9].c = ' '; mappa[23][10].c = ACS_VLINE;
    mappa[24][8].c = ACS_VLINE; mappa[24][9].c = ' '; mappa[24][10].c = ACS_VLINE;
    mappa[25][8].c = ACS_VLINE; mappa[25][9].c = ' '; mappa[25][10].c = ACS_VLINE;
    mappa[26][8].c = ACS_VLINE; mappa[26][9].c = ' '; mappa[26][10].c = ACS_VLINE;
    mappa[27][8].c = ACS_VLINE; mappa[27][9].c = ' '; mappa[27][10].c = ACS_VLINE;
    mappa[28][8].c = ACS_LLCORNER; mappa[28][9].c = ACS_HLINE; mappa[28][10].c = ACS_LRCORNER;

    // L in basso a destra 1
    mappa[21][MAX_X-9].c = ACS_URCORNER; mappa[21][MAX_X-10].c = ACS_HLINE; mappa[21][MAX_X-11].c = ACS_HLINE; 
        mappa[21][MAX_X-12].c = ACS_HLINE; mappa[21][MAX_X-13].c = ACS_HLINE; mappa[21][MAX_X-14].c = ACS_HLINE; mappa[21][MAX_X-15].c = ACS_ULCORNER; 
    mappa[22][MAX_X-9].c = ACS_VLINE; mappa[22][MAX_X-10].c = ' '; mappa[22][MAX_X-11].c = ACS_URCORNER;
        mappa[22][MAX_X-12].c = ACS_HLINE; mappa[22][MAX_X-13].c = ACS_HLINE; mappa[22][MAX_X-14].c = ACS_HLINE; mappa[22][MAX_X-15].c = ACS_LLCORNER; 
    mappa[23][MAX_X-9].c = ACS_VLINE; mappa[23][MAX_X-10].c = ' '; mappa[23][MAX_X-11].c = ACS_VLINE;
    mappa[24][MAX_X-9].c = ACS_VLINE; mappa[24][MAX_X-10].c = ' '; mappa[24][MAX_X-11].c = ACS_VLINE;
    mappa[25][MAX_X-9].c = ACS_VLINE; mappa[25][MAX_X-10].c = ' '; mappa[25][MAX_X-11].c = ACS_VLINE;
    mappa[26][MAX_X-9].c = ACS_VLINE; mappa[26][MAX_X-10].c = ' '; mappa[26][MAX_X-11].c = ACS_VLINE;
    mappa[27][MAX_X-9].c = ACS_VLINE; mappa[27][MAX_X-10].c = ' '; mappa[27][MAX_X-11].c = ACS_VLINE;
    mappa[28][MAX_X-9].c = ACS_LRCORNER; mappa[28][MAX_X-10].c = ACS_HLINE; mappa[28][MAX_X-11].c = ACS_LLCORNER;

    // Trattino in basso a destra
    mappa[21][MAX_X-3].c = ACS_URCORNER; mappa[21][MAX_X-4].c = ACS_HLINE; mappa[21][MAX_X-5].c = ACS_HLINE; 
        mappa[21][MAX_X-6].c = ACS_HLINE; mappa[21][MAX_X-7].c = ACS_ULCORNER;
    mappa[22][MAX_X-3].c = ACS_LRCORNER; mappa[22][MAX_X-4].c = ACS_HLINE; mappa[22][MAX_X-5].c = ACS_HLINE; 
        mappa[22][MAX_X-6].c = ACS_HLINE; mappa[22][MAX_X-7].c = ACS_LLCORNER;

    // pilastro in basso a sinistra
    mappa[24][0].c = ACS_LLCORNER; mappa[25][0].c = ACS_ULCORNER; mappa[24][1].c = ACS_HLINE;
    mappa[24][2].c = ACS_URCORNER; mappa[25][1].c = ACS_HLINE; mappa[25][2].c = ACS_LRCORNER;

    // L in basso a sinistra 2
    mappa[24][4].c = ACS_ULCORNER; mappa[24][5].c = ACS_HLINE; mappa[24][6].c = ACS_URCORNER;
    mappa[25][4].c = ACS_VLINE; mappa[25][5].c = ' '; mappa[25][6].c = ACS_VLINE;
    mappa[26][4].c = ACS_VLINE; mappa[26][5].c = ' '; mappa[26][6].c = ACS_VLINE;
    mappa[27][2].c = ACS_ULCORNER; mappa[27][3].c = ACS_HLINE; mappa[27][4].c = ACS_LRCORNER; 
        mappa[27][5].c = ' '; mappa[27][6].c = ACS_VLINE;
    mappa[28][2].c = ACS_LLCORNER; mappa[28][3].c = ACS_HLINE; mappa[28][4].c = ACS_HLINE; 
        mappa[28][5].c = ACS_HLINE; mappa[28][6].c = ACS_LRCORNER;

    // M in basso al centro
    mappa[MAX_Y-7][12].c = ACS_ULCORNER; mappa[MAX_Y-7][13].c = ACS_HLINE; mappa[MAX_Y-7][14].c = ACS_HLINE; mappa[MAX_Y-7][15].c = ACS_HLINE; 
        mappa[MAX_Y-7][16].c = ACS_HLINE; mappa[MAX_Y-7][17].c = ACS_HLINE; mappa[MAX_Y-7][18].c = ACS_HLINE;
        mappa[MAX_Y-7][19].c = ACS_HLINE; mappa[MAX_Y-7][20].c = ACS_HLINE; mappa[MAX_Y-7][21].c = ACS_HLINE; mappa[MAX_Y-7][22].c = ACS_URCORNER;
    mappa[MAX_Y-6][12].c = ACS_VLINE; mappa[MAX_Y-6][13].c = ' '; mappa[MAX_Y-6][14].c = ACS_ULCORNER; mappa[MAX_Y-6][15].c = ACS_HLINE; 
        mappa[MAX_Y-6][16].c = ACS_HLINE; mappa[MAX_Y-6][17].c = ACS_HLINE; mappa[MAX_Y-6][18].c = ACS_HLINE; mappa[MAX_Y-6][19].c = ACS_HLINE;
        mappa[MAX_Y-6][20].c = ACS_URCORNER; mappa[MAX_Y-6][21].c = ' '; mappa[MAX_Y-6][22].c = ACS_VLINE;
    mappa[MAX_Y-5][12].c = ACS_VLINE; mappa[MAX_Y-5][13].c = ' '; mappa[MAX_Y-5][14].c = ACS_VLINE; 
        mappa[MAX_Y-5][20].c = ACS_VLINE; mappa[MAX_Y-5][21].c = ' '; mappa[MAX_Y-5][22].c = ACS_VLINE;
    mappa[MAX_Y-4][12].c = ACS_VLINE; mappa[MAX_Y-4][13].c = ' '; mappa[MAX_Y-4][14].c = ACS_VLINE;
        mappa[MAX_Y-4][20].c = ACS_VLINE; mappa[MAX_Y-4][21].c = ' '; mappa[MAX_Y-4][22].c = ACS_VLINE;
    mappa[MAX_Y-3][12].c = ACS_LLCORNER; mappa[MAX_Y-3][13].c = ACS_HLINE; mappa[MAX_Y-3][14].c = ACS_LRCORNER;
        mappa[MAX_Y-3][20].c = ACS_LLCORNER; mappa[MAX_Y-3][21].c = ACS_HLINE; mappa[MAX_Y-3][22].c = ACS_LRCORNER;

    // L in basso a destra 2
    mappa[24][MAX_X-5].c = ACS_URCORNER; mappa[24][MAX_X-6].c = ACS_HLINE; mappa[24][MAX_X-7].c = ACS_ULCORNER;
    mappa[25][MAX_X-5].c = ACS_VLINE; mappa[25][MAX_X-6].c = ' '; mappa[25][MAX_X-7].c = ACS_VLINE;
    mappa[26][MAX_X-5].c = ACS_VLINE; mappa[26][MAX_X-6].c = ' '; mappa[26][MAX_X-7].c = ACS_VLINE;
    mappa[27][MAX_X-3].c = ACS_URCORNER; mappa[27][MAX_X-4].c = ACS_HLINE; mappa[27][MAX_X-5].c = ACS_LLCORNER; 
        mappa[27][MAX_X-6].c = ' '; mappa[27][MAX_X-7].c = ACS_VLINE;
    mappa[28][MAX_X-3].c = ACS_LRCORNER; mappa[28][MAX_X-4].c = ACS_HLINE; mappa[28][MAX_X-5].c = ACS_HLINE; 
        mappa[28][MAX_X-6].c = ACS_HLINE; mappa[28][MAX_X-7].c = ACS_LLCORNER;

    // pilastro in basso a destra
    mappa[24][MAX_X-1].c = ACS_LRCORNER; mappa[25][MAX_X-1].c = ACS_URCORNER; mappa[24][MAX_X-2].c = ACS_HLINE;
    mappa[24][MAX_X-3].c = ACS_ULCORNER; mappa[25][MAX_X-2].c = ACS_HLINE; mappa[25][MAX_X-3].c = ACS_LLCORNER;

    // pilastro in basso al centro
    mappa[MAX_Y-1][16].c = ACS_LRCORNER; mappa[MAX_Y-1][17].c = ' '; mappa[MAX_Y-1][18].c = ACS_LLCORNER;
    mappa[MAX_Y-2][16].c = ACS_VLINE; mappa[MAX_Y-2][17].c = ' '; mappa[MAX_Y-2][18].c = ACS_VLINE;
    mappa[MAX_Y-3][16].c = ACS_VLINE; mappa[MAX_Y-3][17].c = ' '; mappa[MAX_Y-3][18].c = ACS_VLINE;
    mappa[MAX_Y-4][16].c = ACS_ULCORNER; mappa[MAX_Y-4][17].c = ACS_HLINE; mappa[MAX_Y-4][18].c = ACS_URCORNER;

    // setto le variabili booleane delle caselle
    //      attraversabile: una casella in cui un oggetto può andare
    for(i=0; i<MAX_Y; i++)
        for(j=0; j<MAX_X; j++) {
            if (mappa[i][j].c == SIMBOLO_SEME || mappa[i][j].c == SIMBOLO_PORTA) {
                mappa[i][j].attraversabile = true;
                if(mappa[i][j].c == SIMBOLO_PORTA)
                    mappa[i][j].seme = false;
                else
                    mappa[i][j].seme = true;
            } else
                mappa[i][j].seme = mappa[i][j].attraversabile = false;
        }    
    
    //      incrocio: una casella da cui si possono andare in almeno 3 direzioni
    for(i=0; i<MAX_Y; i++) 
        for(j=0; j<MAX_X; j++) {
            mappa[i][j].incrocio = false;
            // se la casella i-j è attraversabile
            if (mappa[i][j].attraversabile) {
                //              casella in alto
                if ((i>0 && mappa[i-1][j].attraversabile) && 
                //              casella a sinistra
                ((j>0 && mappa[i][j-1].attraversabile) && 
                //              casella a destra
                (j<MAX_X-1 && mappa[i][j+1].attraversabile)) )
                   mappa[i][j].incrocio = true;
                
                //               casella in basso
                else if((i<MAX_Y-1 && mappa[i+1][j].attraversabile) && 
                //               casella a sinistra
                ((j>0 && mappa[i][j-1].attraversabile) &&
                //               casella a destra
                (j<MAX_X-1 && mappa[i][j+1].attraversabile)))
                   mappa[i][j].incrocio = true;

                //              casella in alto
                if ((i>0 && mappa[i-1][j].attraversabile) && 
                //              casella a sinistra
                ((j>0 && mappa[i][j-1].attraversabile) && 
                //              casella in basso
                (i<MAX_Y-1 && mappa[i+1][j].attraversabile)) )
                   mappa[i][j].incrocio = true;

                //              casella in alto
                if ((i>0 && mappa[i-1][j].attraversabile) && 
                //              casella a destra
                ((j<MAX_X-1 && mappa[i][j+1].attraversabile) && 
                //              casella in basso
                (i<MAX_Y-1 && mappa[i+1][j].attraversabile)) )
                   mappa[i][j].incrocio = true;
            } 
        }

    //      angolo: una casella da cui si possono andare SOLO in 2 direzioni (1 orizzontale e 1 verticale)
    for(i=0; i<MAX_Y; i++) 
        for(j=0; j<MAX_X; j++) {
            mappa[i][j].angolo = false;
            if (mappa[i][j].attraversabile && !mappa[i][j].incrocio) {
                //              casella in alto                     casella a sinistra
                if (i>0 && mappa[i-1][j].attraversabile && j>0 && mappa[i][j-1].attraversabile)
                    mappa[i][j].angolo = true;
                //              casella in alto                     casella a destra
                if (i>0 && mappa[i-1][j].attraversabile && j<MAX_X-1 && mappa[i][j+1].attraversabile)
                    mappa[i][j].angolo = true;
                //              casella in basso                     casella a sinistra
                if (i<MAX_Y-1 && mappa[i+1][j].attraversabile && j>0 && mappa[i][j-1].attraversabile)
                    mappa[i][j].angolo = true;
                //              casella in basso                     casella a destra
                if (i<MAX_Y-1 && mappa[i+1][j].attraversabile && j<MAX_X-1 && mappa[i][j+1].attraversabile)
                    mappa[i][j].angolo = true;

            }
        }

    //      fantasmaNascosto: una casella in cui si nasconde un fantasma
    for(i=0; i<MAX_Y; i++) 
        for(j=0; j<MAX_X; j++)
            mappa[i][j].fantasmaNascosto = false;
}

/**
 * Inizializza tutte le combinazioni di colori usate nel programma
 * */
void inizializzaColori() {
    start_color();              // Permette di usare i colori

    init_pair(TERMINALE, COLOR_BLACK, COLOR_BLACK);     // testo nero e sfondo nero
    init_pair(CASELLA_ATTRAVERSABILE,COLOR_YELLOW, COLOR_BLACK);     // testo giallo e sfondo nero
    init_pair(CASELLA_NON_ATTRAVERSABILE, COLOR_BLUE, COLOR_BLACK);  // testo blu e sfondo nero
    init_pair(PORTA, COLOR_RED, COLOR_BLACK);       // testo rosso e sfondo nero

    init_pair(PACMAN,COLOR_YELLOW, COLOR_BLACK);     // testo giallo e sfondo nero

    init_pair(FANTASMA_ROSSO,COLOR_RED, COLOR_BLACK);           // testo rosso e sfondo nero
    init_pair(FANTASMA_MAGENTA,COLOR_MAGENTA, COLOR_BLACK);     // testo magenta e sfondo nero
    init_pair(FANTASMA_CIANO,COLOR_CYAN, COLOR_BLACK);          // testo ciano e sfondo nero
    init_pair(FANTASMA_VERDE,COLOR_GREEN, COLOR_BLACK);         // testo verde e sfondo nero

    init_pair(MISSILE,COLOR_WHITE, COLOR_BLACK);     // testo bianco e sfondo nero
}

/**
 * Inizializza il background del terminale colorandolo di nero
 * */
void sfondoTerminale() {
    int i, j;
    attron(COLOR_PAIR(TERMINALE));
    for(i = 0; i < getmaxy(stdscr); i++)
        for(j = 0; j < getmaxx(stdscr);j++) 
            mvaddch(i, j, ' ');
    attroff(COLOR_PAIR(TERMINALE));
}

/**
 * Genera N coordinate casuali diverse fra di loro e distanti 5 in orizzontale o verticale fra di loro, 
 * tutte le coordinate appartengono a caselle attraversabili,
 * inoltre (scelta personale) tali caselle non possono essere angoli o incroci
 * @param mappa     Matrice di caselle che costituiscono la mappa
 * @param fd_in     Descrittore di file in scrittura della pipe in cui il processo scriverà le coordinate generate
 * @param nCoordinateDaGenerare     Il numero di coordinate da generare
 * */
void generaCoordinate(Casella mappa[MAX_Y][MAX_X], int fd_in, int nCoordinateDaGenerare) {
    //Indici righe e colonne
    int i, j;
    // Conterrà temporaneamente le coordinate
    Oggetto coordinate[nCoordinateDaGenerare];
    // controllo del ciclo, se la variabile è 'false', ci sono due volte le stesse coordinate o appartengono a caselle non adatte 
    bool controllo;
    // Contiene il valore minimo delle coordinate x, y generabili (la riga e colonna 0 sono i bordi-muri della mappa)
    int min = 1;

    // Genera le coordinate
    for (i = 0; i < nCoordinateDaGenerare; i++) {
        // Inizializza  i campi non necessari
        coordinate[i].c = ' ';
        coordinate[i].colore = TERMINALE;
        coordinate[i].id_oggetto = i;

        // do-while che cicla finché controllo è 'false' 
        do {
            // Genero le i-esime coordinate x e y
            coordinate[i].x = min + rand()%((MAX_X- 1) - min + 1);
            coordinate[i].y = min + rand()%((MAX_Y- 1) - min + 1);       

            // Inizializzo controllo:
            //  true  -  se le coordinate appartengono a una casella: 
            //      NON attraversabile e NON angolo e NON incrocio
            // false  -  altrimenti
            controllo = mappa[coordinate[i].y][coordinate[i].x].attraversabile
                && (!mappa[coordinate[i].y][coordinate[i].x].angolo)
                && (!mappa[coordinate[i].y][coordinate[i].x].incrocio);
            
            // Se controllo è vera, controllo anche se
            if(controllo)
                // Controllo che disti almeno 5 in verticale o orizzontale dalle altre, per evitare che siano troppo vicine
                for(j = 0; j < i; j++)
                    controllo = ( ((abs(coordinate[i].x - coordinate[j].x) >= 5)
                        || (abs(coordinate[i].y - coordinate[j].y) >= 5))
                                && controllo);
        } while(!controllo);
    }
    
    // Scrive le coordinate nella pipe
    for(i = 0; i < nCoordinateDaGenerare; i++)
        write(fd_in, &coordinate[i], sizeof(coordinate[i]));
}

/**
 * Cuore del programma, questa funzione fa tutto i controlli per le collisioni fra i vari oggetti e si occupa della stampa di questi
 * @param   mappa   Matrice di caselle che costituiscono la mappa
 * @param   coordinateOggettiIniziali   Vettore che contiene le coordinate degli oggetti iniziali, utile per gestire l'avvio dei fantasmi
 * @param   fd_coordinate_out   Descrittore di file in lettura della pipe usata dagli oggetti per trasmette la propria posizione
 * @param   fd_rimbalzi_in  Vettore di descrittori di file in scrittura delle pipes usate da questa funzione per avvisare i relativi fantasmi
 * di invertire la direzione (caso: collisione con altri fantasmi)
 * @param   fd_vita_fantasma_out    Vettore di descrittori di file in lettura delle pipes usate dai fantasmi per avvisare questa funzione
 * che sono tornati operativi
 * @param   fd_morte_fantasma_in    Vettore di descrittori di file in scrittura delle pipes usate da questa funzione per avvisare i fantasmi
 * che sono morti
 * @param fd_attiva_fantasmi_in     Vettore di descrittore di file in scrittura delle pipes usati da questa funzione per avvisare i (primi)
 * fantasmi che sono stati "scoperti" e che posson avviarsi
 * @return  intero che rappresenta la vittoria o la sconfitta del giocatore
 * */
int controlli(Casella mappa [MAX_Y][MAX_X], Oggetto coordinateOggettiIniziali[N_FANTASMI + 1], int fd_coordinate_out,
    int fd_rimbalzi_in[N_FANTASMI], int fd_vita_fantasma_out[N_FANTASMI], int fd_morte_fantasma_in[N_FANTASMI],
    int fd_attiva_fantasmi_in[N_FANTASMI]) {

    // Dichiarazione indici, solitamente righe, colonne e ausilio
    int i, j, k, z;

    // Inizializzo pacman locale
    Oggetto pacman;
    pacman.c = SIMBOLO_PACMAN;
    pacman.colore = PACMAN;
    pacman.id_oggetto = -1;
    pacman.x = coordinateOggettiIniziali[0].x ; 
    pacman.y = coordinateOggettiIniziali[0].y;

    // Inizializzo i fantasmi locali
    Oggetto fantasmi[N_FANTASMI];
    for (i = 0; i < N_FANTASMI; i++){
        fantasmi[i].c = SIMBOLO_FANTASMA;
        fantasmi[i].x = coordinateOggettiIniziali[i+1].x; 
        fantasmi[i].y = coordinateOggettiIniziali[i+1].y;
        fantasmi[i].colore = restituisciColore(i);
        fantasmi[i].id_oggetto = i+1;
    }
    
    // Inizializzo i missili pacman: i missili di pacman non possno ferire pacman
    Oggetto missili_pacman[N_DIREZIONI];
    for(i=0; i<N_DIREZIONI; i++) {
        missili_pacman[i].c = SIMBOLO_MISSILE;
        missili_pacman[i].x = missili_pacman[i].y = -1;
        missili_pacman[i].colore = MISSILE;
        missili_pacman[i].id_oggetto = -1;
    }
    // Inizializzo i missili fantasmi: i missili dei fantasmi non possono ferire i fantasmi
    Oggetto missili_fantasmi[N_FANTASMI][N_DIREZIONI];
    for(j=0; j<N_FANTASMI;j++)
        for(i=0; i<N_DIREZIONI; i++) {
            missili_fantasmi[j][i].c = SIMBOLO_MISSILE;
            missili_fantasmi[j][i].x = missili_fantasmi[j][i].y = -1;
            missili_fantasmi[j][i].colore = MISSILE;
            missili_fantasmi[j][i].id_oggetto = -1;
        }
    int proprietarioMissile; // identifica chi ha sparato il missile

    int statoFantasma = FANTASMA_MORTO;
    // Inizializzo il numero di vite di pacman
    int vite_pacman = MAX_VITE; 
    // Inizializzo lo scudo di pacman (10 missili = 1 vita)
    int resistenza_missili = MAX_COLPI_MISSILI;    
    //  Inizializzo il numero di vite dei fantasmi
    int vite_fantasmi[N_FANTASMI];
    for(i=0; i<N_FANTASMI;i++)
        vite_fantasmi[i] = MAX_VITE;  
    // Vettore di booleani per tenere traccia di quali fantasmi sono morti e quali no
    bool fantasmiMorti[N_FANTASMI] = {false} ;
    // Vettore di booleani per tenere traccia di quali fantasmi sono morti ma in attesa di essere impostati a vivi
    bool vivo_in_attesa[N_FANTASMI] = {false} ; 
    
    // Inizializzo gli offset x e y (dall'origine dello schermo in alto a sinistra) per la stampa
    int offset_x, offset_y;         
    offset_x = getmaxx(stdscr)/3;
    offset_y = 2;

    time_t tempo_immune=0;      // contiene il tempo in cui è stato colpito l'ultima volta pacman, 
    // Se differisce di 1 unità dal tempo reale, pacman non può perdere vite
    time_t tempo_reale=0;       // contiene il tempo reale;
    time_t tempo_attivazione_fantasmi[N_FANTASMI] = {0};  // contiene il tempo iniziale di attivazione
    // (l'istante in cui pacman lascia la posizione in cui è nascosto il fantasma)
    int secondi_da_attendere[N_FANTASMI];  // contiene il valore random che un fantasma dovrà aspettare
    // prima di iniziare a muoversi
    for(i=0; i<N_FANTASMI; i++)
        secondi_da_attendere[i] = TEMPO_CASUALE_MINIMO + rand() % (TEMPO_CASUALE_MASSIMO - TEMPO_CASUALE_MINIMO + 1);

    // Salva le caselle in cui sono nascosti i fantasmi
    Oggetto posizione_iniziale_fantasmi[N_FANTASMI];
    for(i=0; i<N_FANTASMI; i++)
        posizione_iniziale_fantasmi[i] = coordinateOggettiIniziali[i+1];
    // Vettore booleano per sapere quale fantasma si è attivato (è stato scoperto, ha passato il tempo random e ha iniziato a muoversi)    
    bool attivato[N_FANTASMI] = {false};

    int movimento;  // contiene un valore che viene inviato a ogni processo fantasma attraverso 1 pipe,
    // serve per far capire ai fantasmi se devono invertire la direzione (collisione fantasmi)
    Oggetto valore_letto;   // contiene il valore letto dalla pipe in una data iterazione:
    // l'oggetto del processo (pacman, fantasma, missile) che ha scritto sulla pipe
    int semi=0;       // contiene il numero di semini che pacman deve mangiare
    for(i=0; i<MAX_Y; i++)
        for(j=0; j<MAX_X; j++)
            if(mappa[i][j].c == SIMBOLO_SEME)
                semi++;
    

    // Cicla se     pacman ha 1 o più vite
    //  E se        ci sono ancora semini da mangiare
    do {
        // Ricava il tempo reale
        tempo_reale = time((time_t*)NULL);

        // Legge dalla pipe le coordinate di un oggetto
        read(fd_coordinate_out, &valore_letto, sizeof(valore_letto));
        
        // Controllo "rinascita" fantasma:
        // Se il fantasma i-esimo è morto, leggo dalla sua pipe, se la pipe non era vuota e il valore letto corrisponde al valore "fantasmi vivo",
        // imposta a vero la variabile vivo_in_attesa
        for(i=0; i< N_FANTASMI; i++) 
            if(fantasmiMorti[i] && !vivo_in_attesa[i]) {
                k = FANTASMA_MORTO;
                if((read(fd_vita_fantasma_out[i], &k, sizeof(k))) > 0)
                        if(k == FANTASMA_VIVO ) 
                            vivo_in_attesa[i] = true;
                        
            }
        
        // Controllo collisioni fantasmi durante l'uscita dal rettangolo:
        // Evitare che un fantasma si attivi se un fantasma è nella sua stessa casella
        for(i=0; i<N_FANTASMI; i++) 
            // Se il fantasma è morto ed è in attesa di tornare vivo
            if(fantasmiMorti[i] && vivo_in_attesa[i]) {
                // Inizializzo le variabili come se fosse vivo
                vite_fantasmi[i] = MAX_VITE;
                fantasmiMorti[i] =  false;
                vivo_in_attesa[i] = false;
                // Se trova una collisione con un altro fantasma, imposto le variabili nuovamente come se fosse morto e in attesa di tornare vivo
                for(j=0; j<N_FANTASMI; j++) 
                    if(fantasmi[i].id_oggetto != fantasmi[j].id_oggetto
                        && fantasmi[i].x == fantasmi[j].x && fantasmi[i].y == fantasmi[j].y
                        && !mappa[posizione_iniziale_fantasmi[j].y][posizione_iniziale_fantasmi[j].x].fantasmaNascosto 
                        && attivato[j]) {
                            vite_fantasmi[i] = 0;
                            fantasmiMorti[i] =  true;
                            vivo_in_attesa[i] = true;   
                    }
            }
            
       
        
        // Se il carattere del valore letto corrisponde al simbolo di pacman
        if(valore_letto.c == SIMBOLO_PACMAN) {

            // Cancello la vecchia posizione di pacman
            mvaddch(offset_y + pacman.y, offset_x + pacman.x, ' ');
            
            // Se le coordinate di pacma sono positive (non di default)
            if(pacman.y > 0 && pacman.x > 0)
                // Se la vecchia posizione di pacman contiene un fantasma nascosto, prima di aggiornare pacman
                if(mappa[pacman.y][pacman.x].fantasmaNascosto) {
                    // Controllo tutti i fantasmi
                    for(i=0; i<N_FANTASMI;i++)
                        // Ricavo la posizione del fantasma i-esimo (nascosto)
                        if(pacman.x == posizione_iniziale_fantasmi[i].x && pacman.y == posizione_iniziale_fantasmi[i].y)
                            break;
                        // Se il valore letto (la nuova posizone di pacman) è diversa dalla vecchia (quindi se pacman si è spostato),
                        // allora imposto il tempo attuale come il tempo in cui si è attivato il fantasma i-esimo
                        if(valore_letto.x != pacman.x || valore_letto.y != pacman.y)
                            // Salvo il tempo reale nel tempo di avvivazione del fantasma i
                            tempo_attivazione_fantasmi[i] = tempo_reale; 
                }

            // Aggiorno il pacman locale
            pacman = valore_letto;

            // Se pacman è su un semino, lo cancello, modifico il carattere nella mappa, la variabile booleana e decremento il contatore di semini
            if(mappa[pacman.y][pacman.x].c == SIMBOLO_SEME) {
                mappa[pacman.y][pacman.x].c = ' ';
                mappa[pacman.y][pacman.x].seme = false;
                semi--;
            }
        }

        // Altrimenti se il carattere del valore letto corrisponde al simbolo del fantasma
        else if (valore_letto.c == SIMBOLO_FANTASMA) {
            // Cerca il fantasma che ha scritto sulla pipe sfruttando gli ID
            for(i=0;i<N_FANTASMI;i++)
                if(fantasmi[i].id_oggetto == valore_letto.id_oggetto)
                    break;
 
            // Se l'ID letto non corrisponde a nessuno degli ID locali (prima volta che il fantasma scrive sulla pipe),
            // allora carico nel primo fantasma locale non inizializzato l'ID del fantasma che ha scritto
            if(i == N_FANTASMI)
                for(i=0;i<N_FANTASMI;i++)
                    if(fantasmi[i].id_oggetto == -1) {
                        fantasmi[i].id_oggetto = valore_letto.id_oggetto;
                        break;
                    }

            // Cancella il carattere nella vecchia posizione
            mvaddch(offset_y + fantasmi[i].y, offset_x + fantasmi[i].x, ' ');

            // Aggiorna la variabile locale fantasma
            fantasmi[i] = valore_letto;

            // Se il fantasma       non è morto
            //               non è più nascosto
            //               si è attivato
            if( !fantasmiMorti[i] &&
                !mappa[posizione_iniziale_fantasmi[i].y][posizione_iniziale_fantasmi[i].x].fantasmaNascosto 
                && attivato[i]) { 
                // Controllo se il fantasma i collide con un fantasma j
                for(j=0;j<N_FANTASMI;j++) {
                    movimento = OK_MOVIMENTO;   // movimento ok, il fantasma i non deve invertire la direzione
                    // Se i fantasmi i e j      sono diversi (id diversi)
                    //                          hanno le stesse coordinate
                    //                          il fantasma j non è morto
                    //                                //      non è nascosto
                    //                          ed è attivato (in movimento)
                    if(fantasmi[i].id_oggetto != fantasmi[j].id_oggetto && 
                        fantasmi[i].x == fantasmi[j].x && fantasmi[i].y == fantasmi[j].y
                        && !fantasmiMorti[j]
                        && !mappa[posizione_iniziale_fantasmi[j].y][posizione_iniziale_fantasmi[j].x].fantasmaNascosto 
                        && attivato[j]) {
                            // Collisione, imposto movimento a "inverti"
                            movimento = INVERTI_MOVIMENTO;
                            // Esco dal ciclo (non ci possono essere più di una collisione contemporaneamente)
                            break;
                    }
                }
                // Scrive sulla pipe i il valore assegnato a movimento (ok o inverti?)
                write(fd_rimbalzi_in[i], &movimento, sizeof(movimento));

                // (Se si è verificata una collisione) 
                // Se j < N_FANTASMI (succede solo con la break, quindi c'è stata una collisione)
                // fantasmi j e i sono diversi
                // il fantasma j non è morto
                //               non è nascosto
                //               è attivo 
                if(j<N_FANTASMI 
                    && fantasmi[j].id_oggetto != fantasmi[i].id_oggetto
                    && !fantasmiMorti[j]
                    && !mappa[posizione_iniziale_fantasmi[j].y][posizione_iniziale_fantasmi[j].x].fantasmaNascosto 
                    && attivato[j]) 
                        // Scrive sulla pipe j il valore assegnato a movimento (sicuramente "inverti")
                        write(fd_rimbalzi_in[j], &movimento, sizeof(movimento));
                
            }
            // Altrmenti se     il fantasma NON è attivo
            else if(!attivato[i]) {                                  
                // Se    il fantasma NON è nascosto
                // E se  sono passati i secondi random che doveva attendere
                if (!mappa[posizione_iniziale_fantasmi[i].y][posizione_iniziale_fantasmi[i].x].fantasmaNascosto 
                    && ((tempo_reale - tempo_attivazione_fantasmi[i]) >= secondi_da_attendere[i]) ) {
                        // Attivo il fantasma i
                        attivato[i] = true;
                        // Ciclo per tutti i fantasmi
                        for(j=0; j<N_FANTASMI; j++) 
                            // Se       il fantasma j è diverso dal fantasma i
                            // E se     il fantasma i e j hanno la stessa coordinate
                            if( fantasmi[i].id_oggetto != fantasmi[j].id_oggetto 
                                && fantasmi[i].x == fantasmi[j].x & fantasmi[i].y == fantasmi[j].y)
                                    // Disattivo il fantasma (ritardo alla prossima iterazione l'avvio del fantasma)
                                    attivato[i] = false;
                }

                // Scrive sulla pipe il valore di attivato (il fantasma è BLOCCATO o NON BLOCCATO)
                write(fd_attiva_fantasmi_in[i], &attivato[i], sizeof(attivato[i]));

                // Se ho attivato il fantasma
                if(attivato[i])
                    // Chiudo il fd (non mi serve più)
                    close(fd_attiva_fantasmi_in[i]);
                
            }
        // Altrimenti se il simbolo letto è il simbolo del missile
        } else if (valore_letto.c == SIMBOLO_MISSILE) {
            // Un ID missile è di 3 cifre, del tipo "1ab", con a="id fantasma che l'ha sparato" 
            //      e b="valore intero che rappresenta la direzione del missile" 
            // Ricavo 'a', se questo è uguale a 0 (pid pacman)
            if( (valore_letto.id_oggetto / OFFSET_ID_MISSILE) == pacman.id_oggetto)
                // il proprietario del missile è pacman
                proprietarioMissile = PROPRIETARIO_PACMAN;
            // Altrimenti 
            else
                //  il missile appartiene a un fantasma
                proprietarioMissile = PROPRIETARIO_FANTASMA;

            // Se il proprietario è pacman
            if(proprietarioMissile == PROPRIETARIO_PACMAN) {
                // Ricavo l'indice del vettore dei missili di pacman (locale)
                for(i=0;i<N_DIREZIONI;i++)
                    if(missili_pacman[i].id_oggetto == valore_letto.id_oggetto)
                        break;
    
                // Se l'ID letto non corrisponde a nessuno degli ID locali (prima volta che il missile scrive),
                // assegno al primo missile con ID non valido, l'ID del missile letto
                if(i == N_DIREZIONI)
                    for(i=0;i<N_DIREZIONI;i++)
                        if(missili_pacman[i].id_oggetto == -1) {
                            missili_pacman[i].id_oggetto = valore_letto.id_oggetto;
                            break;
                        }

                // Cancello il carattere nella vecchia posizione
                mvaddch(offset_y + missili_pacman[i].y, offset_x + missili_pacman[i].x, ' ');

                // Aggiorno la variabile locale missile pacman
                missili_pacman[i] = valore_letto; 

            // Se il missile appartiene a un fantasma
            } else {
                // Salvo in j l'indice nel vettore del fantasma che ha sparato il missile
                for(i=0; i<N_FANTASMI; i++)
                    if(fantasmi[i].id_oggetto == (valore_letto.id_oggetto / OFFSET_ID_MISSILE)) {
                        j = i;
                        break;
                }
                // Ricavo l'indice i della matrice, tale che l'ID del missile fantasma in posizione (j,i) 
                // corrisponde all'ID del missile letto
                for(i=0;i<N_DIREZIONI;i++)
                    if(missili_fantasmi[j][i].id_oggetto == valore_letto.id_oggetto)
                        break;
    
                // Se l'ID letto non corrisponde a nessuno degli ID locali (prima volta che il missile scrive),
                // assegno al primo missile con ID non valido, l'ID del missile letto
                if(i == N_DIREZIONI)
                    for(i=0;i<N_DIREZIONI;i++)
                        if(missili_fantasmi[j][i].id_oggetto == -1) {
                            missili_fantasmi[j][i].id_oggetto = valore_letto.id_oggetto;
                            break;
                        }

                // Cancello il carattere nella vecchia posizione
                mvaddch(offset_y + missili_fantasmi[j][i].y, offset_x + missili_fantasmi[j][i].x, ' ');

                // Aggiorna la variabile locale missile fantasma
                missili_fantasmi[j][i] = valore_letto; 
            }
        }
        
        // Controllo se i fantasmi nascosto sono stati scoperti:
        // Se   il fantasma i è nascosto
        //      il tempo di attivazione è diverso da 0 (succede quando pacman si sposta dalla casella in cui risiede il fantasma dopo aver mangiato il seme)
        //      il tempo reale - il tempo di attivazione >= secondi da attendere
        for(i=0; i<N_FANTASMI; i++) 
            if(mappa[posizione_iniziale_fantasmi[i].y][posizione_iniziale_fantasmi[i].x].fantasmaNascosto
                && tempo_attivazione_fantasmi[i] != 0)
                //&& ((tempo_reale - tempo_attivazione_fantasmi[i]) >= secondi_da_attendere[i])) 
                    // il fantasma i non è più nascosto
                    mappa[posizione_iniziale_fantasmi[i].y][posizione_iniziale_fantasmi[i].x].fantasmaNascosto = false;               
            
        // Controllo collisioni missili pacman
        for(i=0; i<N_DIREZIONI; i++) {
                // Collisione muro o semini
                //  Se          le coordinate dei missili non sono quelle di default
                //  E se        il missile è in una casella NON attraversabile E pacman non è su una porta
                //  OPPURE se   il missile è in una casella NON attraversabile E le coordinate y del missile differiscono dalle coordinate y di pacman
                //      (quindi pacman è su una porta, il missile è su un muro e il missile in questione va verso l'alto o verso il basso)
                //  OPPURE se   il missile è in una casella NON attraversabile E le coordinate x del missile sono interne alla mappa
                //      (quindi pacman è su una porta, il missile è su un muro e si trova all'interno della mappa)
                //  OPPURE se   il missile tocca un semino  
                //      (i semi ovviamente non si trovano sui muri e non mi interessa che pacman sia su una porta)

                //  Da questo controllo si salvano i missili che vengono sparati 'fuori dalla mappa' quando pacman si trova su una porta
                //      e.g.    il missile che va verso sinistra se pacman è sulla porta sinistra della mappa 
                if( missili_pacman[i].x != -1 && missili_pacman[i].y != -1
                    && ((!mappa[missili_pacman[i].y][missili_pacman[i].x].attraversabile && mappa[pacman.y][pacman.x].c != SIMBOLO_PORTA)
                    || (!mappa[missili_pacman[i].y][missili_pacman[i].x].attraversabile && missili_pacman[i].y != pacman.y)
                    || (!mappa[missili_pacman[i].y][missili_pacman[i].x].attraversabile && (missili_pacman[i].x > 0 || missili_pacman[i].x < MAX_X-1))
                    || (mappa[missili_pacman[i].y][missili_pacman[i].x].seme)) ) {
                        //  Allora uccidi il missile e imposta le coordinate del missile i locale alle coordinate di default
                        kill(missili_pacman[i].pid_oggetto, 1);
                        missili_pacman[i].x = missili_pacman[i].y = -1;
                }

                // Collisione pacman
                //  Se          le coordinate dei missili non sono quelle di default
                //  E se        le coordinate di pacman coincidono con quelle di un missile
                else if ( missili_pacman[i].x != -1 && missili_pacman[i].y != -1
                    && missili_pacman[i].x == pacman.x && missili_pacman[i].y == pacman.y) {
                    //  Allora uccidi il missile e imposta le coordinate del missile i locale alle coordinate di default
                    //      (notare che pacman NON subisce danno per via del suo missile)
                    kill(missili_pacman[i].pid_oggetto, 1);
                    missili_pacman[i].x = missili_pacman[i].y = -1;
                }

                // Collisione fantasmi
                // Controllo per tutti i fantasmi
                for(j=0; j<N_FANTASMI; j++) 
                    // Se       le coordinate dei missili non sono quelle di default
                    // E se     il fantasma j è nascosto
                    // E se     le coordinate del fantasma j corrispondo alle coordinate del missile i di pacman
                    if( missili_pacman[i].x != -1 && missili_pacman[i].y != -1
                        && !mappa[posizione_iniziale_fantasmi[j].y][posizione_iniziale_fantasmi[j].x].fantasmaNascosto
                        && missili_pacman[i].x == fantasmi[j].x && missili_pacman[i].y == fantasmi[j].y) {
                            //  Allora uccidi il missile e imposta le coordinate del missile i locale alle coordinate di default
                            kill(missili_pacman[i].pid_oggetto, 1);
                            missili_pacman[i].x = missili_pacman[i].y = -1;
                            // Se       il fantasma NON è morto (magari un missile prende il fantasma morto mentre sta tornando alla base)
                            // E se     il fantasma è attivo (magari un misile prende il fantasma "inerme" che non ha ancora iniziato a muoversi)
                            if(!fantasmiMorti[j] && attivato[j])
                                // Decremento le vite del fantasma i
                                vite_fantasmi[j]--;
                        }
        }


        // Controllo collisioni missili fantasma
        // Controllo per tutti i fantasmi
        // (i controlli sono quasi uguali a quelli fatti per i missili di pacman)
        for(j=0; j<N_FANTASMI; j++) 
            // Controllo per tutte le direzioni dei missili
            for(i=0; i<N_DIREZIONI; i++) {                   
                                    // Collisione muro o semini
                //  Se          le coordinate del missile i del fantasma j non sono quelle di default
                //  E se        il missile è in una casella NON attraversabile E il fantasma non è su una porta
                //  OPPURE se   il missile è in una casella NON attraversabile E le coordinate y del missile differiscono dalle coordinate y del suo fantasma
                //      (quindi il fantasma è su una porta, il missile è su un muro e il missile in questione va verso l'alto o verso il basso)
                //  OPPURE se   il missile è in una casella NON attraversabile E le coordinate x del missile sono interne alla mappa
                //      (quindi il fantasma è su una porta, il missile è su un muro e si trova all'interno della mappa)
                //  OPPURE se   il missile tocca un semino  
                //      (i semi ovviamente non si trovano sui muri e non mi interessa che il fantasma sia su una porta)

                //  Da questo controllo si salvano i missili che vengono sparati 'fuori dalla mappa' quando il fantasma si trova su una porta
                //      e.g.    il missile che va verso sinistra se il fantasma è sulla porta sinistra della mappa,
                if( missili_fantasmi[j][i].x != -1 && missili_fantasmi[j][i].y != -1
                    && ((!mappa[missili_fantasmi[j][i].y][missili_fantasmi[j][i].x].attraversabile && mappa[fantasmi[j].y][fantasmi[j].x].c != SIMBOLO_PORTA)
                    || (!mappa[missili_fantasmi[j][i].y][missili_fantasmi[j][i].x].attraversabile && missili_fantasmi[j][i].y != fantasmi[j].y)
                    || (!mappa[missili_fantasmi[j][i].y][missili_fantasmi[j][i].x].attraversabile && (missili_fantasmi[j][i].x > 0 || missili_fantasmi[j][i].x < MAX_X-1))
                    || (mappa[missili_fantasmi[j][i].y][missili_fantasmi[j][i].x].seme)) ) {
                        //  Allora uccidi il missile i del fantasma j e imposta le coordinate del missile locale alle coordinate di default
                        kill(missili_fantasmi[j][i].pid_oggetto, 1);
                        missili_fantasmi[j][i].x = missili_fantasmi[j][i].y = -1;
                    }

                    // Collisione pacman
                    //  Se          le coordinate del missile i del fantasma j non sono quelle di default
                    //  E se        le coordinate di pacman coincidono con quelle del missile
                    if (missili_fantasmi[j][i].x != -1 && missili_fantasmi[j][i].y != -1
                        && missili_fantasmi[j][i].x == pacman.x && missili_fantasmi[j][i].y == pacman.y) {
                            //  Allora uccidi il missile i del fantasma j e imposta le coordinate del missile locale alle coordinate di default
                            kill(missili_fantasmi[j][i].pid_oggetto, 1);
                            missili_fantasmi[j][i].x = missili_fantasmi[j][i].y = -1;
                            // Decremento lo scudo dei missili
                            resistenza_missili--;
                            // Se lo scudo si è azzerato
                            if(resistenza_missili == 0) {
                                resistenza_missili = MAX_COLPI_MISSILI;     // Lo scudo torna al massimo
                                vite_pacman--;      // pacman perde una vita
                            }
                    }

                    // I MISSILI DEVONO ESPLODERE SE SI SCONTRANO (dubbio specifiche)?
                    // SE NON SI DEVONO SCONTRARE BSATA COMMENTARE QUESTI DUE BLOCCHI DI CODICE 

                    // BLOCCO1: Collisione missili pacman
                    // Controllo per tutte le direzioni
                    for(k=0; k<N_DIREZIONI; k++) 
                        //  Se       le coordinate del missile i del fantasma j non sono quelle di default
                        //  E se     le coordinate del missile j del fantasma i e il missile k di pacman coincidono 
                        //      (sapendo che le coordinate del primo NON sono quelle di default)
                        if( missili_fantasmi[j][i].x != -1 && missili_fantasmi[j][i].y != -1
                            && missili_fantasmi[j][i].x == missili_pacman[k].x && missili_fantasmi[j][i].y == missili_pacman[k].y) {
                                //  Allora uccidi il missile i del fantasma j e imposta le coordinate del missile locale alle coordinate di default
                                //      (notare che il fantasma k non perde vite)
                                kill(missili_pacman[k].pid_oggetto, 1);
                                missili_pacman[k].x = missili_pacman[k].y = -1;
                                kill(missili_fantasmi[j][i].pid_oggetto, 1);
                                missili_fantasmi[j][i].x = missili_fantasmi[j][i].y = -1;
                            }

                    // BLOCCO2: Collisione missili fantasma
                    // Controllo per tutti i fantasmi
                    for(z=0; z<N_FANTASMI; z++)
                        if(fantasmi[z].id_oggetto != fantasmi[j].id_oggetto)
                            // Controllo per tutte le direzioni
                            for(k=0; k<N_DIREZIONI; k++) 
                                //  Se       le coordinate del missile i del fantasma j non sono quelle di default
                                //  E se     le coordinate del missile j del fantasma i e il missile k di pacman coincidono 
                                //      (sapendo che le coordinate del primo NON sono quelle di default)
                                if( missili_fantasmi[j][i].x != -1 && missili_fantasmi[j][i].y != -1
                                    && missili_fantasmi[j][i].x == missili_fantasmi[z][k].x && missili_fantasmi[j][i].y == missili_fantasmi[z][k].y) {
                                        //  Allora uccidi il missile i del fantasma j e imposta le coordinate del missile locale alle coordinate di default
                                        //      (notare che il fantasma k non perde vite)
                                        kill(missili_fantasmi[z][k].pid_oggetto, 1);
                                        missili_fantasmi[z][k].x = missili_fantasmi[z][k].y = -1;
                                        kill(missili_fantasmi[j][i].pid_oggetto, 1);
                                        missili_fantasmi[j][i].x = missili_fantasmi[j][i].y = -1;
                                    }
                    

                    // Collisione fantasmi
                    // Controllo per tutti i fantasmi
                    for(k=0; k<N_FANTASMI; k++) 
                        // Se          le coordinate del missile i del fantasma j non sono quelle di default
                        // E se        il fantasma k è nascosto
                        // E se        le coordinate del fantasma k corrispondo alle coordinate del missile i del fantasma j
                        if( missili_fantasmi[j][i].x != -1 && missili_fantasmi[j][i].y != -1
                            && !mappa[posizione_iniziale_fantasmi[k].y][posizione_iniziale_fantasmi[k].x].fantasmaNascosto 
                            && missili_fantasmi[j][i].x == fantasmi[k].x && missili_fantasmi[j][i].y == fantasmi[k].y) {
                                //  Allora uccidi il missile i del fantasma j e imposta le coordinate del missile locale alle coordinate di default
                                //      (notare che il fantasma k non perde vite)
                                kill(missili_fantasmi[j][i].pid_oggetto, 1);
                                missili_fantasmi[j][i].x = missili_fantasmi[j][i].y = -1;
                            }
                }
            

        // Controllo morte fantasma
        // Per tutti i fantasmi
        for(i=0; i< N_FANTASMI; i++) 
            // Se       il fantasma i è morto
            // E se     le sue vite sono non positive
            if(!fantasmiMorti[i]
                && vite_fantasmi[i] <= 0) {
                    // Informo il fantasm i che è morto, ci penserà lui ad avviare  il processola gestione della sua morte
                    write(fd_morte_fantasma_in[i], &statoFantasma, sizeof(statoFantasma));
                    // Imposto il fantasma come morto
                    fantasmiMorti[i] = true;
                }
        

        // Controllo collisione pacman con un fantasma
        // Per tutti i fantasmi
        for(i=0; i<N_FANTASMI; i++) 
            // Se       il fantasma non è nascosto
            // E se     è attivo
            // E se     non è morto
            // E se     le coordinate del fantasma i coincidono con quelle di pacman
            // E se     è passato almeno 1 secondo dall'ultima collisione di pacman con un fantasma (per evitare di perdere tutte le vite in pochi ms)
            if(!mappa[posizione_iniziale_fantasmi[i].y][posizione_iniziale_fantasmi[i].x].fantasmaNascosto 
                && attivato[i]
                && !fantasmiMorti[i]
                && pacman.x == fantasmi[i].x && pacman.y == fantasmi[i].y
                && (tempo_reale - tempo_immune > 1)) {
                    // Allora decremento le vite
                    vite_pacman--;
                    // Aggiorno la variabile "tempo_immune" col tempo attuale
                    tempo_immune = tempo_reale;   
            }
        

        // Stampa la mappa
        attron(A_BOLD);     // attributo grassetto: on
        for(i=0; i<MAX_Y; i++)          // ciclo righe
            for(j=0; j<MAX_X; j++) {    // ciclo colonne
                // Se il simbolo (i,j) della mappa è il semino
                if(mappa[i][j].c == SIMBOLO_SEME) {
                    attron(COLOR_PAIR(CASELLA_ATTRAVERSABILE));         // attributo  combinazione colore(casella attraversabile):  on  
                    mvaddch(offset_y+i, offset_x+j, mappa[i][j].c);     // stampo il carattere
                    attroff(COLOR_PAIR(CASELLA_ATTRAVERSABILE));        // attributo  combinazione colore(casella attraversabile):  off
                // Altrimenti se il simbolo (i,j) è il simbolo della porta
                } else if (mappa[i][j].c == SIMBOLO_PORTA) {
                    attron(COLOR_PAIR(PORTA));                          // attributo  combinazione colore(porta):  on
                    mvaddch(offset_y+i, offset_x+j, mappa[i][j].c);     // stampo il carattere
                    attroff(COLOR_PAIR(PORTA));                         // attributo  combinazione colore(porta):  off
                // In tutti gli altri casi
                } else {
                    attron(COLOR_PAIR(CASELLA_NON_ATTRAVERSABILE));     // attributo  combinazione colore(muro):  on
                    mvaddch(offset_y+i, offset_x+j, mappa[i][j].c);     // stampo il carattere
                    attroff(COLOR_PAIR(CASELLA_ATTRAVERSABILE));        // attributo  combinazione colore(muro):  off
                }
            }      
        attroff(A_BOLD);    // attributo grassetto: off
        

        // Stampo i fantasmi
        // Per tutti i fantasmi
        for(i=0; i<N_FANTASMI; i++) 
            // Se       le coordinate del fantasma i NON sono quelle di default
            // E se     è stato scoperto 
            if(fantasmi[i].y != -1 && fantasmi[i].x != -1
                && tempo_attivazione_fantasmi[i] != 0) {
                // Stampo la scritta FANTASMI
                mvprintw(0,0, "FANTASMI");      
                // Stampo la stringa che contiene il colore del fantasma
                mvprintw(i+1,0, "%s", stampaColore(fantasmi[i].colore));
                // Stampo il numero di vite del fantasma    
                mvprintw(i+1, 8, "vite: %d", vite_fantasmi[i]);
                // attributo colore fantasma: on     
                attron(COLOR_PAIR(fantasmi[i].colore));     
                // Stampa il fantasma
                mvaddch(offset_y+fantasmi[i].y, offset_x+fantasmi[i].x, fantasmi[i].c);
                // attributo colore fantasma: off
                attroff(COLOR_PAIR(fantasmi[i].colore));    
            }
        

        // Stampo i missili di pacman
        // attributo colore missile: on
        attron(COLOR_PAIR(MISSILE));
        // Per tutte le direzioni
        for(i=0; i<N_DIREZIONI; i++) 
            // Se       le coordinate del missile i NON sono quelle di default
            if(missili_pacman[i].y != -1 && missili_pacman[i].x != -1) 
                // Stampo il missile
                mvaddch(offset_y+missili_pacman[i].y, offset_x+missili_pacman[i].x, missili_pacman[i].c);
        

        // Stampo i missili dei fantasmi
        // Per tutti i fantasmi
        for(j=0; j<N_FANTASMI; j++) 
            // Per tutte le direzioni
            for(i=0; i<N_DIREZIONI; i++)
                // Se       il missile i del fantasma j ha le coordinate NON di default
                if(missili_fantasmi[j][i].y != -1 && missili_fantasmi[j][i].x != -1)
                    // Stampo il missile 
                    mvaddch(offset_y+missili_fantasmi[j][i].y, offset_x+missili_fantasmi[j][i].x, missili_fantasmi[j][i].c);
                
        // attributo colore missile: off
        attroff(COLOR_PAIR(MISSILE));
                

        // Stampo la scritta "pacman"
        mvprintw(N_FANTASMI+2,0, "PACMAN");
        // Stampo il numero di vite
        mvprintw(N_FANTASMI+3,5, "vite: %d", vite_pacman);
        // Stampo i semini che deve ancora mangiare per poter vincere
        mvprintw(N_FANTASMI+4,5, "semini da mangiare: %d   ", semi);
        // Stampo il numero di missile che può ancora subire prima di perdere una vita
        mvprintw(N_FANTASMI+5,5, "scudo missili: %d   ", resistenza_missili);
        // attributo colore pacman: on
        attron(COLOR_PAIR(pacman.colore));
        // Stampo il simbolo di pacman
        mvaddch(offset_y+pacman.y, offset_x+pacman.x, pacman.c);
        // attributo colore pacman: off
        attroff(COLOR_PAIR(pacman.colore));

        // Refresh del terminale
        refresh();

    } while(vite_pacman > 0 && semi > 0);

    // il gioco è finito, uccido i processi pacman e fantasmi
    kill(pacman.pid_oggetto, 1);
    for(i=0; i<N_FANTASMI;i++)
        kill(fantasmi[i].pid_oggetto, 1);

    // Uccido gli eventuali processi missili pacman rimasti
    // Controllo per tutte le direzioni
    for(i=0; i<N_DIREZIONI; i++) 
        // Se le coordinate dei missili non sono quelle di default
        if(missili_pacman[i].x != -1 && missili_pacman[i].y != -1) {
            //  uccidi il missile e imposta le coordinate del missile i locale alle coordinate di default
            kill(missili_pacman[i].pid_oggetto, 1);
            missili_pacman[i].x = missili_pacman[i].y = -1;
        }

    // Uccido gli eventuali processi missili fantasma rimasti
    // Controllo per tutti i fantasmi
    for(j=0; j<N_FANTASMI; j++)
        // Controllo per tutte le direzioni dei missili
        for(i=0; i<N_DIREZIONI; i++) 
            // Se le coordinate del missile i del fantasma j non sono quelle di default
            if(missili_fantasmi[j][i].x != -1 && missili_fantasmi[j][i].y != -1) {                        
                //  uccidi il missile i del fantasma j e imposta le coordinate del missile locale alle coordinate di default
                kill(missili_fantasmi[j][i].pid_oggetto, 1);
                missili_fantasmi[j][i].x = missili_fantasmi[j][i].y = -1;
            }

    // Se pacman ha ancora delle vite, è uscito dal do-while perché ha finito i semi: HA VINTO
    if(vite_pacman > 0)
        return VITTORIA;
    // Altrimenti ha perso
    else
        return SCONFITTA;
}