#include <GL/glut.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <semaphore.h>
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "image.h"
#include "surface.h"
#include "world.h"
#include "vehicle.h"
#include "world_viewer.h"
#include "so_game_protocol.h"
#include "common.h"
#include "utils.h"

int window;
WorldViewer viewer;
World world;
Vehicle* vehicle; // The vehicle
char username[64];
char password[64];
int socket_desc;		//socket desc TCP
int socket_desc_UDP;	//socket desc UDP
sem_t sem_world_c;
int communication = 1;



//se recv() restituisce 0 o un errore di socket (ENOTCONN et similia), vuol dire che la comunicazione è stata chiusa. Idem per recvfrom
//send() invece, in caso di errore (ENOTCONN et similia), restituisce -1 e setta errno a un certo valore. Idem per sendto

void handle_signal(int sig){
    int ret;
    printf("Signal caught: %d\n", sig);

    switch(sig){
        case SIGHUP:
            break;
        case SIGTERM:
        case SIGQUIT:
        case SIGINT:
        case SIGALRM:
            if (verbosity_level>=General) printf("Closing...\n");
            communication = 0;
            ret = close(socket_desc);
            ERROR_HELPER(ret, "Error in closing socket desc TCP");

            ret = close(socket_desc_UDP);
            ERROR_HELPER(ret, "Error in closing socket desc UDP");

            ret = sem_destroy(&sem_world_c);
            ERROR_HELPER(ret, "Error in destroy sem_world_c");

            if (verbosity_level>=General) printf("Socket chiuse e semafori distrutti\n");

            World_destroy(&world);
            free(vehicle);

            if (verbosity_level>=General) printf("Mondo e veicolo distrutti\n");
            break;

        case SIGSEGV:
            if (verbosity_level>=General) printf("Segmentation fault... closing\n");
            communication = 0;
            ret = close(socket_desc);
            ERROR_HELPER(ret, "Error in closing socket desc TCP");

            ret = close(socket_desc_UDP);
            ERROR_HELPER(ret, "Error in closing socket desc UDP");

            ret = sem_destroy(&sem_world_c);
            ERROR_HELPER(ret, "Error in destroy sem_world_c");

            if (verbosity_level>=General) printf("Socket chiuse e semafori distrutti\n");

            World_destroy(&world);
            free(vehicle);

            if (verbosity_level>=General) printf("Mondo e veicolo distrutti\n");
            break;

        case SIGPIPE:
            if (verbosity_level>=General) printf("Socket closed\n");
            communication = 0;
            break;

        default:
            if (verbosity_level>=General) printf("Caught wrong signal...\n");
            return;
    }

}


void* thread_listener_tcp(void* client_args){
	/**COMUNICAZIONE TCP**/
    /**

    Al momento del login il server manda sul thread TCP ImagePackets
    contenenti id e texture di tutti i client presenti nel mondo e quelli che
    arriveranno nel mondo. Quando il campo texture dell'ImagePacket è settato a
    NULL, vuol dire che quell'utente si è disconnesso e deve essere eliminato dal
    mondo del client.


    Come fare per disconnettersi: ad intervalli regolari, il client sul thread
    TCP chiede la lista degli utenti connessi. Se vede la presenza di un nuovo
    utente, o nuovi utenti, chiede al server le loro texture e le aggiunge al
    mondo. Se vede che nella lista non ci sono persone che sono presenti nel suo
    mondo (cioè nella sua lista), allora le rimuove dal suo mondo

    **/

    int ret;

    thread_client_args* arg = (thread_client_args*) client_args;
    int socket=arg->socket_desc_TCP;        //socket TCP

    //Ricezione degli ImagePacket contenenti id e texture di tutti i client presenti nel mondo
    char* user = (char*)malloc(DIM_BUFF*sizeof(char));
    Vehicle *v;
    int msg_len;

    int bytes_read = 0;

    while (communication){

        //Il server invia le celle dell'array dei connessi che non sono messe a 0 o a -1.
        if (verbosity_level>=DebugTCP) printf("\n[TCP] Ricevo gli utenti già nel mondo\n");

        ret = recv_TCP_packet(socket, user, 0, &bytes_read);
        if (ret == -2 || ret == 0){
        	printf("Could not receive users already in world\n");
            alarm(1);
            printf("\nServer closed, goodbye!\n");
            exit(0);
        }
        else PTHREAD_ERROR_HELPER(ret, "Could not receive users already in world");

        msg_len=bytes_read;

        if (verbosity_level>=DebugTCP) printf("[TCP] Ricevuti i client connessi\n");

        if (verbosity_level>=DebugTCP) printf("[TCP] Deserializzo user\n");

        PacketHeader* clienth = Packet_deserialize(user, msg_len);

        if (verbosity_level>=DebugTCP) printf("[TCP] User deserializzato\n");

        if (clienth->type==PostTexture){
			ImagePacket* client=(ImagePacket*)clienth;

			int id = client->id;

            ret = sem_wait(&sem_world_c);
            PTHREAD_ERROR_HELPER(ret, "Failed to wait sem_world_c  adding vehicle \n");

			v = (Vehicle*) malloc(sizeof(Vehicle));
			Vehicle_init(v, &world, id, client->image);
            World_addVehicle(&world, v);

            ret = sem_post(&sem_world_c);
            PTHREAD_ERROR_HELPER(ret, "Failed to post sem_world_c  adding vehicle \n");

            client->image = NULL;       // altrimenti la packet_free elimina la texture vera e propria
            Packet_free((PacketHeader *) client);
		}
		else if(clienth->type==GetId){
			IdPacket* clientd=(IdPacket*)clienth;
			if(clientd->id>=MAX_USER_NUM);
			else{
				int id=clientd->id;

                ret = sem_wait(&sem_world_c);
                PTHREAD_ERROR_HELPER(ret, "Failed to wait sem_world_c  removing vehicle \n");

                v = World_getVehicle(&world, id);
				World_detachVehicle(&world, v);

                ret = sem_post(&sem_world_c);
                PTHREAD_ERROR_HELPER(ret, "Failed to post sem_world_c  removing vehicle \n");

                free(v);
			}
            Packet_free((PacketHeader *) clientd);
		}

        if (verbosity_level>=DebugTCP) printf("[TCP] Eliminati i client disconnessi dal mondo\n");
    }

    free(user);
    free(arg);
    pthread_exit(NULL);

}

