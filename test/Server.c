#include <stdlib.h>
#include <stdio.h>
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
#include <math.h>

#define DIM_BUFFER 256

#define PARAM_ERR 1
#define NETW_ERR 2
#define TX_NETW_ERR 3
#define RX_NETW_ERR 4
#define IO_ERR 5
#define FORK_ERR 6
#define EXEC_ERR 7
#define SELECT_ERR 8

#define max(a,b)((a > b) ? a : b)
/*
int is_regular_file(const char *path) {
    struct stat path_stat;
    stat(path, &path_stat);
    return S_ISREG(path_stat.st_mode);
}

int is_directory(const char *path) {
    struct stat path_stat;
    stat(path, &path_stat);
    return S_ISDIR(path_stat.st_mode);
}*/

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
    char zero = 0;

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
	if (bind(udpFd,(struct sockaddr *) &servaddr, sizeof(servaddr)) < 0)	{
		perror("Errore binding socket datagram");
		exit(NETW_ERR);
	}

	// Creo la coda per la ricezione delle richieste di connessione socket stream
	if (listen(listenFd, 5) < 0) {
		perror("Errore creazione coda di ascolto."); 
		exit(NETW_ERR);
	}

	// Imposto handler per la gestione del segnale SIG_CHLD
	signal(SIGCHLD, SIG_IGN);

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
                close(listenFd); //chiudo la socket di ascolto di mio padre

                printf("SERVER: sono dentro al figlio\n");
                int nRead;
                char nomeFile[DIM_BUFFER];

                char errorMsg[DIM_BUFFER];

                for(;;){ // ciclo infinito di ricezione del nome del direttorio sulla stessa directory

                    if ((nRead = read(fdConnect, nomeFile, sizeof(nomeFile))) < 0) {
                        perror("Errore nella lettura del nome del file.");
                        strcpy(errorMsg, "Errore nella lettura del nome del file.\n");
                        if (write(fdConnect, errorMsg, strlen(errorMsg)) < 0) {
                            perror("Errore scrittura su socket connessa");
                            close(fdConnect);
                        }
                        write(fdConnect, &zero, sizeof(char));
                        continue;
                    } else if (nRead == 0) {
                        printf("SERVER FIGLIO (%d): ricevuto EOF, termino.\n", getpid());
                        break;
                    } else {
			//Start time
			clock_t begin = clock();

                        // Ho letto il nome del file, logica applicativa
                        DIR *currDir;
                        struct dirent *currItem;

                        // Verifico se esiste e ne possiedo i diritti
                        currDir = opendir(nomeFile);

                        if(currDir == NULL){
                            printf("SERVER FIGLIO (%d): errore apertura direttorio %s\n", getpid(), nomeFile);
                            // il file che mi hai passato o non esiste o non hai i diritti te lo comunico e rileggo
                            strcpy(errorMsg, "Errore nella lettura del nome del file.\n");  
                            if (write(fdConnect, errorMsg, strlen(errorMsg)) < 0) {
                                perror("Errore scrittura su socket connessa");
                                closedir(currDir);
                                close(fdConnect);
                            }
                            write(fdConnect, &zero, sizeof(char));
                            continue;
                        }
                        printf("SERVER FIGLIO (%d): ricevuto direttorio %s e aperto correttamente\n", getpid(), nomeFile);

                        // Ho aperto la directory richiesta dal cliente analizzo il contenuto e invio i nomi dei file 2° liv.
                        
                        while((currItem = readdir(currDir)) != NULL){ //ciclo su tutti i file della directory

                            // #define DT_DIR 4 (è definito così, a me segna errore quindi ho messo 4)
                            //sto controllando che sia una directory DT_DIR
                            if(currItem->d_type ==  4 && (strcmp(".", currItem->d_name) != 0) && (strcmp("..", currItem->d_name) != 0)){

                                //apro la directory di secondo livello da analizzare
                                DIR *secondLevelDir;
                                char subdirName[DIM_BUFFER];
                                char fileName[DIM_BUFFER];
                                strcpy(subdirName, nomeFile);
                                strcat(subdirName, "/");
                                strcat(subdirName, currItem->d_name);
                                printf("SUBDIR NAME = %s\n", subdirName);
                                secondLevelDir = opendir(subdirName);
                                struct dirent *secondLevelItem;

                                while((secondLevelItem = readdir(secondLevelDir)) != NULL) {
                                    //stessa motivazione di prima
                                    // #define DT_REG 8 (è definito così, a me segna errore quindi ho messo 8)
                                    //sto controllando che sia un file regolare DT_REG
                                    if(secondLevelItem->d_type ==  8){ //scrivo il nome del file sulla socket connessa
                                        strcpy(fileName, secondLevelItem->d_name);
                                        fileName[strlen(fileName)] = '\n';

                                        //stampo tutto \n compreso strlen conta tutto tranne eventuale \0
                                        // ATTENZIONE: problema è che per C 0 e \0 sono uguali!!!
                                        write(fdConnect, fileName, strlen(fileName));
//                                        printf("Trovato file nella sottodirectory %s --> %s\n", subdirName, secondLevelItem->d_name);
//                                        printf("Lo comunico al client\n");
                                    }
                                }

                                closedir(secondLevelDir);
                            } //if --> se file che analizzo dal direttorio principale è un direttorio
                        } //while --> fino a quando ho elementi nella cartella passata

                        closedir(currDir);

                        // Server ha terminato di rispondere al client per quella richiesta
                        printf("SERVER FIGLIO (%d): ho terminato di servire la richiesta per il direttorio %s\n", getpid(), nomeFile);

                        //devo comunicare al client che ho terminato di inviare
                        //tutti i nomi di file per la dir che mi ha passato
                        write(fdConnect, &zero, sizeof(char));


			//End time
			clock_t end = clock();
			double time_spent = (double)(end-begin)/CLOCKS_PER_SEC;
	                printf("SERVER STREAM: tempo di esecuzione %lf sec\n", time_spent);

                        
                    } //if..else
                } //for
                
                // Quando esco dal ciclo mi devo ricordare di chiudere la socket connessa ho ricevuto EOF!
                close(fdConnect);

                //il client non vuole più inviarmi nomi di directory, mi ha mandato EOF
                exit(0);

            } else if (pid > 0) { //ramo del padre che ciclicamente deve solo chiudere le socket connesse che si andranno via via creando
                close(fdConnect);
            } else {
                perror("Errore nella creazione di un figlio.");
                exit(FORK_ERR);
            }
        }

        if(FD_ISSET(udpFd, &fdSet)){
            // Allora ho ricevuto un datagram sulla socket server datagram.
            int fdCurrFile;  //fd del file aperto
            int fdtmpFile;  //fd file tmp
            char request[DIM_BUFFER];  //nome del file passato
            char *fileName;
            char *wordToDelete;  //parola da cancellare passata
            char currWord[DIM_BUFFER];  //parola da confrontare con quella da eliminare
            int res;  //intero da inviare al client 
            int nread;  //numero char letti dal file aperto dalla read
            char tmpChar;  //carattere letto da inserire nella stringa
            int pos = 0;  //posizione nella stringa currWord
            int resCmp;  //risultato della strcmp
            int dimCurrFile = 0;

            lenAddress=sizeof(struct sockaddr_in);/* valore di ritorno */ 

            //attendo nomeFile e parola dal client
            if (recvfrom(udpFd, request, DIM_BUFFER, 0, (struct sockaddr *)&cliaddr, &lenAddress)<0) {
                perror("recvfrom ");
                //qua DEVO morire nella comunicazione con il client comunico un intero negativo
                //ha senso fare la continue?
                res = -1;
                if (sendto(udpFd, &res, sizeof(res), 0, (struct sockaddr *)&cliaddr, lenAddress)<0) {
                    perror("sendto ");
                }
                continue;
            }

            //  devo separare *nomeFile* da *wordToDelete* dalla stringa ricevuta 
            //  TODO:fileName, wordToDelete
            //ciao.txt,prova
            printf("SERVER DATAGRAM: ricevuta richiesta %s\n", request);


            const char *delimiter = ",";
            fileName = strtok(request, delimiter);
            wordToDelete = strtok(NULL, delimiter);
            wordToDelete[strlen(wordToDelete)] = '\0';

            printf("SERVER DATAGRAM: ricevuto file %s\nParola da eliminare %s\n", fileName, wordToDelete);

            for(int i = 0; i < strlen(wordToDelete); i++)
                printf("%c\n", wordToDelete[i]);

            //Start time
            clock_t begin = clock();

            if (( fdCurrFile = open(fileName, O_RDWR)) < 0 ) {
                //non riesco ad aprire il file
                //Mando come risposta l'errore della open.
                res = fdCurrFile;
                //stampa locale di controllo
                printf("Errore: open file (parte datagram) il file non esiste o non ho i diritti\n");
                if (sendto(udpFd, &res, sizeof(res), 0, (struct sockaddr *)&cliaddr, lenAddress)<0) {
                    perror("Errore sendto res");
                }
                continue;
    	    } else {
			    //File aperto

                // LOGICA DI ELIMINAZIONE PAROLE 
                //  TODO:
                // 0)creo un file tmp di appoggio per eliminazione parole
                // 1)leggo un char alla volta, se diverso da ' ' e
                //   diverso da '\n', lo salvo nella stringa temporanea
                // 2)quando trovo ' ' o '\n' ho finito una parola, la
                //   confronto con strcmp e valuto il risultato 
                // 3)se la parola è diversa da wordToDelete la scrivo su 
                //   un file tmp e incremento il contatore res (altrimenti nulla)
                // 4)al termine sovrascrivo il file originario

                //se fallisce cosa segnalo al client??
                if ((fdtmpFile = open("temp", O_RDWR | O_CREAT | O_TRUNC, 0666)) < 0) {
                    printf("Errore: apertura file tmp\n");
                    res = -1;
                    //ha senso fare la continue?
                    if (sendto(udpFd, &res, sizeof(res), 0, (struct sockaddr *)&cliaddr, lenAddress)<0) {
                        perror("sendto ");
                    }
                    continue;
                }
                
			    while((nread = read(fdCurrFile, &tmpChar, sizeof(char))) > 0){
                    dimCurrFile += nread;

				    //controllo che il carattere letto sia della mia parola e non un delimitatore
                    if(tmpChar == ' ' || tmpChar == '\n'){
                        //ho terminato una parola --> valuto 
//                        printf("SERVER: terminato di leggere una parola %s\n", currWord);

                        //aggiungo il terminatore
                        currWord[pos] = '\0'; 

                        //confronto le due stringhe
//                        printf("Lunghezza parola da eliminare %d lunghezza parola corrente %d", strlen(wordToDelete), strlen(currWord));
                        resCmp = strcmp(wordToDelete, currWord);
                        if (resCmp != 0) {
                            //le 2 "parole" sono diverse --> la scrivo su file tmp
//                            printf("SERVER: terminato di leggere una parola %s diversa da quella da eliminare %s\n", currWord, wordToDelete);

                            //write(fdtmpFile, currWord, strlen(currWord) + 1);
                            if(write(fdtmpFile, currWord, strlen(currWord)) < 0){
                                perror("Errore scrittura su file temporaneo");
                            }
                            write(fdtmpFile, &tmpChar, sizeof(char));
                        } else {
                            //le 2 "parole" sono uguali --> non la scrivo su file e incremento
                            res++;
//                            printf("SERVER: terminato di leggere una parola %s uguale a quella da eliminare %s occ = %d\n", currWord, wordToDelete, res);
                        }

                        //strcpy(currWord, "");
                        bzero(currWord, sizeof(currWord));
                        pos=0;

                        
                    } else {
                        //sto ancora leggendo una parola --> aggiungo al currWord
                        currWord[pos++] = tmpChar;
                    }
			    } //terminato di processare il file copio il tmp e invio risposta che sta in res

                close(fdCurrFile);
                if (( fdCurrFile = open(fileName, O_WRONLY | O_TRUNC)) < 0 ) { //riaperto in sovrascrittura
                    perror("Errore riapertura file da sovrascrivere");
                }

                //sposto lseek IO pointer del tmp all'inizio
                lseek(fdtmpFile, 0, SEEK_SET);

                // ho finito di esaminare tuto il file, devo sovrascrivere 
                // e poi inviare risposta al client
                char ch;
                while (read(fdtmpFile, &ch, sizeof(char)) > 0)
                    write(fdCurrFile, &ch, sizeof(char));
                
                //Invio risposta.
                if (sendto(udpFd, &res, sizeof(res), 0, (struct sockaddr *)&cliaddr, lenAddress)<0) {
                    perror("sendto ");
                    continue;
                }

		//Stop time
		clock_t end = clock();
		double time_spent = (double)(end-begin)/CLOCKS_PER_SEC;
		printf("SERVER DATAGRAM: tempo di esecuzione %lf sec\n", time_spent);

                //ripristino il contenuto della variabile dove immagazzino le richieste degli utenti
                bzero(request, sizeof(request));

                //chiudo il file e resetto numero parole eliminate
                close(fdCurrFile);
                close(fdtmpFile);
                res = 0;
            }

        } //ifset datagram

        FD_ZERO(&fdSet); //ciclicamente risetto la maschera dei FD tutti a 0
    } //for

    

}
