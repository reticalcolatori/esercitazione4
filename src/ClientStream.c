#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/ioctl.h> 
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>

#define DIM_BUFFER 256
#define DIM_WIND 128

#define PARAM_ERR 1
#define NETW_ERR 2
#define TX_NETW_ERR 3
#define RX_NETW_ERR 4
#define IO_ERR 5
#define FORK_ERR 6
#define EXEC_ERR 7

int main(int argc, char const *argv[]) {
    // client serverAddress portAddress

    char okstr[DIM_BUFFER];

    /* CONTROLLO ARGOMENTI ---------------------------------- */
	if(argc!=3){
		printf("Error:%s serverAddress serverPort\n", argv[0]);
		exit(PARAM_ERR);
	}

    int i = 0;
    char zero = 0;

    //Controllo sulla porta
	while( argv[2][i]!= '\0' ){
		if((argv[2][i] < '0') || (argv[2][i] > '9')){
			printf("Secondo argomento non intero\n");
			printf("Error:%s serverAddress serverPort\n", argv[0]);
			exit(PARAM_ERR);
		}
		i++;
	} 

	//Conversione port a intero
	int port = atoi(argv[2]);

	/* VERIFICA PORT e HOST */
	if (port < 1024 || port > 65535){
		printf("%s = porta scorretta...\n", argv[2]);
		exit(PARAM_ERR);
	}

    //Controllo subito se il nodoServer è corretto, altrimenti esco
    struct hostent *host;
    host = gethostbyname(argv[1]);

    if(host == NULL){
        perror("Impossibile risalire all'indirizzo del server.");
        exit(PARAM_ERR);
    }

    //predispongo strutture per la socket e inizializzo
    struct sockaddr_in serveraddr;
    int fdSocket = -1;
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(port);
    serveraddr.sin_addr.s_addr= ((struct in_addr *)(host->h_addr_list[0]))->s_addr;

    //Preparo variabili per nome e parola da eliminare
    char nomeDirettorio[DIM_BUFFER];
    char lineIN[DIM_BUFFER];
        
    //chiedo ciclicamente il nome del direttorio fino ad EOF!
    //Chiedo separatamente nome direttorio e parola da eliminare.
    printf("Inserisci un nome di direttorio (EOF per uscire):\n");

    //Secondo le DOCS di fgets:
    //1) Se c'era un \n lo aggiungo al buffer.
    //2) Aggiungo un \0 finale.
    //Al server da noia il \n?????
    while((fgets(nomeDirettorio, DIM_BUFFER, stdin)) != NULL){

        //Invio al server fino a \n quindi non server un +1
        int lenNomeDirettorio = strlen(nomeDirettorio) - 1;

        //prendo tempo di start
        clock_t begin = clock();

        /*ho già eseguito tutti i controlli del caso, posso procedere ad inviare al server:
        * 1) Nome direttorio
        */

        //creo la socket solo se non l'ho fatto prima:
        
        if(fdSocket < 0){
            if((fdSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0){
                perror("Impossibile creare la socket.");
                exit(NETW_ERR);
            }

            //eseguo la connect, la BIND verrà effettuata automaticamente
            if((connect(fdSocket, (struct sockaddr *) &serveraddr, sizeof(struct sockaddr_in))) < 0){
                perror("Impossibile instaurare la connessione con il server.");
                exit(NETW_ERR);
            }
            printf("Client: correttamente connesso al server!\n");
        }

        

        //Invio al server 
        if((write(fdSocket, &nomeDirettorio, sizeof(char)*lenNomeDirettorio)) < 0){
            perror("Errore durante la scrittura del nome del direttorio al server.");
            //qua sarebbe contro protocollo proseguire dall'altro lato ho il server che si aspetta proprio il nome del direttorio
            //--> chiudo così dopo un po' anche il server si accorgerà che qualcosa non è andato e chiuderà
            //la connessione con il corrispettivo client per il quale si è verificato l'errore.
            close(fdSocket);
            exit(TX_NETW_ERR);
        }

        printf("CLIENT: inviato al server il nome del direttorio %s\n", nomeDirettorio);

        //FILTRO A CARATTERI:
        //Devo distinguere un fine linea da un terminatore.

        char currCh;
        int counter = 0;
        int nread = 0;

        while((nread = read(fdSocket, &currCh, sizeof(char))) > 0){
            if(currCh == '\n'){
                //Fine linea invia ancora.
                //Aggiungo terminatore
                lineIN[counter] = '\0';
                printf("%s\n", lineIN);
            }else if(currCh == zero){
                //Terminatore ha finito di mandare i nomi dei file.
                //Qui dovrei aver il buffer della driver vuoto fino a nuova richiesta.
                //Aggiungo terminatore
                lineIN[counter] = '\0';
                printf("%s\n", lineIN);
                //esco
                break;
            }else{
                //Sto leggendo la riga.
                lineIN[counter++] = currCh;
            }
        }             

        //Controllo sul read:
        if(nread <= 0){
            if(nread == 0){
                //Server ha inviato EOF.
                perror("Il server ha inviato EOF.");
            }else{
                //Errore lettura
                perror("Errore lettura.");
            }

            exit(RX_NETW_ERR);
        }

                        
        //prendo il tempo finale
        clock_t end = clock();
        double time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
        printf("CLIENT: tempo di esecuzione %f sec\n", time_spent);

        printf("Inserisci un nome di direttorio (EOF per uscire):\n");
        gets(okstr); 
        //consumo il restante della linea (\n compreso), altrimenti alla prossima iterazione la fgets avrebbe già
        //il resto della linea da leggere
    }

        // Chiusura socket in spedizione -> invio dell'EOF
        shutdown(fdSocket, SHUT_WR);

        // Chiusura socket in ricezione
        shutdown(fdSocket, SHUT_RD);
                
        //A questo punto sono riuscito ad inviare tutto il file dall'altra parte e ora divento un filtro ben fatto,
        //per questo ho già creato un figlio che ha il compito di consumare tutto l'input che mi ritorna dal server,
        //per evitare che si saturi la buffer di ricezione del client.        
        close(fdSocket);

    return 0;

}