void* thread_listener_udp_M(void* client_args){

    /**COMUNICAZIONE UDP**/
    /**

    Client, via UDP invia dei pacchetti VehicleUpdate al server. Il contenuto del VehicleUpdate viene prelevato da desired_force,
    è contenuto nella struttura del veicolo e si mette in attesa di pacchetti WorldUpdate che contengono al
    loro interno una lista collegata. Ricevuti questi pacchetti, smonta la lista collegata all'interno e per ogni elemento della lista,
    preso l'id, preleva dal mondo il veicolo con quell'id e ne aggiorna lo stato. I pacchetti di veicoli ancora non aggiunti al proprio mondo
    vengono ignorati (per necessità).

    **/

    int ret;

    thread_client_args* arg = (thread_client_args*) client_args;
    int socket_UDP = arg->socket_desc_UDP;
    int id=arg->id;
    Vehicle* veicolo=arg->v;
    struct sockaddr_in server_UDP = arg->server_addr_UDP;
    int slen/*, bytes_sent*/;


    /**
    Ciclo while che opera fino a quando il client è in funzione.
    **/

    VehicleUpdatePacket* update = (VehicleUpdatePacket*) malloc(sizeof(VehicleUpdatePacket));
    PacketHeader update_head;
    char *vehicle_update = (char *)malloc(DIM_BUFF*sizeof(char));

    while(communication){


    //creazione di un pacchetto di update personale da inviare al server.
        
        if (verbosity_level>=DebugUDP) printf("\n[UDP SENDER] I'm alive!\n");

        slen = sizeof(struct sockaddr);
        update_head.type = VehicleUpdate;

        update->header=update_head;
        update->translational_force = veicolo->translational_force_update;
        update->rotational_force = veicolo->rotational_force_update;
        update->id = id;

        int vehicle_update_len = Packet_serialize(vehicle_update, &(update->header));
        if (verbosity_level>=DebugUDP) printf("[UDP SENDER] Serializzato pacchetto con le proprie forze  di lunghezza %d , io ho id %d \n", vehicle_update_len,update->id);
        
        if (verbosity_level>=DebugUDP) printf("[UDP SENDER] Forze: %f e %f \n",update->rotational_force , update->translational_force);

        if (!communication) break;

        /*bytes_sent = 0;*/

        ret = send_UDP(socket_UDP, vehicle_update, vehicle_update_len, 0, &server_UDP, (socklen_t) slen);
        if (ret == -2) {
            printf("Connection closed\n");
            break;
        }
        PTHREAD_ERROR_HELPER(ret, "Could not send vehicle updates to server");
        if (verbosity_level>=DebugUDP) printf("[UDP SENDER] Inviato pacchetto con le proprie forze\n");
        usleep(2000);

    }

    Packet_free((PacketHeader *) update);
    free(vehicle_update);
    free(arg);
    pthread_exit(NULL);

    /**uscire dal while, significa che il client si sta disconnettendo. Il server deve salvare il suo stato da qualche parte, per ripristinarlo più avanti
       se il client si connetterà ancora**/

    /**funzioni di send e receive per comunicazione UDP**/
    //sendto(int sockfd, void* buff, size_t #bytes, int flags, const struct sockaddr* to, socklen_t addrlen)
    //recvfrom(int sockfd, void* buff, size_t #bytes, int flags, const struct sockaddr* from, socklen_t addrlen)
}

