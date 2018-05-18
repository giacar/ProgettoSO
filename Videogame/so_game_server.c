// #include <GL/glut.h> // not needed here
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <semaphore.h>
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>

#include "image.h"
#include "surface.h"
#include "world.h"
#include "vehicle.h"
#include "world_viewer.h"
#include "utils.h"
#include "common.h"
#include "so_game_protocol.h"

user_table utenti[MAX_USER_NUM];
// Creare lista di tutti i client connessi che verranno man mano aggiunti e rimossi
// deve contenere ID texture. Stessa cosa vale con una lista di client disconnessi
// in modo da ripristinare lo stato in caso di un nuovo login
clients  client[MAX_USER_NUM];    //vettore di puntatori a strutture dati clients
ListHead mov_int_list;             //lista delle intenzioni dei movimenti
int num_online = 0;                //numero degli utenti online
World world;                       //mondo


sem_t sem_utenti;
sem_t sem_thread_UDP;
sem_t sem_online;

/**
NOTA PER IL SERVER: Quando c'è un errore su una socket di tipo ENOTCONN (sia UDP che TCP), allora vuol dire che il client si è disconnesso (indipendetemente
se la disconnessione è avvenuta in maniera pulita oppure no). Ciò si verifica anche quando la recv e la receive_from restituiscono 0. In tal caso, il server
dovrà salvare lo stato del client che si è disconnesso (semplicemente, si sposta il suo contenuto dalla sua cella di online_client alla sua cella di
offline_client, e poi si mette a NULL la sua cella in online_client.
**/




