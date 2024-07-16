/**
 * @author Jacopo Mereu Matricola N. 65699.
 * @date (file inviato il)  06/01/20
 * 
 * @section INFORMAZIONI GENERICHE
 *  Gruppo:  1 persona.
 *  Tempo impiegato (indicativamente): 12 gg.
 *  Specifiche rispettate: Tutte quelle sufficienti e necessarie (da pg. 1 a pg. 3 del file delle specifiche + l'effetto pacman).
 * 
 * @section TASTI PACMAN
 *  <spazio>    spara i missili in ogni direzione possibile, 
 * finché il missile sparato in una data direzione non "esplode", non sarà possibile ri-sparare in quella direzione.
 * <freccia sinistra>       sposta pacman, se possibile (se non c'è un muro), di 1 casella a sinistra.
 * <freccia su>             sposta pacman, se possibile (se non c'è un muro), di 1 casella in alto.
 * <freccia destra>         sposta pacman, se possibile (se non c'è un muro), di 1 casella a destra.
 * <freccia giu>            sposta pacman, se possibile (se non c'è un muro), di 1 casella in basso.
 * 
 * @section INFORMAZIONI OGGETTI
 *  VITE PACMAN                          3
 *  SCUDO ANTI-MISSILE                  10
 *  
 *  NUMERO FANTASMI                      4
 *  VITE FANTASMI                        3
 *  INTERVALLO SPARO DEI MISSILI       1-3 s
 *  INTERVALLO IN PRIGIONE            5-10 s
 * 
 * @section ISTRUZIONI PER COMPILARE ED ESEGUIRE IL PROGETTO
 * 1)   Aprire un terminale
 * 2)   Spostarsi nella cartella del progetto 
 * 3)   Se si modifica il codice, si deve eseguire il comando  "make"
 * 4)   Avviare l'eseguibile    "./pac_man"
 * */

#include "gestoreGioco.h"

int main() {
    avviaGioco();
    return 0;
}