void* thread_listener_udp_W(void* client_args){

    /**COMUNICAZIONE UDP**/
    /**

    Client, via UDP invia dei pacchetti VehicleUpdate al server. Il contenuto del VehicleUpdate viene prelevato da desired_force,
    è contenuto nella struttura del veicolo e si mette in attesa di pacchetti WorldUpdate che contengono al
    loro interno una lista collegata. Ricevuti questi pacchetti, smonta la lista collegata all'interno e per ogni elemento della lista,
    preso l'id, preleva dal mondo il veicolo con quell'id e ne aggiorna lo stato. I pacchetti di veicoli ancora non aggiunti al proprio mondo
    vengono ignorati (per necessità).

    **/

    int ret;

    thread_client_args* arg = (thread_client_args*) client_args;
    int socket_UDP = arg->socket_desc_UDP;
    struct sockaddr_in server_UDP = arg->server_addr_UDP;
    socklen_t slen;
    //int my_id = arg->id;


    /**
    Ciclo while che opera fino a quando il client è in funzione.
    **/

    char *world_update = (char *)malloc(DIM_BUFF*sizeof(char));
    WorldUpdatePacket *wup;
    ClientUpdate *client_update;    //Vettore di client updates
    ClientUpdate update;            //Singolo client update
    Vehicle *v;
    int dimensione_mondo;
    int bytes_sent = 0;

    while(communication){


    //richiesta di tutti gli update degli altri veicoli, per aggiornare il proprio mondo
    //da sistemare la dimensione

        if (verbosity_level>=DebugUDP) printf("\n[UDP RECEIVER] I'm alive!\n");

        //non sappiamo quanto è grande la stringa che ci deve arrivare e che dobbiamo convertire in numero intero

        if (verbosity_level>=DebugUDP) printf("[UDP RECEIVER] Mi metto in attesa del pacchetto world_update...\n");

        slen = sizeof(server_UDP);

        ret = recv_UDP_packet(socket_UDP, world_update, 0, (struct sockaddr*) &server_UDP, &slen, &bytes_sent);
        if (ret == -2) {
            printf("Connection closed\n");
            break;
        }

        if (verbosity_level>=DebugUDP) printf("[UDP RECEIVER] Ricevuto pacchetto col mondo aggiornato\n");

        if (!communication) break;

        if (verbosity_level>=DebugUDP) printf("[UDP RECEIVER] Sono stati ricevuti %d bytes\n", ret);

        PacketHeader* header = (PacketHeader*) world_update;
        if (header->size != ret) ERROR_HELPER(-1, "Partial read!");
        if (header->type == WorldUpdate){

            //estriamo il numero di veicoli e gli update di ogni veicolo

            dimensione_mondo = ret;

            wup = (WorldUpdatePacket*) Packet_deserialize(world_update, dimensione_mondo);
            if (verbosity_level>=DebugUDP) printf("[UDP RECEIVER] Deserializzato pacchetto con il mondo aggiornato\n");
            int num_vehicles = wup->num_vehicles;
            client_update = wup->updates; 

            int i;
            for(i=0;i<num_vehicles;i++){
                update = client_update[i];

                //estrapoliamo tutti i dati per il singolo veicolo presente nel mondo, identificato da "id"

                int id = update.id;
                float x = update.x;
                float y = update.y;
                float theta = update.theta;

                //Aggiornamento veicolo

                ret = sem_wait(&sem_world_c);
                PTHREAD_ERROR_HELPER(ret, "Failed to wait sem_world_c in thread_UDP_receiver");

                v = World_getVehicle(&world, id);
                if(v!=0 /*&& my_id != id*/){
                    if (verbosity_level>=DebugUDP) printf("[UDP RECEIVER] Posizioni veicolo %d ricevute \n", id);
                    v->x = x;
                    v->y = y;
                    v->theta = theta;
                }

                ret = sem_post(&sem_world_c);
                ERROR_HELPER(ret, "Failed to post sem_world_c in thread_UDP_receiver");

                if (verbosity_level>=DebugUDP) printf("[UDP RECEIVER] Data update!\n");
                Packet_free((PacketHeader *) wup);           // dealloco sia la lista degli update che la struttura
            }
        }

    }

    free(world_update);
    free(arg);
    pthread_exit(NULL);
    /**uscire dal while, significa che il client si sta disconnettendo. Il server deve salvare il suo stato da qualche parte, per ripristinarlo più avanti
       se il client si connetterà ancora**/

}