void* thread_server_TCP(void* args){

    int ret, i;

    //implementare protocollo login e aggiungere il nuovo client al mondo ed alla lista dei client connessi
    thread_server_TCP_args *arg = (thread_server_TCP_args *)args;
    int socket=arg->socket_desc_TCP_client;
    Image* elevation_map = arg->elevation_map;      //elevation map
    Image* map = arg->map;                          //map
    clients* client=arg->list;

    // Strutture dati per il Login
    char user_att[64];
    char pass_att[64];
    char pass_giusta[64];
    int login_reply;
    int login_status;        //questa variabile serve per far capire al server se il client che si è connesso è un nuovo utente oppure si era già registrato
    char risposta_login[21];

    //l'utilizzo della sola variabile login_reply non permette di identificare i due tipi di client, perchè viene modificata per altri scopi

    // Ricezione dell'user

    ret = recv_TCP(socket, user_att, 1, 0);
    if (ret == -2){
        printf("Could not receive client username\n");
        pthread_exit(NULL);
    }
    else PTHREAD_ERROR_HELPER(ret, "Failed to read username from client");

    if (DEBUG) {
        int value;
        ret = sem_getvalue(&sem_utenti, &value);
        printf("[LOGIN] Valore semaforo prima della wait = %d\n", value);
    }

    ret = sem_wait(&sem_utenti);
    ERROR_HELPER(ret, "Error in sem_utenti wait");

    if (DEBUG) printf("[LOGIN] Controllo se già registrato\n");

	// Verifico se user già registrato
	int idx = -1;
	for (i = 0; strlen(utenti[i].username)>0; i++) {
		if (strcmp(user_att,utenti[i].username) == 0) {
			idx = i;
		}
	}

    if (DEBUG) printf("[LOGIN] Fine Controllo\n");

	// Nuovo user
	if (idx == -1) {
		// inserisco user nel primo slot libero
		login_reply = 0;	// È un nuovo user
        login_status = login_reply;
		for (i = 0; i < MAX_USER_NUM && strlen(utenti[i].username)>0; i++);

		/**
			Slot utenti disponibili terminati
			Devo informare il client che gli slot sono terminati, in modo che termini e che il thread corrente termini
		**/
		if (i >= MAX_USER_NUM){
            login_reply = -1;
        }

		else {
			idx = i;
			strcpy(utenti[idx].username,user_att);
		}

		ret = sem_post(&sem_utenti);
		ERROR_HELPER(ret, "Error in sem_utenti post");

        if (DEBUG) {
            int value;
            ret = sem_getvalue(&sem_utenti, &value);
            printf("[LOGIN] Valore semaforo dopo post in seguito a registrazione utente = %d\n", value);
        }

		// informo il client che è un nuovo user o che gli slot sono terminati

        sprintf(risposta_login, "%d", login_reply);

        ret = send_TCP(socket, risposta_login, strlen(risposta_login)+1, 0);
        if (ret == -2) {
            printf("Could not send login_reply to client\n");
            pthread_exit(NULL);
        }
        else PTHREAD_ERROR_HELPER(ret, "Failed to send login_reply to client");

		if (login_reply == -1){
            ret=close(socket);
            pthread_exit(NULL);		// Slot user terminati devo terminare il thread corrente
        }

        //ricezione password

        ret = recv_TCP(socket, pass_att, 1, 0);
        if (ret == -2){
             printf("Could not receive password from client\n");
             pthread_exit(NULL);
        }
        else PTHREAD_ERROR_HELPER(ret, "Failed to read password from client");

        login_reply = 0;
        sprintf(risposta_login, "%d", login_reply);

        ret = send_TCP(socket, risposta_login, strlen(risposta_login)+1, 0);
        if (ret == -2) {
            printf("Could not send login_reply to client\n");
            pthread_exit(NULL);
        }
        else PTHREAD_ERROR_HELPER(ret, "Failed to send login_reply to client");

        if (DEBUG) {
            int value;
            ret = sem_getvalue(&sem_utenti, &value);
            printf("[LOGIN] Valore semaforo prima di wait per registrazione password = %d\n", value);
        }

		// registrazione password ed id
		ret = sem_wait(&sem_utenti);
        PTHREAD_ERROR_HELPER(ret, "Error in sem_utenti wait: failed to register username & password");

		strcpy(utenti[idx].password,pass_att);

        if (DEBUG) printf("[LOGIN] Ho salvato la password\n");

		ret = sem_post(&sem_utenti);
        ERROR_HELPER(ret, "Error in sem_utenti post: failed to register username & password");

        if (DEBUG) {
            int value;
            ret = sem_getvalue(&sem_utenti, &value);
            printf("[LOGIN] Valore semaforo alla fine del login = %d\n", value);
        }


	}

	// Già registrato
	else if (idx > -1) {
        strcpy(pass_giusta,utenti[idx].password);

		ret = sem_post(&sem_utenti);
        ERROR_HELPER(ret, "Error in sem_utenti post");

		// informo il client che è già registrato
		login_reply = 1;
        login_status = login_reply;

        sprintf(risposta_login, "%d", login_reply);

        ret = send_TCP(socket, risposta_login, strlen(risposta_login)+1, 0);
        if (ret == -2) {
            printf("Could not send login_reply to client\n");
            pthread_exit(NULL);
        }
        else PTHREAD_ERROR_HELPER(ret, "Failed to send login_reply to client");

		//ricezione password
		do {

            ret = recv_TCP(socket, pass_att, 1, 0);
            if (ret == -2) {
                printf("Could not receive password from client\n");
                pthread_exit(NULL);
            }
            else PTHREAD_ERROR_HELPER(ret, "Failed to read password from client");

			// password giusta
			if (!strcmp(pass_att,pass_giusta)) {
				login_reply = 1;

                sprintf(risposta_login, "%d", login_reply);

                ret = send_TCP(socket, risposta_login, strlen(risposta_login)+1, 0);
                if (ret == -2) {
                    printf("Could not send login_reply to client\n");
                    pthread_exit(NULL);
                }
			}

			else {
				login_reply = -1;

                sprintf(risposta_login, "%d", login_reply);

                ret = send_TCP(socket, risposta_login, strlen(risposta_login)+1, 0);
                if (ret == -2){
                     printf("Could not send login_reply to client\n");
                     pthread_exit(NULL);
                }
                else PTHREAD_ERROR_HELPER(ret, "Failed to send login_reply to client");
			}
		} while (strcmp(pass_att,pass_giusta) != 0);



	}





    //nuovo utente: richiesta di id

    if (login_status == 0){

        if (DEBUG) printf("[IDPACKET] Sono entrato nel <if (login_status == 0)>\n");

        char* idPacket = (char*) calloc(DIM_BUFF+1, sizeof(char));

        ret = recv_TCP(socket, idPacket, sizeof(idPacket)+1, 0);
        if (ret == -2){
            printf("Could not receive id request from client\n");
            pthread_exit(NULL);
        }
        PTHREAD_ERROR_HELPER(ret, "Could not receive id request from client");

        if (DEBUG) printf("[IDPACKET] Ho ricevuto idPacket request dal client\n");

        size_t msg_len = ret-1;
        /*idPacket[msg_len] = '\0';
        msg_len++;      Ho messo nel client lo '\0' */

        IdPacket* id = (IdPacket*) Packet_deserialize(idPacket, msg_len);
        if (id->header.type != GetId) PTHREAD_ERROR_HELPER(-1, "Error in packet type (client id)");

        if (DEBUG) printf("[IDPACKET] Ho deserializzato l'idPacket\n");

        if (DEBUG) printf("[IDPACKET] Accedo ai dati dell'id request\n");

        if (id->id == -1){
            id->id = idx;
            size_t idPacket_response_len;
            idPacket_response_len = Packet_serialize(idPacket, &(id->header));
            idPacket[idPacket_response_len] = '\0';

            ret = send_TCP(socket, idPacket, idPacket_response_len+1, 0);
            if (ret == -2){
                printf("Could not send id response to client\n");
                pthread_exit(NULL);
            }
            PTHREAD_ERROR_HELPER(ret, "Could not send id response to client\n");

            if (DEBUG) printf("[IDPACKET] IdPacket inviato al client\n");


        }
        else{
            PTHREAD_ERROR_HELPER(-1, "Error in id packet request!");
            pthread_exit(NULL);
        }

        free(idPacket);

        //nuovo utente: invia la sua texture

        if (DEBUG) printf("[TEXTURE] Attendo la texture dal client\n");

        char* texture_utente = (char*) calloc(DIM_BUFF+1, sizeof(char));

        ret = recv_TCP(socket, texture_utente, sizeof(texture_utente), 0);
        if (ret == -2){
            printf("Could not receive client texture\n");
            pthread_exit(NULL);
        }
        PTHREAD_ERROR_HELPER(ret, "Could not receive client texture");

        if (DEBUG) printf("[TEXTURE] Texture ricevuta dal client. Deserializzo.\n");

        msg_len = ret-1;
        /*texture_utente[msg_len] = '\0';
        msg_len++;*/ // Ho aggiunto '\0' nel client

        ImagePacket* client_texture = (ImagePacket*) Packet_deserialize(texture_utente, msg_len);
        if (client_texture->header.type!=PostTexture) PTHREAD_ERROR_HELPER(-1, "Error in client texture packet!");

        if (DEBUG) printf("[TEXTURE] Texture deserializzata\n");

        client[idx].id = idx;
        client[idx].texture = client_texture->image;
        client[idx].status = 1;
        client[idx].addr=arg->addr;
        client[idx].socket_TCP=arg->socket_desc_TCP_client;


        size_t texture_utente_len;
        texture_utente_len = Packet_serialize(texture_utente, &(client_texture->header));
        texture_utente[texture_utente_len] = '\0';

        if (DEBUG) printf("[TEXTURE] Invio al client la sua texture\n");

        ret = send_TCP(socket, texture_utente, texture_utente_len+1, 0);
        if (ret == -2){
            printf("Could not send client texture\n");
            client[idx].status = 0;
            pthread_exit(NULL);
        }
        else PTHREAD_ERROR_HELPER(ret, "Could not send client texture\n");

        if (DEBUG) printf("[TEXTURE] Texture inviata al client con successo\n");

        free(texture_utente);

         // create a vehicle
		Vehicle* vehicle=(Vehicle*) malloc(sizeof(Vehicle));
		Vehicle_init(vehicle, &world, idx, client[idx].texture);

        if (DEBUG) printf("[VEHICLE] Veicolo del client creato\n");

		//  add it to the world
		World_addVehicle(&world, vehicle);

        if (DEBUG) printf("[WORLD] Veicolo aggiunto al mondo con successo\n");

    }

    //utente gia registrato

    else if (login_status == 1){

        /**client invia un IdPacket, con id settato a -1, noi rispondiamo con un ImagePacket, contenente la sua texture (salvata precedentemente) e il suo id
         * in pratica gli inviamo tutto insieme
        **/

        char* idPacket_buf = (char*) calloc(DIM_BUFF+1, sizeof(char));
        size_t idPacket_buf_len;

        ret = recv_TCP(socket, idPacket_buf, sizeof(idPacket_buf)+1, 0);
        if (ret == -2) {
            printf("Could not receive IdPacket from client\n");
            pthread_exit(NULL);
        }
        else PTHREAD_ERROR_HELPER(ret, "Could not receive IdPacket from client");

        size_t msg_len = ret-1;
        /*idPacket_buf[msg_len] = '\0';
        msg_len++;*/

        IdPacket* received = (IdPacket*) Packet_deserialize(idPacket_buf, msg_len);
        if (received->header.type != GetTexture) PTHREAD_ERROR_HELPER(-1, "Connection error (texture request)");

        client[idx].status = 1;
        client[idx].addr=arg->addr;
        client[idx].socket_TCP=arg->socket_desc_TCP_client;

        ImagePacket* client_texture = (ImagePacket*) malloc(sizeof(ImagePacket));
        PacketHeader client_header;
        client_texture->header = client_header;
        client_texture->id = idx;
        client_texture->image = client[idx].texture;
        client_texture->header.type = PostTexture;
        client_texture->header.size = sizeof(ImagePacket);

        idPacket_buf_len = Packet_serialize(idPacket_buf, &(client_texture->header));
        idPacket_buf[idPacket_buf_len] = '\0';

        ret = send_TCP(socket, idPacket_buf, idPacket_buf_len+1, 0);
        if (ret == -2) {
            printf("Could not send client texture and id\n");
            client[idx].status = 0;
            pthread_exit(NULL);
        }
        else PTHREAD_ERROR_HELPER(ret, "Could not send client texture and id");

        free(idPacket_buf);

         // create a vehicle
		Vehicle* vehicle=(Vehicle*) malloc(sizeof(Vehicle));
		Vehicle_init(vehicle, &world, idx, client[idx].texture);

		//  add it to the world
		World_addVehicle(&world, vehicle);

    }

    if (DEBUG) printf("[ELEVATION_MAP] Attendo la richiesta di elevation_map dal client\n");


    //utente invia la richiesta di elevation map

    char* elevation_map_buffer = (char*) calloc(DIM_BUFF+1, sizeof(char));
    size_t elevation_map_len;

    ret = recv_TCP(socket, elevation_map_buffer, sizeof(elevation_map_buffer)+1, 0);
    if (ret == -2) {
        printf("Could not receive elevation map request\n");
        client[idx].status = 0;
        pthread_exit(NULL);
    }
    else PTHREAD_ERROR_HELPER(ret, "Could not receive elevation map request");

    if (DEBUG) printf("[ELEVATION_MAP] Richiesta ricevuta. Deserializzo il pacchetto\n");

    size_t msg_len = ret-1;
    /*elevation_map_buffer[msg_len] = '\0';
    msg_len++;*/

    IdPacket* elevation= (IdPacket*) Packet_deserialize(elevation_map_buffer,msg_len);
    if(elevation->header.type!=GetElevation) ERROR_HELPER(-1,"error in communication \n");

    if (DEBUG) printf("[ELEVATION_MAP] Deserializzazione completata\n");

    int id = elevation->id;

    if (DEBUG) printf("[ELEVATION_MAP] Creo il pacchetto di elevation_map\n");

    ImagePacket* ele_map = (ImagePacket*) malloc(sizeof(ImagePacket));
    PacketHeader elevation_header;
    ele_map->header = elevation_header;
    ele_map->image = elevation_map;
    ele_map->id = id;
    ele_map->header.type = PostElevation;
    ele_map->header.size = sizeof(ImagePacket);

    if (DEBUG) printf("[ELEVATION_MAP] Pacchetto creato, serializzo\n");

    elevation_map_len = Packet_serialize(elevation_map_buffer, &(ele_map->header));
    elevation_map_buffer[elevation_map_len] = '\0';

    if (DEBUG) printf("[ELEVATION_MAP] Serializzazione completata, invio il pacchetto\n");

    ret = send_TCP(socket, elevation_map_buffer, elevation_map_len+1, 0);
    if (ret == -2) {
        printf("Could not send elevation map\n");
        client[idx].status = 0;
        pthread_exit(NULL);
    }
    else PTHREAD_ERROR_HELPER(ret, "Could not send elevation map to client");

    if (DEBUG) printf("[ELEVATION_MAP] Invio del pacchetto avvenuto con successo\n");

    free(elevation_map_buffer);

    //ricezione richiesta mappa e invio mappa

    if (DEBUG) printf("[MAP] Attendo la richiesta di mappa dal client\n");

    char* map_buffer = (char*) calloc(DIM_BUFF+1, sizeof(char));
    size_t map_len;

    ret = recv_TCP(socket, map_buffer, sizeof(map_buffer)+1, 0);
    if (ret == -2) {
        printf("Could not receive map request\n");
        client[idx].status = 0;
        pthread_exit(NULL);
    }
    else PTHREAD_ERROR_HELPER(ret, "Could not receive map request");

    if (DEBUG) printf("[MAP] Richiesta ricevuta. Deserializzo\n");

    msg_len = ret-1;
    /*map_buffer[msg_len] = '\0';
    msg_len++;*/

    IdPacket* map_request = (IdPacket*) Packet_deserialize(map_buffer, msg_len);
    if(map_request->header.type!=GetTexture)   ERROR_HELPER(-1, "Connection error (map request)");

    if (DEBUG) printf("[MAP] Deserializzazione completata\n");

    id = map_request->id;

    if (DEBUG) printf("[MAP] Creo il pacchetto della mappa\n");

    //invio mappa
    ImagePacket* map_packet = (ImagePacket*) malloc(sizeof(ImagePacket));
    PacketHeader map_header;
    map_packet->header = map_header;
    map_packet->id = id;
    map_packet->image = map;
    map_packet->header.type = PostTexture;
    map_packet->header.size = sizeof(ImagePacket);

    if (DEBUG) printf("[MAP] Pacchetto mappa creato. Serializzo\n");

    map_len = Packet_serialize(map_buffer, &(map_packet->header));
    map_buffer[map_len] = '\0';

    if (DEBUG) printf("[MAP] Serializzazione completata. Invio del pacchetto in corso...\n");

    ret = send_TCP(socket, map_buffer, map_len+1, 0);
    if (ret == -2){
        printf("Could not send map to client\n");
        client[idx].status = 0;
        pthread_exit(NULL);
    }
    else PTHREAD_ERROR_HELPER(ret, "Could not send map to client\n");

    if (DEBUG) printf("[MAP] Invio del pacchetto avvenuto con successo!\n");

    free(map_buffer);

    /** Ultimata la connessione e l'inizializzazione del client, c'è bisogno di inviargli lo stato di tutti gli altri client già connessi **/

    ImagePacket* client_alive = (ImagePacket*) malloc(sizeof(ImagePacket));
    PacketHeader client_alive_header;
    client_alive->header = client_alive_header;
    client_alive->header.type=PostTexture;
    client_alive->header.size=sizeof(ImagePacket);

    char client_alive_buf[DIM_BUFF];
    size_t alive_len;

    //inviamo  le texture di tutti i client connessi

    for (i = 0; i < MAX_USER_NUM; i++){
        if (i != idx && client[i].status==1){
            client_alive->id = client[i].id;
            client_alive->image = client[i].texture;

            alive_len = Packet_serialize(client_alive_buf, &(client_alive->header));
            client_alive_buf[alive_len] = '\0';

            ret = send_TCP(socket, client_alive_buf, alive_len+1, 0);
            if (ret == -2){
                printf("Could not send user data to client\n");
                client[idx].status = 0;
                pthread_exit(NULL);
            }
            else PTHREAD_ERROR_HELPER(ret, "Could not send user data to client\n");
        }
    }

    //Packet_free(&client_alive_header);  DISCUTERNE L'APPLICAZIONE

    //inviamo a tutti i client attualmente connessi la texture del nuovo client appena arrivato
    ImagePacket* texture=(ImagePacket*)malloc(sizeof(ImagePacket));
	PacketHeader head;
	head.type=PostTexture;
    head.size=sizeof(ImagePacket);
	texture->header=head;
	texture->id=idx;
	texture->image=client[idx].texture;
	char texture_buffer[DIM_BUFF];
	size_t texture_len=Packet_serialize(texture_buffer,&(texture->header));
    texture_buffer[texture_len] = '\0';

    for(i=0;i<MAX_USER_NUM;i++){
		if(i!=idx && client[i].status == 1){
			ret = send_TCP(client[i].socket_TCP, texture_buffer, texture_len+1, 0);
            if (ret == -2){
                printf("Could not send user data to client\n");
                client[i].status = 0;
                pthread_exit(NULL);
            }
            else PTHREAD_ERROR_HELPER(ret, "Could not send user data to client\n");
        }
    }

    //Packet_free(&head);

	char test_buf[DIM_BUFF];
	size_t test_len;

	IdPacket* test=(IdPacket*)malloc(sizeof(IdPacket));
	PacketHeader testh;
	testh.type=GetId;
	testh.size=sizeof(IdPacket);
	test->header=testh;
	test->id=90;

	test_len=Packet_serialize(test_buf,&(test->header));
    test_buf[test_len] = '\0';
	while(1){
		ret = send_TCP(socket, test_buf, test_len+1, 0);
		if (ret == -2){
			for(i=0;i<MAX_USER_NUM;i++){
				if(client[i].status==1 && i!=idx){
					test->id=idx;
					test_len=Packet_serialize(test_buf,&(test->header));
					ret = send_TCP(client[i].socket_TCP, test_buf, test_len, 0);
				}
			}
			pthread_exit(NULL);
		}
		else PTHREAD_ERROR_HELPER(ret, "Could not send user data to client\n");
		sleep(1000);
	}

    /** FINE LAVORO TCP **/




}

