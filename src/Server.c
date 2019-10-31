#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>
#include <dirent.h>
#include <netdb.h>

#define DIM_BUFFER 256

#define PARAM_ERR 1
#define NETW_ERR 2
#define TX_NETW_ERR 3
#define RX_NETW_ERR 4
#define IO_ERR 5
#define FORK_ERR 6
#define EXEC_ERR 7
#define SELECT_ERR 8

void gestore(int signo){
	int stato;
	printf("esecuzione gestore di SIGCHLD\n");
	wait(&stato);
}

//server porta

int main(int argc, char **argv) {
    //Costante per definire REUSE_ADDR
    const int on = 1;

    //Socket descriptor per la listen e la connessione al client.
	int listenFd, udpFd;
    
	int port;
    //Lunghezza indirizzo
    int lenAddress;

    //Contatore per verifica args.
    long num;
	
    //Indirizzi client e server.
	struct sockaddr_in cliaddr, servaddr;
    //Risoluzione host client.
	struct hostent *host;

    //Strutture per algoritmo DATAGRAM
    int countRmWord = 0; //contatore numero di parole eliminate dal server
    char requestDatagram[DIM_BUFFER];
    char *nomeFile;
    char *parola;

    //Strutture per algoritmo STREAM
    //--------------------------------------------TODO------------------------------------------------------------

    //Inizializzo il buffer a zero
    memset(requestDatagram, 0, DIM_BUFFER);

	// Controllo argomenti
	if (argc != 2) {
		printf("Error: %s port\n", argv[0]);
		exit(PARAM_ERR);
	} else {
		num = 0;
		while (argv[1][num] != '\0') {
			if( (argv[1][num] < '0') || (argv[1][num] > '9') ){
				printf("Secondo argomento non intero\n");
				exit(PARAM_ERR);
			}
			num++;
		} 	
		port = atoi(argv[1]);
		if (port < 1024 || port > 65535) {
			printf("Error: %s port\n", argv[0]);
			printf("1024 <= port <= 65535\n");
			exit(PARAM_ERR);  	
		}
	}

	// Inizializzazione struttura server.
	memset ((char *)&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = INADDR_ANY;
	servaddr.sin_port = htons(port);

//--------------------------------------INIZIALIZZAZIONE SOCKET STREAM----------------------------------------------------
	// Creazione socket stream!
	listenFd = socket(AF_INET, SOCK_STREAM, 0);
	if(listenFd < 0) {
		perror("Errore creazione socket stream.");
		exit(NETW_ERR);
	}

    //Imposto REUSE_ADDR per la stream
	if(setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0){
		perror("Errore: impossibile abilitare opzione reuse address stream socket");
		exit(NETW_ERR);
	}

    // Eseguo il bind della socket stream
    if (bind(listenFd,(struct sockaddr *) &servaddr, sizeof(servaddr)) < 0)	{
		perror("Errore binding socket stream");
		exit(NETW_ERR);
	}

//--------------------------------------INIZIALIZZAZIONE SOCKET DATAGRAM----------------------------------------------------
    // Creazione socket datgram
    udpFd = socket(AF_INET, SOCK_DGRAM, 0);
    if(udpFd < 0){
        perror("Errore creazione datagram socket.");
        exit(NETW_ERR);
    }

    // Imposto REUSE_ADDR per la socket datagram
	if(setsockopt(udpFd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0){
		perror("Errore: impossibile abilitare opzione reuse address socket datagram");
		exit(NETW_ERR);
	}
    
    // Eseguo il bind della socket datagram
	if (bind(listenFd,(struct sockaddr *) &servaddr, sizeof(servaddr)) < 0)	{
		perror("Errore binding socket stream");
		exit(NETW_ERR);
	}

	// Creo la coda per la ricezione delle richieste di connessione socket stream
	if (listen(listenFd, 5) < 0) {
		perror("Errore creazione coda di ascolto."); 
		exit(NETW_ERR);
	}

	// Imposto handler per la gestione del segnale SIG_CHLD
	signal(SIGCHLD, gestore);

//---------------------------------------TERMINATO SETTING DELLE SOCKET STREAM/UDP----------------------------------------------

    // Ora pulisco le maschere e inizio a settare i fd di mio interesse che utilizzo nella select
    fd_set fdSet;
    FD_ZERO(&fdSet);

    int maxFd = max(listenFd, udpFd) + 1;

	// Ciclo infinito delle richieste che leggo tramite la select
	for (;;) {
        
        // Setto le maschere per la select
        FD_SET(listenFd, &fdSet);
        FD_SET(udpFd, &fdSet);

        if(select(maxFd, &fdSet, NULL, NULL, NULL) < 0){
            if (errno == EINTR) //se sono stato interrotto da un segnale la ripeto
                continue; 
            else { //errore esco
                perror("Errore esecuzione select");
                close(listenFd);
                close(udpFd);
                exit(SELECT_ERR);    
            }
        }

        if(FD_ISSET(listenFd, &fdSet)){
            // Allora un cliente stream ha richiesto una connessione sulla socket d'ascolto.
            printf("SERVER: ricevuta una richiesta di connessione...\n");
            int len = sizeof(struct sockaddr_in);
            int fdConnect;
            if((fdConnect = accept(listenFd, (struct sockaddr *)&cliaddr, &len)) < 0){
                if (errno==EINTR){
				    perror("Forzo la continuazione della accept.");
				    continue;
			    } else
                    exit(IO_ERR);
            }

            // Se tutto ok in fdConnect ho la socket per gestire la connessione
            int pid = fork();

            if(pid == 0){ // sono il figlio ciclicamente andrò a ricevere nomi di direttori (STESSA connessione)
                close(listenFd);

                int nRead;
                char nomeFile[DIM_BUFFER];

                for(;;){ // ciclo infinito di ricezione del nome del direttorio sulla stessa directory

                    if ((nRead = read(fdConnect, nomeFile, sizeof(nomeFile))) < 0) {
                        perror("Errore nella lettura del nome del file");
                        continue;
                    } else if (nRead == 0) {
                        printf("SERVER STREAM (FIGLIO): ricevuto EOF, termino.\n");
                        break;
                    } else {
                        // Ho letto il nome del file, logica applicativa
                        DIR *currDir;
                        
                        // Verifico se esiste e ne possiedo i diritti
                        currDir = opendir(nomeFile);
                        if(currDir == NULL){
                            // il file che mi hai passato o non esiste o non hai i diritti te lo comunico e rileggo
                            char *errorMsg = "Directory non presente o non possiedi i diritti.";
                            if (write(fdConnect, errorMsg, strlen(errorMsg)) < 0) {
                                perror("Errore scrittura su socket connessa");
                                closedir(currDir);
                                close(fdConnect);
                            }
                            continue;
                        }

                        // Ho aperto la directory richiesta dal cliente analizzo il contenuto e invio i nomi dei file 2° liv.
                        struct dirent *currItem;
                        
                        while((currItem = readdir(currDir)) != NULL){ //ciclo su tutti i file della directory
                            // #define DT_DIR 4 (è definito così, a me segna errore quindi ho messo 4)
                            //sto controllando che sia una directory
                            if(currItem->d_type == /*DT_DIR*/ 4 ){

                                //apro la directory di secondo livello da analizzare
                                DIR *secondLevelDir;
                                secondLevelDir = opendir(currItem);
                                struct dirent *secondLevelItem;

                                while((secondLevelItem = readdir(secondLevelDir)) != NULL) {
                                    //stessa motivazione di prima
                                    // #define DT_REG 8 (è definito così, a me segna errore quindi ho messo 8)
                                    //sto controllando che sia un file regolare
                                    if(currItem->d_type == /*DT_REG*/ 8 )
                                        //scrivo il nome del file sulla pipe
                                        write(fdConnect, currItem->d_name, strlen(currItem->d_name));
                                }

                                closedir(secondLevelDir);
                            }
                        }
                        closedir(currDir);

                        //devo comunicare al client che ho terminato di inviare
                        //tutti i nomi di file per la dir che mi ha passato
                        write(fdConnect, '\0', sizeof(char));
                        
                    } //if..else
                    
                } //for

                //il client non vuole più inviarmi nomi di directory, mi ha mandato EOF
            }
        }

        if(FD_ISSET(udpFd, &fdSet)){
            // Allora ho ricevuto un datagram sulla socket server datagram.
        }

        FD_ZERO(&fdSet); //ciclicamente risetto la maschera dei FD tutti a 0
        



        

		if (fork() == 0){ // figlio itera e processa le richieste fino a quando non riceve EOF!
			/*Chiusura FileDescr non utilizzati*/
			close(listen_sd);

            //Recupero informazioni client per stampa.
			host = gethostbyaddr( (char *) &cliaddr.sin_addr, sizeof(cliaddr.sin_addr), AF_INET);
			if (host == NULL){
				perror("Impossibile risalire alle informazioni dell'host a partire dal suo indirizzo.\n");
				continue;
			} else 
				printf("Server (figlio): host client è %s\n", host->h_name);

            //Leggo il numero della linea da rimuovere.
            if(read(conn_sd, &deleteLine, sizeof(long)) < 0){
                //Non riesco a leggere la linea da eliminare esco.
                perror("Impossibile leggere il numero della linea da eliminare.");
                exit(IO_ERR);
            }
			//se tutto ok non eseguo controlli sono tutti eseguiti dal client nel caso non invia nemmeno la richiesta al server.
			printf("Linea da cancellare: %ld\n", deleteLine);
			
            //Posso procedere:
            //Inizio a leggere il file
            //Mentre leggo conto le righe.
            //Se la riga è quella da eliminare non invio indietro.
            
            //Contatore dei caratteri
            num = 0;

			//prendo tempo di start
        	clock_t begin = clock();

			while((read(conn_sd, &tmpChar, sizeof(char))) > 0){

                //Salvo il carattere in un buffer temporaneo.
                buff[num] = tmpChar;

                //Se il carattere è il fine linea:
                if(buff[num] == '\n'){
                    //Se consideriamo come prima linea la 1 allora faccio prima:
                    currentLine++;

                    //Controllo linea da saltare
                    if(currentLine != deleteLine){
                        //Scrivo indietro la riga con \n finale e la stampo!
						printf("SERVER: rispedisco indietro la linea --> %s\n", buff);
                        write(conn_sd, buff, strlen(buff));
                    }

                    //Resetto il contatore.
                    num = 0;
                } else 
					num++;
            }

			// Chiusura socket in spedizione -> invio dell'EOF
			shutdown(conn_sd, 1);
			shutdown(conn_sd, 0);
       		close(conn_sd);


			//prendo il tempo finale
        	clock_t end = clock();
        	double time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
        	printf("CLIENT: tempo di esecuzione %f sec\n", time_spent);

            if(nread < 0){
                perror("Errore lettura file");
                exit(IO_ERR);
            }

		} // figlio

        //PADRE
		close(conn_sd);  // padre chiude socket di connessione non di ascolto
	} // ciclo for infinito
}