int main(int argc, char **argv) {

	if (argc<3) {
		printf("usage: %s <server_address> <player texture>\n", argv[1]);
		exit(-1);
	}

    if (verbosity_level>=General) printf("DEBUG MODE\n");

	printf("loading texture image from %s ... ", argv[2]);
	Image* my_texture = Image_load(argv[2]);
	if (my_texture) {
		printf("Done! \n");
	} else {
		printf("Fail! \n");
	}


    int ret;

    //inizializzo semaforo

    ret = sem_init(&sem_world_c, 0, 1);
    ERROR_HELPER(ret, "Failed to initialization of sem_world_c");

    //gestione segnali
    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sa.sa_flags = SA_RESTART;
    sigfillset(&sa.sa_mask);

    ret = sigaction(SIGINT, &sa, NULL);
    ERROR_HELPER(ret, "Could not handle SIGINT");
    ret = sigaction(SIGTERM, &sa, NULL);
    ERROR_HELPER(ret, "Could not handle SIGTERM");
    ret = sigaction(SIGQUIT, &sa, NULL);
    ERROR_HELPER(ret, "Could not handle SIGQUIT");
    ret = sigaction(SIGHUP, &sa, NULL);
    ERROR_HELPER(ret, "Could not handle SIGHUP");
    ret = sigaction(SIGSEGV, &sa, NULL);
    ERROR_HELPER(ret, "Could not handle SIGSEGV");
    ret = sigaction(SIGPIPE, &sa, NULL);
    ERROR_HELPER(ret, "Could not handle SIGPIPE");
    ret = sigaction(SIGALRM, &sa, NULL);
    ERROR_HELPER(ret, "Could not handle SIGALRM");


	Image* my_texture_for_server = my_texture;

	//variables for handling a socket
	struct sockaddr_in server_addr = {0};    //some fields are required to be filled with 0

	//creating a socket TCP
	socket_desc = socket(AF_INET, SOCK_STREAM, 0);
	ERROR_HELPER(socket_desc, "Could not create socket");

	//set up parameters for connection
	server_addr.sin_addr.s_addr = inet_addr(argv[1]);
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(SERVER_PORT_TCP);

	//initiate a connection to the socket
	ret = connect(socket_desc, (struct sockaddr*) &server_addr, sizeof(struct sockaddr_in));
	ERROR_HELPER(ret, "Could not connect to socket");

	//variable for UDP socket
	struct sockaddr_in server_addr_UDP;
	memset(&server_addr_UDP, 0, sizeof(server_addr_UDP));
	//creating UDP sopcket
	socket_desc_UDP = socket(AF_INET, SOCK_DGRAM, 0);
	ERROR_HELPER(socket_desc_UDP, "Could not create socket udp");
	//set up parameters
	server_addr_UDP.sin_addr.s_addr = inet_addr(argv[1]);
	server_addr_UDP.sin_family = AF_INET;
	server_addr_UDP.sin_port = htons(SERVER_PORT_UDP);


	/**LOGIN**/
	/** Client inserisce username e password appena si connette:
	*   -se utente non esiste allora i dati che ha inserito vengono usati per registrare l'utente
	*	-variabile login_state ha 3 valori 0 se nuovo utente registrato, 1 se già esistente e -1 se password sbagliata
	**/

  	char stato_login[DIM_BUFF];		// in this variabile there is the login's state
  	int user_length;
  	int pass_length;
    int login_state;

    while (1) {
        printf("LOGIN\nPlease enter username: ");
        ret = scanf("%s", username);
        if (verbosity_level>=DebugTCP) printf("[LOGIN] scanf fatta: %s\n", username);
        user_length = strlen(username);
        if (user_length > 64) printf("ERROR! username too length, please retry.\n");
        else break;
    }

    if (verbosity_level>=General) printf("[LOGIN] username inserito\n");

	ret = send_TCP(socket_desc, username, user_length+1, 0);
	ERROR_HELPER(ret, "Failed to send login data");

	if (verbosity_level>=DebugTCP) printf("[LOGIN] username INVIATO\n");

	ret = recv_TCP(socket_desc, stato_login, 1, 0);
	ERROR_HELPER(ret, "Failed to update login's state");

	if (verbosity_level>=DebugTCP) printf("[LOGIN] login state per username ricevuto\n");

    login_state = atoi(stato_login);

	if (login_state == 1) printf("\nWelcome back %s.", username);
	else if (login_state == 0) printf("\nWelcome %s.", username);
    else if (login_state == 999) {
        printf("\nIl client è già connesso. Riprovare più tardi\n");
        alarm(0);
        exit(0);
    }
	else {
		// Non c'è più posto tra gli user
		printf("\nÈ stato raggiunto il massimo numero di utenti. Riprova più tardi.\n");
		exit(0);
	}

	printf(" Please enter password: ");
	ret = scanf("%s", password);
	printf("\n");
	if (verbosity_level>=DebugTCP) printf("[LOGIN] scanf fatta: %s\n", password);
	pass_length = strlen(password);

	ret = send_TCP(socket_desc, password, pass_length+1, 0);
	ERROR_HELPER(ret, "Failed to send login data");

	if (verbosity_level>=DebugTCP) printf("[LOGIN] password INVIATA\n");

	ret = recv_TCP(socket_desc, stato_login, 1, 0);
	ERROR_HELPER(ret, "Failed to update login's state");

	if (verbosity_level>=DebugTCP) printf("[LOGIN] login state per password ricevuta\n");

    login_state = atoi(stato_login);

	while (login_state == -1) {
		printf("Incorrect Password, please insert it again: ");
		ret = scanf("%s", password);
		printf("\n");
		pass_length = strlen(password);

		ret = send_TCP(socket_desc, password, pass_length+1, 0);
		ERROR_HELPER(ret, "Failed to send login data");

		if (verbosity_level>=DebugTCP) printf("[LOGIN] Nuova password inviata\n");

		ret = recv_TCP(socket_desc, stato_login, 1, 0);
		ERROR_HELPER(ret, "Failed to receive login's state");

		if (verbosity_level>=DebugTCP) printf("[LOGIN] login state della password ricevuto\n");

        login_state = atoi(stato_login);
	}

	int my_id = -2;
	Image* my_texture_from_server = NULL;
    size_t msg_len;                 //utile dichiararla qui, visto che andrà usata in ogni caso
    int bytes_read = 0;
    ClientUpdate *my_coord = NULL; // qui salverò le vecchie coordinate ricevute dal server per poterle ripristinare se sono già registrato

	if (login_state == 0){

		printf("You're signed up with user: %s, welcome to the game!\n", username);	//password salvata

		//requesting and receving the ID
		IdPacket* request_id=(IdPacket*)malloc(sizeof(IdPacket));
		//PacketHeader id_head;
		//request_id->header=id_head;
		request_id->header.type=GetId;
		request_id->id = -1;

		char *idPacket_request = (char *)malloc(DIM_BUFF*sizeof(char));
		char *idPacket = (char *)malloc(DIM_BUFF*sizeof(char));

		size_t idPacket_request_len = Packet_serialize(idPacket_request,&(request_id->header));

		if (verbosity_level>=DebugTCP) printf("[IDPACKET] Sto per mandare idPacket\n");

        if (verbosity_level>=DebugTCP) printf("[IDPACKET] Sto per mandare: %d\n", (int) idPacket_request_len);

		ret = send_TCP(socket_desc, idPacket_request, idPacket_request_len, 0);
		ERROR_HELPER(ret, "Could not send id request  to socket");

        if (verbosity_level>=DebugTCP) printf("[IDPACKET] Byte inviati: %d\n", (int) ret);

        if (ret == idPacket_request_len){
            if (verbosity_level>=DebugTCP) printf("[IDPACKET] Tutto ok\n");
        }
        else{
            if (verbosity_level>=DebugTCP){
                printf("[IDPACKET] No buono\n");
                printf("[IDPACKET] ret = %d\n", ret);
                printf("[IDPACKET] idPacket_request_len = %d\n", (int) idPacket_request_len);
            }
        }

		if (verbosity_level>=DebugTCP) printf("[IDPACKET] idPacket richiesto\n");

        free(idPacket_request);
        Packet_free((PacketHeader *) request_id);

		ret = recv_TCP_packet(socket_desc, idPacket, 0, &bytes_read);
		ERROR_HELPER(ret, "Could not read id from socket");

        if (verbosity_level>=DebugTCP) printf("[IDPACKET] Byte letti: %d\n", (int) bytes_read);

        if (verbosity_level>=DebugTCP) printf("[IDPACKET] idPacket ricevuto\n");

        if (bytes_read != (int) sizeof(IdPacket)){
            if (verbosity_level>=DebugTCP) printf("[IDPACKET] C'è un problema! Il numero di byte arrivati non corrisponde!\n");
        }

		msg_len=bytes_read;

		IdPacket* id = (IdPacket*) Packet_deserialize(idPacket, msg_len);
		if (id->header.type!=GetId) ERROR_HELPER(-1,"Error in packet type \n");

        if (verbosity_level>=DebugTCP) printf("[IDPACKET] idPacket deserializzato\n");

        free(idPacket);

		// sending my texture
        char *texture_for_server = (char *)malloc(DIM_BUFF*sizeof(char));

        if (verbosity_level>=DebugTCP) printf("[TEXTURE] Alloco la mia texture\n");

        ImagePacket* my_texture = (ImagePacket*) malloc(sizeof(ImagePacket));
		//PacketHeader img_head;
		//my_texture->header=img_head;
		my_texture->header.type=PostTexture;
		my_texture->id=id->id;
		my_texture->image=my_texture_for_server;

        if (verbosity_level>=DebugTCP) printf("[TEXTURE] Sizeof ImagePacket: %d\n", (int) sizeof(ImagePacket));

        if (verbosity_level>=DebugTCP) printf("[TEXTURE] Pacchetto my_texture allocato. Serializzo.\n");

		size_t texture_for_server_len = Packet_serialize(texture_for_server, &(my_texture->header));

        if (verbosity_level>=DebugTCP) printf("[TEXTURE] Texture serializzata.\n");

        if (verbosity_level>=DebugTCP) printf("[TEXTURE] Sto per inviare: %d\n", (int) texture_for_server_len);

		ret = send_TCP(socket_desc, texture_for_server, texture_for_server_len, 0);
		ERROR_HELPER(ret, "Could not send my texture for server");

        if (verbosity_level>=DebugTCP) printf("[TEXTURE] texture_for_server inviata\n");

        if (verbosity_level>=DebugTCP) printf("[TEXTURE] Byte inviati: %d\n", (int) ret);

        if (ret == texture_for_server_len){
            if (verbosity_level>=DebugTCP) printf("[TEXTURE] Tutto ok\n");
        }
        else{
            if (verbosity_level>=DebugTCP){
                printf("[TEXTURE] No buono\n");
                printf("[TEXTURE] ret = %d\n", ret);
                printf("[TEXTURE] texture_for_server_len = %d\n", (int) texture_for_server_len);
            }
        }

        free(texture_for_server);
        my_texture->image = NULL;
        Packet_free((PacketHeader *) my_texture);

		// receving my texture from server

        if (verbosity_level>=DebugTCP) printf("[TEXTURE] Attendo la mia texture dal server\n");

		char *my_texture_server = (char *)malloc(DIM_BUFF*sizeof(char));

        bytes_read = 0;

		ret = recv_TCP_packet(socket_desc, my_texture_server, 0, &bytes_read);
		ERROR_HELPER(ret, "Could not read my texture from socket");

        if (verbosity_level>=DebugTCP) printf("[TEXTURE] Byte letti: %d\n", bytes_read);

        if (verbosity_level>=DebugTCP) printf("[TEXTURE] my_texture_server ricevuta\n");

		msg_len=bytes_read;

        if (verbosity_level>=DebugTCP) printf("[TEXTURE] Deserializzo la mia texture ricevuta\n");

		ImagePacket* my_texture_received = (ImagePacket*) Packet_deserialize(my_texture_server,msg_len);
		/* Potrebbe dare problemi: confronta gli indirizzi a cui puntano e non il contenuto
        if(my_texture_received!=my_texture) ERROR_HELPER(-1,"error in communication: texture not matching! \n");*/

        if (verbosity_level>=DebugTCP) printf("[CLIENT] Texture deserializzata. Aggiorno i miei parametri id e texture\n");
        if (verbosity_level>=DebugTCP) printf("[CLIENT] La Texture deserializzata ha campo size = %d\n", my_texture_received->header.size);

        free(my_texture_server);

		// these come from the server
		my_id = id->id;
		my_texture_from_server = my_texture_received->image;
        
        my_texture_received->image = NULL;
        Packet_free((PacketHeader *) my_texture_received);

        if (verbosity_level>=DebugTCP) printf("[CLIENT] Parametri aggiornati\n"); 
    }

   	else if (login_state == 1) {
   		printf("Login success, welcome back %s\n", username);
		//requesting and receving texture and id
		IdPacket* request_texture=(IdPacket*)malloc(sizeof(IdPacket));
		//PacketHeader request_texture_head;
		//request_texture->header=request_texture_head;
		request_texture->id=-1; //ancora non lo conosco lo scopro nella risposta
		request_texture->header.type=GetTexture;

		char *request_texture_for_server = (char *)malloc(DIM_BUFF*sizeof(char));
		char *my_texture = (char *)malloc(DIM_BUFF*sizeof(char));
        char *my_coord_buf = (char *)malloc(DIM_BUFF*sizeof(char));

		size_t request_texture_len = Packet_serialize(request_texture_for_server, &(request_texture->header));

        if (verbosity_level>=DebugTCP) printf("[CLIENT] Invio la richiesta di texture al server\n");

		ret = send_TCP(socket_desc, request_texture_for_server, request_texture_len, 0);
		ERROR_HELPER(ret, "Could not send my texture for server");

        if (verbosity_level>=DebugTCP) printf("[CLIENT] Richiesta inviata\n");

        free(request_texture_for_server);
        Packet_free((PacketHeader *) request_texture);

        if (verbosity_level>=DebugTCP) printf("[CLIENT] Ricevo la texture dal server\n");

		ret = recv_TCP_packet(socket_desc, my_texture, 0, &bytes_read);
		ERROR_HELPER(ret, "Could not read my texture from socket");

        if (verbosity_level>=DebugTCP) printf("[CLIENT] Texture ricevuta\n");

		msg_len=bytes_read;

		ImagePacket* my_texture_received = (ImagePacket*) Packet_deserialize(my_texture,msg_len);
		if(my_texture_received->header.type!=PostTexture && my_texture_received->id==0) ERROR_HELPER(-1,"error in communication \n");

        free(my_texture);

		// these come from the server
		my_id = my_texture_received->id;
		my_texture_from_server = my_texture_received->image;

        my_texture_received->image = NULL;
        Packet_free((PacketHeader *) my_texture_received);

        if (verbosity_level>=DebugTCP) printf("[CLIENT] Ricevo le vecchie coordinate dal server\n");

        // ricevo le vecchie coordinate
        ret = recv_TCP(socket_desc, my_coord_buf, sizeof(ClientUpdate), 0);
        if (ret != sizeof(ClientUpdate)) ERROR_HELPER(-1, "Could not receive old position");

        if (verbosity_level>=DebugTCP) printf("[CLIENT] Coordinate ricevute\n");

        my_coord = (ClientUpdate *)malloc(DIM_BUFF*sizeof(ClientUpdate));
        memcpy(my_coord, my_coord_buf, sizeof(ClientUpdate));

        free(my_coord_buf);
	}

    if (verbosity_level>=DebugTCP) printf("[ELEVATION_MAP] Faccio la richiesta di elevation_map\n");

	//requesting and receving elevation map
	IdPacket* request_elevation=(IdPacket*)malloc(sizeof(IdPacket));
	PacketHeader request_elevation_head;
	request_elevation->header=request_elevation_head;
	request_elevation->id=my_id;
	request_elevation->header.type=GetElevation;

    if (verbosity_level>=DebugTCP) printf("[ELEVATION_MAP] Pacchetto richiesta elevation_map creato\n");

	char *request_elevation_for_server = (char *)malloc(DIM_BUFF*sizeof(char));
	char *elevation_map = (char *)malloc(DIM_BUFF*sizeof(char));

    if (verbosity_level>=DebugTCP) printf("[ELEVATION_MAP] Serializzo il pacchetto\n");

	size_t request_elevation_len =Packet_serialize(request_elevation_for_server, &(request_elevation->header));

    if (verbosity_level>=DebugTCP) printf("[ELEVATION_MAP] Pacchetto serializzato\n");

	ret = send_TCP(socket_desc, request_elevation_for_server, request_elevation_len, 0);
	ERROR_HELPER(ret, "Could not send my texture for server");

    if (verbosity_level>=DebugTCP) printf("[ELEVATION_MAP] Byte inviati: %d\n", ret);

    if (ret == request_elevation_len){
        if (verbosity_level>=DebugTCP) printf("[ELEVATION_MAP] Tutto ok\n");
    }
    else{
        if (verbosity_level>=DebugTCP){
            printf("[ELEVATION_MAP] No buono\n");
            printf("[ELEVATION_MAP] ret = %d\n", ret);
            printf("[ELEVATION_MAP] request_elevation_len = %d\n", (int) request_elevation_len);
        }
    }

    if (verbosity_level>=DebugTCP) printf("[ELEVATION_MAP] Pacchetto di richiesta inviato\n");

    free(request_elevation_for_server);
    Packet_free((PacketHeader *) request_elevation);

    if (verbosity_level>=DebugTCP) printf("[ELEVATION_MAP] Attendo il pacchetto di elevation_map dal server\n");



    bytes_read=0;

    ret = recv_TCP_packet(socket_desc, elevation_map,0,&bytes_read);
    ERROR_HELPER(ret, "Could not read elevation map from socket");
    
    msg_len=bytes_read;

    if (verbosity_level>=DebugTCP) printf("[ELEVATION_MAP] Byte letti: %d\n", (int)msg_len);
    ImagePacket* elevation=(ImagePacket*)Packet_deserialize(elevation_map,msg_len);
    if (verbosity_level>=DebugTCP) printf("[ELEVATION_MAP] Pacchetto deserializzato \n");
 

    free(elevation_map);


	//requesting and receving map

    if (verbosity_level>=DebugTCP) printf("[MAP] Richiedo la mappa al server\n");

	char *request_texture_map_for_server = (char *)malloc(DIM_BUFF*sizeof(char)); //buffer per la richiesta della mappa

    if (verbosity_level>=DebugTCP) printf("[MAP] Creo il pacchetto di richiesta\n");

	IdPacket* request_map=(IdPacket*)malloc(sizeof(IdPacket));
	PacketHeader request_map_head;
	request_map->header=request_map_head;
	request_map->header.type=GetTexture;
	request_map->id=my_id;

    if (verbosity_level>=DebugTCP) printf("[MAP] Pacchetto creato. Serializzo\n");

	size_t request_texture_map_for_server_len=Packet_serialize(request_texture_map_for_server, &(request_map->header));

    if (verbosity_level>=DebugTCP) printf("[MAP] Serializzazione completata, invio del pacchetto in corso...\n");

  	ret = send_TCP(socket_desc, request_texture_map_for_server, request_texture_map_for_server_len, 0);
  	ERROR_HELPER(ret, "Could not send my texture for server");

    if (verbosity_level>=DebugTCP) printf("[MAP] Byte inviati: %d\n", ret);

    if (verbosity_level>=DebugTCP) printf("[MAP] Invio del pacchetto avvenuto con successo!\n");

    free(request_texture_map_for_server);
    Packet_free((PacketHeader *) request_map);

    if (verbosity_level>=DebugTCP) printf("[MAP] Attendo la mappa dal server\n");


    char *texture_map = (char *)malloc(DIM_BUFF*sizeof(char));                    //buffer per la ricezione della mappa

    bytes_read=0;

  	ret = recv_TCP_packet(socket_desc, texture_map,0,&bytes_read);

  	ERROR_HELPER(ret, "Could not read map texture from socket");

    msg_len = bytes_read;

    if (verbosity_level>=DebugTCP) printf("[MAP] Byte letti: %ld\n", msg_len);

    ImagePacket* map =(ImagePacket*)Packet_deserialize(texture_map,msg_len);
    if (verbosity_level>=DebugTCP) printf("[MAP] Paccheto deserializzato \n");


    free(texture_map);
 
	// these come from the server

    if (verbosity_level>=DebugTCP) printf("[CLIENT] Aggiorno i parametri\n");

	Image* map_elevation = elevation->image;
	Image* map_texture = map->image;

    elevation->image = NULL;
    map->image = NULL;
    Packet_free((PacketHeader *) elevation);
    Packet_free((PacketHeader *) map);

    if (verbosity_level>=DebugTCP) printf("[CLIENT] Parametri aggiornati. Costruzione del mondo\n");

	// construct the world
	World_init(&world, map_elevation, map_texture, 0.5, 0.5, 0.5);
	vehicle=(Vehicle*) malloc(sizeof(Vehicle));
    Vehicle_init(vehicle, &world, my_id, my_texture_from_server);
    // prendo le vecchie coordinate se sono un utente già registrato
    if (my_coord != NULL) {
        if (verbosity_level>=DebugTCP) printf("[CLIENT] Ecco le mie vecchie coordinate: "
            "(x,y,theta) = (%f,%f,%f)\n", my_coord->x, my_coord->y, my_coord->theta);
        vehicle->x = my_coord->x; vehicle->prev_x = my_coord->x;
        vehicle->y = my_coord->y; vehicle->prev_y = my_coord->y;
        vehicle->theta = my_coord->theta; vehicle->prev_theta = my_coord->theta;
        free(my_coord);
    }
	World_addVehicle(&world, vehicle);

	// spawn a thread that will listen the update messages from
	// the server, and sends back the controls
	// the update for yourself are written in the desired_*_force
	// fields of the vehicle variable
	// when the server notifies a new player has joined the game
	// request the texture and add the player to the pool
	/*FILLME*/
	thread_client_args* args=malloc(sizeof(thread_client_args));
	args->v=vehicle;
	args->id=my_id;
	args->socket_desc_TCP=socket_desc;
	args->socket_desc_UDP=socket_desc_UDP;
	args->map_texture=map_texture;
	args->server_addr_UDP = server_addr_UDP;
	pthread_t thread_tcp;
	pthread_t thread_udp_M;
	pthread_t thread_udp_W;

	ret = pthread_create(&thread_tcp, NULL, thread_listener_tcp, (void*) args);
	ERROR_HELPER(ret, "Could not create thread");

    if (verbosity_level>=DebugTCP) printf("TCP thread created\n");

    ret = pthread_create(&thread_udp_M, NULL, thread_listener_udp_M, (void*) args);
    ERROR_HELPER(ret, "Could not create thread");

    if (verbosity_level>=DebugTCP) printf("UDP sender thread created\n");

    ret = pthread_create(&thread_udp_W, NULL, thread_listener_udp_W, (void*) args);
    ERROR_HELPER(ret, "Could not create thread");

    if (verbosity_level>=DebugTCP) printf("UDP receiver thread created\n");

	ret = pthread_detach(thread_tcp);
	ERROR_HELPER(ret, "Could not detach thread");

	ret = pthread_detach(thread_udp_M);
	ERROR_HELPER(ret, "Could not detach thread");

	ret = pthread_detach(thread_udp_W);
	ERROR_HELPER(ret, "Could not detach thread");

	WorldViewer_runGlobal(&world, vehicle, &argc, argv);

	return 0;
}