void* thread_server_UDP_sender(void* args){

    // server world update, send by server (UDP)
/**typedef struct {
  PacketHeader header;
  int num_vehicles;
  ClientUpdate* updates;
} WorldUpdatePacket;
*
* typedef struct {
  int id;
  float x;
  float y;
  float theta;
} ClientUpdate;
*
* typedef struct {
  PacketHeader header;
  int id;
  float rotational_force;
  float translational_force;
} VehicleUpdatePacket;**/

    int ret, socket = 0;
    char msg[DIM_BUFF];

    PacketHeader* head = (PacketHeader*)malloc(sizeof(PacketHeader));


    //bisogna estrapolare il veicolo identificato dall'id, aggiornarlo basandoci sulle nuove forze applicate, e infilarlo dentro il world_update_packet

    thread_server_UDP_args *arg = (thread_server_UDP_args*) args;

    socket = arg->socket_desc_UDP_server_M;
    clients* client=arg->list;



	//ad intervalli regolari integrare il mondo,inviare le nuove posizioni di tutti i client a tutti i client
    //DA FINIRE E CONTROLLARNE LA CORRETTEZZA
    while(1) {
        ret = sem_wait(&sem_thread_UDP);
        ERROR_HELPER(ret, "Failed to wait sem_thread_UDP in thread_UDP_sender");

        //DA FINIRE
        World_update(&world);
        int i;
        int num_connected=0;
        for(i=0;i<MAX_USER_NUM;i++){
			if(client[i].status==1){
				num_connected++;
			}
		}
		ClientUpdate* update=(ClientUpdate*)malloc(num_connected*sizeof(ClientUpdate));
        for(i=0;i<MAX_USER_NUM;i++){
			if(client[i].status==1){
				Vehicle* v = World_getVehicle(&world,client[i].id);
				update[i].id=client[i].id;
				update[i].x=v->x;
				update[i].y=v->y;
				update[i].theta=v->theta;
			}
		}

		WorldUpdatePacket* worldup=(WorldUpdatePacket*)malloc(sizeof(WorldUpdatePacket));
		head->type=WorldUpdate;
		head->size=sizeof(WorldUpdatePacket)+num_connected*sizeof(ClientUpdate);
		memcpy(&(worldup->header), head, sizeof(PacketHeader)); //worldup->header=head;
		worldup->num_vehicles=num_connected;
		worldup->updates=update;

		size_t packet_len=Packet_serialize(msg,&worldup->header);
        msg[packet_len] = '\0';

		// invio a tutti i connessi


		for(i=0;i<MAX_USER_NUM;i++){
			if(client[i].status==1){
				ret = send_UDP(socket,msg,packet_len+1,0,(const struct sockaddr*)client[i].addr,(socklen_t)sizeof(struct sockaddr_in));
				if (ret == -2) {
					printf("Could not send user data to client\n");
					ret = sem_post(&sem_thread_UDP);
					ERROR_HELPER(ret, "Failed to post sem_thread_UDP in thread_UDP_receiver");
				}
			}
		}

        //faccio la free delle strutture dati che non mi servono più (tanto verranno ricreate alla prossima ciclata)

        free(update);
        //Packet_free(head);

        //sblocco il semaforo


        ret = sem_post(&sem_thread_UDP);
        ERROR_HELPER(ret, "Failed to post sem_thread_UDP in thread_UDP_sender");

        sleep(500);
    }

}

void* thread_server_UDP_receiver(void* args){

    int ret, socket  = 0;
    char msg[DIM_BUFF];

    PacketHeader *head;
    VehicleUpdatePacket *packet;

    thread_server_UDP_args *arg = (thread_server_UDP_args*) args;

    socket = arg->socket_desc_UDP_server_W;
    struct sockaddr_in server_struct_UDP_W = arg->server_addr_UDP_W;
    int slen=sizeof(server_struct_UDP_W);


	//ricevere tutte le intenzioni di movimento e le sostituisce nei veicoli
    // DA CONTROLLARNE LA CORRETTEZZA


        while(1) {//dobbiamo gestire ancora la chiusura del server
            ret = recv_UDP(socket, msg, 1, 0, (struct sockaddr *)& server_struct_UDP_W,(socklen_t*)&slen);
            if (ret == -2) {
                printf("Could not send user data to client\n");
            }

            // deserializzo il pacchetto appena ricevuto
            head = Packet_deserialize(msg, sizeof(msg)-1);

            // pacchetto di aggiornamento del veicolo
            packet = (VehicleUpdatePacket *) head;

            ret = sem_wait(&sem_thread_UDP);
			PTHREAD_ERROR_HELPER(ret, "Failed to wait sem_thread_UDP in thread_UDP_receiver");
            //sostuisco le sue intenzioni di movimento ricevute nelle sue variabili nel mondo

            Vehicle* v=World_getVehicle(&world,packet->id);
            v->translational_force_update=packet->translational_force;
            v->rotational_force_update=packet->rotational_force;

			ret = sem_post(&sem_thread_UDP);
			ERROR_HELPER(ret, "Failed to post sem_thread_UDP in thread_UDP_receiver");
    }

}


int main(int argc, char **argv) {
    if (argc<3) {
        printf("usage: %s <elevation_image> <texture_image>\n", argv[1]);
        exit(-1);
    }

    if (DEBUG) printf("DEBUG MODE\n");

    char* elevation_filename=argv[1];
    char* texture_filename=argv[2];
    char* vehicle_texture_filename="./images/arrow-right.ppm";
    printf("loading elevation image from %s ... ", elevation_filename);

    // load the images
    Image* surface_elevation = Image_load(elevation_filename);  //elevation map da passare al thread TCP per consegnarla al singolo client
    if (surface_elevation) {
        printf("Done! \n");
    } else {
        printf("Fail! \n");
    }


    printf("loading texture image from %s ... ", texture_filename);
    Image* surface_texture = Image_load(texture_filename);      //map da passare al thread TCP per consegnarla al singolo client
    if (surface_texture) {
        printf("Done! \n");
    } else {
        printf("Fail! \n");
    }

    printf("loading vehicle texture (default) from %s ... ", vehicle_texture_filename);
    Image* vehicle_texture = Image_load(vehicle_texture_filename);
    if (vehicle_texture) {
        printf("Done! \n");
    } else {
        printf("Fail! \n");
    }

    // inizializzo l'array di client

    int i;
    for (i = 0; i < MAX_USER_NUM; i++){
        client[i].status=-1;
    }

    int ret;

	// inizializzo i semafori di mutex per tabella utenti e per num_online

	ret = sem_init(&sem_utenti, 0, 1);
	ERROR_HELPER(ret, "Failed to initialization of sem_utenti");

    ret = sem_init(&sem_online, 0, 1);
    ERROR_HELPER(ret, "Failed to initialization of sem_online");

	// inizializzo array di utenti
	for (i = 0; i < MAX_USER_NUM; i++) {
		utenti[i].username[0]='\0';
		utenti[i].password[0]='\0';
	}

	//creo il mondo

	World_init(&world, surface_elevation, surface_texture,  0.5, 0.5, 0.5);



    //definisco descrittore server socket e struttura per bind

    int server_socket_UDP_M;
    struct sockaddr_in server_addr_UDP_M = {0};

    //imposto i valori della struttura

    server_addr_UDP_M.sin_addr.s_addr=INADDR_ANY;
    server_addr_UDP_M.sin_family=AF_INET;
    server_addr_UDP_M.sin_port=htons(SERVER_PORT_UDP_M);

    //creo la socket

    server_socket_UDP_M=socket(AF_INET,SOCK_DGRAM,0);
    ERROR_HELPER(server_socket_UDP_M,"error creating socket UDP \n");

    // faccio la bind

    ret=bind(server_socket_UDP_M,(struct sockaddr*) &server_addr_UDP_M,sizeof(struct sockaddr_in));
    ERROR_HELPER(ret,"error binding socket  UDP \n");

    //definisco descrittore server socket e struttura per bind

    int server_socket_UDP_W;
    struct sockaddr_in server_addr_UDP_W = {0};

    //imposto i valori della struttura

    server_addr_UDP_W.sin_addr.s_addr=INADDR_ANY;
    server_addr_UDP_W.sin_family=AF_INET;
    server_addr_UDP_W.sin_port=htons(SERVER_PORT_UDP_W);

    //creo la socket

    server_socket_UDP_W=socket(AF_INET,SOCK_DGRAM,0);
    ERROR_HELPER(server_socket_UDP_W,"error creating socket UDP \n");

    // faccio la bind
    /*
    ret=bind(server_socket_UDP_W,(struct sockaddr*) &server_addr_UDP_W,sizeof(struct sockaddr_in));
    ERROR_HELPER(ret,"error binding socket  UDP \n");*/

    //creo i thread che si occuperanno di ricevere le forze dai vari client e di inviare un world update packet a tutti i client (UDP)

    ret = sem_init(&sem_thread_UDP, 0, 1);
	ERROR_HELPER(ret, "Failed to initialization of sem_thread_UDP");


    pthread_t thread_UDP_receiver;
    pthread_t thread_UDP_sender;

    thread_server_UDP_args* args_UDP=(thread_server_UDP_args*)malloc(sizeof(thread_server_UDP_args));
    args_UDP->socket_desc_UDP_server_W=server_socket_UDP_W;
    args_UDP->socket_desc_UDP_server_M=server_socket_UDP_M;
    args_UDP->server_addr_UDP_W = server_addr_UDP_W;
    args_UDP->server_addr_UDP_M = server_addr_UDP_M;
    args_UDP->list=client;

    //creo i thread

    ret = pthread_create(&thread_UDP_receiver, NULL, thread_server_UDP_receiver, (void*) args_UDP);
    ERROR_HELPER(ret, "Could not create thread");

    ret = pthread_create(&thread_UDP_sender, NULL, thread_server_UDP_sender, (void*) args_UDP);
    ERROR_HELPER(ret, "Could not create thread");

    // impongo di non aspettare terminazione in modo da non bloccare tutto il programma

    ret = pthread_detach(thread_UDP_receiver);
    ERROR_HELPER(ret, "Could not detach thread");

    ret = pthread_detach(thread_UDP_sender);
    ERROR_HELPER(ret, "Could not detach thread");


    //definisco descrittore server socket TCP

    int server_socket_TCP;
    struct sockaddr_in server_addr = {0};

    //imposto i valori delle struttura per accettare connessioni

    server_addr.sin_addr.s_addr=INADDR_ANY;
    server_addr.sin_family=AF_INET;
    server_addr.sin_port=htons(SERVER_PORT_TCP);

    //creo la socket

    server_socket_TCP=socket(AF_INET,SOCK_STREAM,0);
    ERROR_HELPER(server_socket_TCP,"error creating socket \n");

    // faccio la bind

    ret=bind(server_socket_TCP,(struct sockaddr*) &server_addr,sizeof(struct sockaddr_in));
    ERROR_HELPER(ret,"error binding socket \n");

    // rendo la socket in grado di accettare connessioni

    ret=listen(server_socket_TCP,3);
    ERROR_HELPER(ret,"error setting socket to listen connections \n");

    while(1){

        // definisco descrittore della socket con cui parlerò poi con ogni client

        struct sockaddr_in* client_addr= (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
        memset(client_addr,0,sizeof(struct sockaddr_in));
        int client_socket;
        int slen=sizeof(struct sockaddr_in);

        // mi metto in attesa di connessioni

        client_socket=accept(server_socket_TCP,(struct sockaddr*) &client_addr,(socklen_t*)&slen);
        ERROR_HELPER(client_socket,"error accepting connections \n");

        //lancio il thread che si occuperà di parlare poi con il singolo client che si è connesso

        pthread_t thread;

        thread_server_TCP_args* args_TCP=(thread_server_TCP_args*)malloc(sizeof(thread_server_TCP_args));
        args_TCP->addr=client_addr;
        args_TCP->socket_desc_TCP_client = client_socket;
        args_TCP->list=client;
        args_TCP->elevation_map = surface_elevation;
        args_TCP->map = surface_texture;

        ret = pthread_create(&thread, NULL,thread_server_TCP,args_TCP);
        ERROR_HELPER(ret, "Could not create thread");

        ret = pthread_detach(thread);
        ERROR_HELPER(ret, "Could not detach thread");


    }


  // not needed here
  //   // construct the world
  // World_init(&world, surface_elevation, surface_texture,  0.5, 0.5, 0.5);

  // // create a vehicle
  // vehicle=(Vehicle*) malloc(sizeof(Vehicle));
  // Vehicle_init(vehicle, &world, 0, vehicle_texture);

  // // add it to the world
  // World_addVehicle(&world, vehicle);



  // // initialize GL
  // glutInit(&argc, argv);
  // glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
  // glutCreateWindow("main");

  // // set the callbacks
  // glutDisplayFunc(display);
  // glutIdleFunc(idle);
  // glutSpecialFunc(specialInput);
  // glutKeyboardFunc(keyPressed);
  // glutReshapeFunc(reshape);

  // WorldViewer_init(&viewer, &world, vehicle);


  // // run the main GL loop
  // glutMainLoop();

  // // check out the images not needed anymore
  // Image_free(vehicle_texture);
  // Image_free(surface_texture);
  // Image_free(surface_elevation);

  // // cleanup
  // World_destroy(&world);
  return 0;
}
