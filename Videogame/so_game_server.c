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
#include <signal.h>

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
clients client[MAX_USER_NUM];      //vettore di puntatori a strutture dati clients
ListHead mov_int_list;             //lista delle intenzioni dei movimenti
int num_online = 0;                //numero degli utenti online
World world;                       //mondo    
int server_socket_UDP;			   //descrittore server socket UDP
int server_socket_TCP;			   //descrittore server socket TCP
int communication = 1;             //variabile per la terminazione dei thread UDP
int main_var = 1;                  //variabile per la terminazione del main

sem_t sem_utenti;
sem_t sem_world;

/**
NOTA PER IL SERVER: Quando c'è un errore su una socket di tipo ENOTCONN (sia UDP che TCP), allora vuol dire che il client si è disconnesso (indipendetemente
se la disconnessione è avvenuta in maniera pulita oppure no). Ciò si verifica anche quando la recv e la receive_from restituiscono 0. In tal caso, il server
dovrà salvare lo stato del client che si è disconnesso (semplicemente, si sposta il suo contenuto dalla sua cella di online_client alla sua cella di
offline_client, e poi si mette a NULL la sua cella in online_client.
**/

void handle_signal(int sig){
    int ret;
    int i;
    printf("Signal caught: %d\n", sig);

    switch(sig){
        case SIGHUP:
            break;
        case SIGTERM:
        case SIGQUIT:
        case SIGINT:
            if (verbosity_level>=General) printf("Closing...\n");

            communication = 0;
            main_var = 0;
            sleep(1);           // attendo che gli altri thread escano dal while

            
            for(i=0;i<MAX_USER_NUM;i++){
                if(client[i].status==1){
                    ret=shutdown(client[i].socket_TCP,SHUT_RDWR);
                    ERROR_HELPER(ret,"Error shutdown socket socket \n");
                    ret=close(client[i].socket_TCP);
                    ERROR_HELPER(ret,"Error closing socket \n");
                    if(verbosity_level>=General)printf("TCP client socket closed %d \n",i);

                }
            }

            ret = close(server_socket_TCP);
            ERROR_HELPER(ret, "Error in closing socket desc TCP");

            ret = close(server_socket_UDP);
            ERROR_HELPER(ret, "Error in closing socket desc UDP");

            ret = sem_destroy(&sem_world);
            ERROR_HELPER(ret, "Error in destroy sem_world");

            ret = sem_destroy(&sem_utenti);
            ERROR_HELPER(ret, "Error in destroy sem_utenti");

            if (verbosity_level>=General) printf("Socket closed and semaphores destroyed\n");

            World_destroy(&world);

            if (verbosity_level>=General) printf("World destroyed\n");

            break;

        case SIGSEGV:
            if (verbosity_level>=General) printf("Segmentation fault... closing\n");

            communication = 0;
            main_var = 0;
            sleep(1);           // attendo che gli altri thread escano dal while

            for(i=0;i<MAX_USER_NUM;i++){
                if(client[i].status==1){
                    ret=shutdown(client[i].socket_TCP,SHUT_RDWR);
                    ERROR_HELPER(ret,"Error shutdown socket socket \n");
                    ret=close(client[i].socket_TCP);
                    ERROR_HELPER(ret,"Error closing socket \n");
                    if(verbosity_level>=General)printf("TCP client socket closed %d \n",i);

                }
            }

            ret = close(server_socket_TCP);
            ERROR_HELPER(ret, "Error in closing socket desc TCP");

            ret = close(server_socket_UDP);
            ERROR_HELPER(ret, "Error in closing socket desc UDP");

            ret = sem_destroy(&sem_world);
            ERROR_HELPER(ret, "Error in destroy sem_world");

            ret = sem_destroy(&sem_utenti);
            ERROR_HELPER(ret, "Error in destroy sem_utenti");

            if (verbosity_level>=General) printf("Socket closed and semaphores destroyed\n");

            World_destroy(&world);

            if (verbosity_level>=General) printf("World destroyed\n");

            break;

        case SIGPIPE:
            if (verbosity_level>=General) printf("Socket closed\n");
            break;

        default:
            if (verbosity_level>=General) printf("Caught wrong signal...\n");
            return;
    }

}



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

    if (verbosity_level>=DebugTCP) {
        int value;
        ret = sem_getvalue(&sem_utenti, &value);
        printf("[LOGIN] Semaphore's value before wait = %d\n", value);
    }

    ret = sem_wait(&sem_utenti);
    ERROR_HELPER(ret, "Error in sem_utenti wait");

    if (verbosity_level>=DebugTCP) printf("[LOGIN] Checking if already registered...\n");

	// Verifico se user già registrato
	int idx = -1;
	for (i = 0; strlen(utenti[i].username)>0; i++) {
		if (strcmp(user_att,utenti[i].username) == 0) {
			idx = i;
		}
	}

    if (verbosity_level>=DebugTCP) printf("[LOGIN] Checking done\n");

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

        if (verbosity_level>=DebugTCP) {
            int value;
            ret = sem_getvalue(&sem_utenti, &value);
            printf("[LOGIN] Semaphore's value after post following user registration = %d\n", value);
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

        if (verbosity_level>=DebugTCP) {
            int value;
            ret = sem_getvalue(&sem_utenti, &value);
            printf("[LOGIN] Semaphore's value before wait for password registration = %d\n", value);
        }

		// registrazione password ed id
		ret = sem_wait(&sem_utenti);
        PTHREAD_ERROR_HELPER(ret, "Error in sem_utenti wait: failed to register username & password");

		strcpy(utenti[idx].password,pass_att);

        if (verbosity_level>=DebugTCP) printf("[LOGIN] Password saved\n");

		ret = sem_post(&sem_utenti);
        ERROR_HELPER(ret, "Error in sem_utenti post: failed to register username & password");

        if (verbosity_level>=DebugTCP) {
            int value;
            ret = sem_getvalue(&sem_utenti, &value);
            printf("[LOGIN] Semaphore's value at the end of login protocol = %d\n", value);
        }


	}

	// Già registrato
	else if (idx > -1) {
        strcpy(pass_giusta,utenti[idx].password);

		ret = sem_post(&sem_utenti);
        ERROR_HELPER(ret, "Error in sem_utenti post");

        // client attualmente connesso
        if (client[idx].status == 1) {
            login_reply = 999;  // identificativo che sono già connesso
            login_status = login_reply;
        }

		// informo il client che è già registrato
        else {
            login_reply = 1;
            login_status = login_reply;
        }

        sprintf(risposta_login, "%d", login_reply);

        ret = send_TCP(socket, risposta_login, strlen(risposta_login)+1, 0);
        if (ret == -2) {
            printf("Could not send login_reply to client\n");
            pthread_exit(NULL);
        }
        else PTHREAD_ERROR_HELPER(ret, "Failed to send login_reply to client");

        // termino il thread in quanto già connesso
        if (login_status == 999) {
            ret = close(socket);
            if (errno != EBADF) ERROR_HELPER(ret, "could not close socket_TCP");
            pthread_exit(NULL);
        }

		//ricezione password
		do {

            ret = recv_TCP(socket, pass_att, 1, 0);
            if (ret == -2) {
                printf("Could not receive password from client\n");
                pthread_exit(NULL);
            }
            else PTHREAD_ERROR_HELPER(ret, "Failed to read password from client");

			// password giusta
			if (strcmp(pass_att,pass_giusta) == 0) {
				login_reply = 1;

                sprintf(risposta_login, "%d", login_reply);

                ret = send_TCP(socket, risposta_login, strlen(risposta_login)+1, 0);
                if (ret == -2) {
                    printf("Could not send login_reply to client\n");
                    pthread_exit(NULL);
                }
			}

            // password sbagliata
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

    int bytes_read = 0;

    //nuovo utente: richiesta di id

    if (login_status == 0){

        if (verbosity_level>=DebugTCP) printf("[IDPACKET] I'm in <if (login_status == 0)>\n");

        char *idPacket = (char *)malloc(DIM_BUFF*sizeof(char));

        ret = recv_TCP_packet(socket, idPacket, 0, &bytes_read);
        if (ret == -2){
            printf("Could not receive id request from client\n");
            pthread_exit(NULL);
        }
        PTHREAD_ERROR_HELPER(ret, "Could not receive id request from client");

        if (verbosity_level>=DebugTCP) printf("[IDPACKET] Received IdPacket request from client\n");

        if (verbosity_level>=DebugTCP) printf("[IDPACKET] Bytes read: %d\n", bytes_read);

        if (bytes_read != (int) sizeof(IdPacket)){
            if (verbosity_level>=DebugTCP) printf("[IDPACKET] There's an error! Size of bytes arrived not matching!\n");
        }

        size_t msg_len = bytes_read;

        IdPacket* id = (IdPacket*) Packet_deserialize(idPacket, msg_len);
        if (id->header.type != GetId) PTHREAD_ERROR_HELPER(-1, "Error in packet type (client id)");

        if (verbosity_level>=DebugTCP) printf("[IDPACKET] IdPacket deserialized\n");

        if (verbosity_level>=DebugTCP) printf("[IDPACKET] Accessing Id request data...\n");

        if (id->id == -1){
            id->id = idx;
            size_t idPacket_response_len;
            idPacket_response_len = Packet_serialize(idPacket, &(id->header));

            ret = send_TCP(socket, idPacket, idPacket_response_len, 0);
            if (ret == -2){
                printf("Could not send id response to client\n");
                pthread_exit(NULL);
            }
            PTHREAD_ERROR_HELPER(ret, "Could not send id response to client\n");

            if (verbosity_level>=DebugTCP) printf("[IDPACKET] IdPacket sent to client\n");

            if (verbosity_level>=DebugTCP) printf("[IDPACKET] Bytes sent: %d\n", (int) ret);

            if (ret == idPacket_response_len){
                if (verbosity_level>=DebugTCP) printf("[IDPACKET] Perfect\n");
            }
            else{
                if (verbosity_level>=DebugTCP){
                    printf("[IDPACKET] No good\n");
                    printf("[IDPACKET] ret = %d\n", ret);
                    printf("[IDPACKET] idPacket_request_len = %d\n", (int) idPacket_response_len);
                }
            }
        }

        else{
            PTHREAD_ERROR_HELPER(-1, "Error in id packet request!");
            pthread_exit(NULL);
        }

        free(idPacket);
        Packet_free((PacketHeader *) id);

        //nuovo utente: invia la sua texture

        if (verbosity_level>=DebugTCP) printf("[TEXTURE] Waiting for texture from client\n");

        char *texture_utente = (char *)malloc(DIM_BUFF*sizeof(char));

        bytes_read = 0;

        ret = recv_TCP_packet(socket, texture_utente, 0, &bytes_read);
        if (ret == -2){
            printf("Could not receive client texture\n");
            pthread_exit(NULL);
        }
        PTHREAD_ERROR_HELPER(ret, "Could not receive client texture");

        if (verbosity_level>=DebugTCP) printf("[TEXTURE] Bytes read: %d\n", bytes_read);

        if (verbosity_level>=DebugTCP) printf("[TEXTURE] Texture received from client. Deserializing....\n");

        msg_len = bytes_read;

        ImagePacket* client_texture = (ImagePacket*) Packet_deserialize(texture_utente, msg_len);
        if (client_texture->header.type!=PostTexture) PTHREAD_ERROR_HELPER(-1, "Error in client texture packet!");

        if (verbosity_level>=DebugTCP) printf("[TEXTURE] Texture deserialized\n");

        if (verbosity_level>=DebugTCP) printf("Updating client status inside his array entry\n");

        client[idx].id = idx;
        client[idx].texture = client_texture->image;
        client[idx].status = 1;
        client[idx].socket_TCP=arg->socket_desc_TCP_client;

        if (verbosity_level>=DebugTCP) printf("[TEXTURE] Serializing client's texture\n");

        size_t texture_utente_len;
        texture_utente_len = Packet_serialize(texture_utente, &(client_texture->header));

        if (verbosity_level>=DebugTCP) printf("[TEXTURE] Texture serialized\n");

        if (verbosity_level>=DebugTCP) printf("[TEXTURE] Sending to client his texture...\n");

        ret = send_TCP(socket, texture_utente, texture_utente_len, 0);
        if (ret == -2){
            printf("Could not send client texture\n");
            client[idx].status = 0;
            ret = close(socket);
            ERROR_HELPER(ret, "Could not close socket");
            pthread_exit(NULL);
        }
        else PTHREAD_ERROR_HELPER(ret, "Could not send client texture\n");

        if (verbosity_level>=DebugTCP) printf("[TEXTURE] Bytes sent: %d\n", (int) ret);

        if (ret == texture_utente_len){
            if (verbosity_level>=DebugTCP) printf("[TEXTURE] Perfect\n");
        }
        else{
            if (verbosity_level>=DebugTCP){
                printf("[TEXTURE] No good\n");
                printf("[TEXTURE] ret = %d\n", ret);
                printf("[TEXTURE] texture_utente_len = %d\n", (int) texture_utente_len);
            }
        }

        if (verbosity_level>=DebugTCP) printf("[TEXTURE] Texture sent to client successfully\n");

        free(texture_utente);

        if (verbosity_level>=DebugTCP) printf("[TEXTURE] Creating client vehicle!\n");

         // create a vehicle

        ret = sem_wait(&sem_world);
        PTHREAD_ERROR_HELPER(ret, "Error in sem_world wait \n");

		Vehicle* vehicle=(Vehicle*) malloc(sizeof(Vehicle));
		Vehicle_init(vehicle, &world, idx, client[idx].texture);

        if (verbosity_level>=DebugTCP) printf("[VEHICLE] Client vehicle created. Adding it to the world...\n");

		//  add it to the world
		World_addVehicle(&world, vehicle);

        ret = sem_post(&sem_world);
        PTHREAD_ERROR_HELPER(ret, "Error in sem_world post \n");

        if (verbosity_level>=DebugTCP) printf("[WORLD] Vehicle added successfully\n");

    }

    //utente gia registrato

    else if (login_status == 1){

        /**client invia un IdPacket, con id settato a -1, noi rispondiamo con un ImagePacket, contenente la sua texture (salvata precedentemente) e il suo id
         * in pratica gli inviamo tutto insieme
        **/

        char *idPacket_buf = (char *)malloc(DIM_BUFF*sizeof(char));
        size_t idPacket_buf_len;

        if (verbosity_level>=DebugTCP) printf("[IDPACKET] Waiting for IdPacket from client registered before\n");

        ret = recv_TCP_packet(socket, idPacket_buf, 0, &bytes_read);
        if (ret == -2) {
            printf("Could not receive IdPacket from client\n");
            pthread_exit(NULL);
        }
        else PTHREAD_ERROR_HELPER(ret, "Could not receive IdPacket from client");

        size_t msg_len = bytes_read;

        if (verbosity_level>=DebugTCP) printf("[SERVER] IdPacket received. Deserializing\n");

        IdPacket* received = (IdPacket*) Packet_deserialize(idPacket_buf, msg_len);
        if (received->header.type != GetTexture) PTHREAD_ERROR_HELPER(-1, "Connection error (texture request)");
        Packet_free((PacketHeader *) received);

        if (verbosity_level>=DebugTCP) printf("[SERVER] Packet deserialized. Updating client status inside his array entry\n");

        client[idx].status = 1;
        client[idx].socket_TCP=arg->socket_desc_TCP_client;

        if (verbosity_level>=DebugTCP) printf("[SERVER] Inizialing texture packet for client\n");

        ImagePacket* client_texture = (ImagePacket*) malloc(sizeof(ImagePacket));
        client_texture->id = idx;
        client_texture->image = client[idx].texture;
        client_texture->header.type = PostTexture;

        if (verbosity_level>=DebugTCP) printf("[SERVER] Packet cread and inizialized. Serializing...\n");

        idPacket_buf_len = Packet_serialize(idPacket_buf, &(client_texture->header));

        if (verbosity_level>=DebugTCP) printf("[SERVER] Packet serialized, sending...\n");

        ret = send_TCP(socket, idPacket_buf, idPacket_buf_len, 0);
        if (ret == -2) {
            printf("Could not send client texture and id\n");
            client[idx].status = 0;
            ret = close(socket);
            ERROR_HELPER(ret, "Could not close socket");
            pthread_exit(NULL);
        }
        else PTHREAD_ERROR_HELPER(ret, "Could not send client texture and id");

        if (verbosity_level>=DebugTCP) printf("[SERVER] Packet sent. Creating a vehicle for reconnected client\n");

        ret = sem_wait(&sem_world);
        PTHREAD_ERROR_HELPER(ret, "Error in sem_world wait \n");

         // create a vehicle
		Vehicle* vehicle=(Vehicle*) malloc(sizeof(Vehicle));
		Vehicle_init(vehicle, &world, idx, client[idx].texture);
        // inserisco le ultime posizioni nel veicolo
        vehicle->x = client[idx].x; vehicle->prev_x = client[idx].x;
        vehicle->y = client[idx].y; vehicle->prev_y = client[idx].y;
        vehicle->theta = client[idx].theta; vehicle->prev_theta = client[idx].theta;

        if (verbosity_level>=DebugTCP) printf("[SERVER] Vehicle created, adding to the world\n");

		//  add it to the world
        Vehicle *v_prev = World_getVehicle(&world, client[idx].id);
        if (v_prev) {
            World_detachVehicle(&world, v_prev);
            Vehicle_destroy(v_prev);
        }
		World_addVehicle(&world, vehicle);

        if (verbosity_level>=DebugTCP) printf("[SERVER] Vehicle added successfully\n");

        ret = sem_post(&sem_world);
        PTHREAD_ERROR_HELPER(ret, "Error in sem_world post \n");

        if (verbosity_level>=DebugTCP) printf("[SERVER] Sending coordinates to client\n");

        ClientUpdate *coord= (ClientUpdate *)malloc(sizeof(ClientUpdate));
        // completo il pacchetto
        coord->id = idx; 
        coord->x = vehicle->x; 
        coord->y = vehicle->y; 
        coord->theta = vehicle->theta;

        char *coord_buf = (char *)malloc(DIM_BUFF*sizeof(char));
        memcpy(coord_buf, coord, sizeof(ClientUpdate));     // non posso usare serialize perché non tratta questo caso

        ret = send_TCP(socket, coord_buf, sizeof(ClientUpdate), 0);
        if (ret != sizeof(ClientUpdate)) ERROR_HELPER(-1, "Could not send position");

        free(idPacket_buf);
        free(coord_buf);
        free(coord);

    }

    if (verbosity_level>=DebugTCP) printf("[ELEVATION_MAP] Waiting elevation map request from client\n");

    //utente invia la richiesta di elevation map

    char *elevation_map_buffer = (char *)malloc(DIM_BUFF*sizeof(char));
    size_t elevation_map_len;

    bytes_read = 0;

    ret = recv_TCP_packet(socket, elevation_map_buffer, 0, &bytes_read);
    if (ret == -2) {
        printf("Could not receive elevation map request\n");
        client[idx].status = 0;
        ret = close(socket);
        if (errno != EBADF) ERROR_HELPER(ret, "Could not close socket");
        pthread_exit(NULL);
    }
    else PTHREAD_ERROR_HELPER(ret, "Could not receive elevation map request");

    if (verbosity_level>=DebugTCP) printf("[ELEVATION_MAP] Bytes read: %d\n", bytes_read);

    if (verbosity_level>=DebugTCP) printf("[ELEVATION_MAP] Request received. Deserializing packet...\n");

    size_t msg_len = bytes_read;

    IdPacket* elevation= (IdPacket*) Packet_deserialize(elevation_map_buffer,msg_len);
    if(elevation->header.type!=GetElevation) ERROR_HELPER(-1,"error in communication \n");

    if (verbosity_level>=DebugTCP) printf("[ELEVATION_MAP] Deserialization complete\n");

    int id = elevation->id;
    Packet_free((PacketHeader *) elevation);

    if (verbosity_level>=DebugTCP) printf("[ELEVATION_MAP] Creating elevation map packet\n");

    ImagePacket* ele_map = (ImagePacket*) malloc(sizeof(ImagePacket));
    ele_map->image = elevation_map;
    ele_map->id = id;
    ele_map->header.type = PostElevation;

    if (verbosity_level>=DebugTCP) printf("[ELEVATION_MAP] Packet created, serializing...\n");

    memset(elevation_map_buffer, 0, DIM_BUFF);
    elevation_map_len = Packet_serialize(elevation_map_buffer, &(ele_map->header));
    ele_map->image = NULL;      // per evitare la deallocazione di quest'ultima quando invoco Packet_free
    Packet_free((PacketHeader *) ele_map);

    PacketHeader *h_test = (PacketHeader *) elevation_map_buffer; 
    if (verbosity_level>=DebugTCP) printf("[ELEVATION_MAP] Header size field: %d, serialized %ld byte\n", h_test->size, elevation_map_len);
    if (verbosity_level>=DebugTCP) printf("[ELEVATION_MAP] Serialization complete, sending packet\n");

    // invio dell'elevation map
    

    ret = send_TCP(socket, elevation_map_buffer, elevation_map_len, 0);
    if (ret == -2) {
        printf("Could not send elevation map\n");
        client[idx].status = 0;
        ret = close(socket);
        if (errno != EBADF) ERROR_HELPER(ret, "Could not close socket");
        pthread_exit(NULL);
    }
    else PTHREAD_ERROR_HELPER(ret, "Could not send elevation map to client");

    if (verbosity_level>=DebugTCP) printf("[ELEVATION_MAP] Bytes sent: %d\n", ret);

    if (verbosity_level>=DebugTCP) printf("[ELEVATION_MAP] Sending complete\n");

    free(elevation_map_buffer);

    //ricezione richiesta mappa e invio mappa

    if (verbosity_level>=DebugTCP) printf("[MAP] Waiting map request from client\n");

    char *map_buffer = (char *)malloc(DIM_BUFF*sizeof(char));

    bytes_read = 0;

    ret = recv_TCP_packet(socket, map_buffer,0,&bytes_read);
    if (ret == -2) {
        printf("Could not receive map request\n");
        client[idx].status = 0;
        ret = close(socket);
        if (errno != EBADF) ERROR_HELPER(ret, "Could not close socket");
        pthread_exit(NULL);
    }
    else PTHREAD_ERROR_HELPER(ret, "Could not receive map request");
    

    if (verbosity_level>=DebugTCP) printf("[MAP] Bytes read: %d\n", bytes_read);

    if (verbosity_level>=DebugTCP) printf("[MAP] Request received. Deserializing...\n");

    msg_len = bytes_read;

    IdPacket* map_request = (IdPacket*) Packet_deserialize(map_buffer, msg_len);
    if(map_request->header.type!=GetTexture)   ERROR_HELPER(-1, "Connection error (map request)");

    free(map_buffer);

    if (verbosity_level>=DebugTCP) printf("[MAP] Deserialization complete\n");

    id = map_request->id;
    Packet_free((PacketHeader *) map_request);

    if (verbosity_level>=DebugTCP) printf("[MAP] Creating map packet\n");

    //invio mappa

    char *mappa = (char *)malloc(DIM_BUFF*sizeof(char));
    size_t mappa_len;

    ImagePacket* map_packet = (ImagePacket*) malloc(sizeof(ImagePacket));
    map_packet->id = id;
    map_packet->image = map;
    map_packet->header.type = PostTexture;

    if (verbosity_level>=DebugTCP) printf("[MAP] Map packet created. Serializing...\n");

    mappa_len = Packet_serialize(mappa, &(map_packet->header));
    map_packet->image = NULL;
    Packet_free((PacketHeader *) map_packet);

    if (verbosity_level>=DebugTCP) printf("[MAP] Serialization complete.\n");
    
    if (verbosity_level>=DebugTCP) printf("[MAP] Sending: %d bytes\n", (int) mappa_len);

    if (verbosity_level>=DebugTCP) printf("[MAP] Sendin packet!\n");

    ret = send_TCP(socket, mappa, mappa_len, 0);
    if (ret == -2){
        printf("Could not send map to client\n");
        client[idx].status = 0;
        ret = close(socket);
        if (errno != EBADF) ERROR_HELPER(ret, "Could not close socket");
        pthread_exit(NULL);
    }
    else PTHREAD_ERROR_HELPER(ret, "Could not send map to client\n");

    if (verbosity_level>=DebugTCP) printf("[MAP] Bytes sent: %d\n [MAP] Sending complete!\n",(int) msg_len);

    if (verbosity_level>=DebugTCP) printf("[SERVER] Client with %d connected (status = %d)\n", client[idx].id, client[idx].status);

    free(mappa);


    /** Ultimata la connessione e l'inizializzazione del client, c'è bisogno di inviargli lo stato di tutti gli altri client già connessi **/

    ImagePacket* client_alive = (ImagePacket*) malloc(sizeof(ImagePacket));
    client_alive->header.type=PostTexture;

    char *client_alive_buf = (char *)malloc(DIM_BUFF*sizeof(char));
    size_t alive_len;

    //inviamo  le texture di tutti i client connessi

    if (verbosity_level>=DebugTCP) printf("[ALIVE] Sending textures of clients already in world\n");

    Vehicle *v_p;

    for (i = 0; i < MAX_USER_NUM; i++){
        if (i != idx && client[i].status==1){
            client_alive->id = client[i].id;
            client_alive->image = client[i].texture;

            alive_len = Packet_serialize(client_alive_buf, &(client_alive->header));
            if (verbosity_level>=DebugTCP) printf("[ALIVE] Packet to send serialized\n");

            ret = send_TCP(socket, client_alive_buf, alive_len, 0);
            if (ret == -2){
                printf("Could not send user data to client\n");

                ret = sem_wait(&sem_world);
                ERROR_HELPER(ret, "Could not wait sem_world");
                client[idx].status = 0;
                // Salvo l'ultima posizione del veicolo e lo elimino dal mondo
                v_p = World_getVehicle(&world, client[idx].id);
                client[idx].x = v_p->x; 
                client[idx].y = v_p->y; 
                client[idx].theta = v_p->theta;
                World_detachVehicle(&world, v_p);
                ret = sem_post(&sem_world);
                ERROR_HELPER(ret, "Could not post sem_world");

                ret = close(socket);
                if (errno != EBADF) ERROR_HELPER(ret, "Could not close socket");
                free(client_alive_buf);
                client_alive->image = NULL;
                Packet_free((PacketHeader *) client_alive);
                pthread_exit(NULL);
            }
            else PTHREAD_ERROR_HELPER(ret, "Could not send user data to client\n");

            if (verbosity_level>=DebugTCP) printf("[ALIVE] Packet sent\n"); 
        }
    }

    free(client_alive_buf);
    client_alive->image = NULL;
    Packet_free((PacketHeader *) client_alive);

    //inviamo a tutti i client attualmente connessi la texture del nuovo client appena arrivato
    ImagePacket* texture=(ImagePacket*)malloc(sizeof(ImagePacket));
	texture->header.type=PostTexture;
	texture->id=idx;
	texture->image=client[idx].texture;
	char *texture_buffer = (char *)malloc(DIM_BUFF*sizeof(char));
	size_t texture_len=Packet_serialize(texture_buffer,&(texture->header));
    if (verbosity_level>=DebugTCP) printf("[ALIVE TEXTURE] Packet serialized\n");

    for(i=0;i<MAX_USER_NUM;i++){
		if(i!=idx && client[i].status == 1){
			ret = send_TCP(client[i].socket_TCP, texture_buffer, texture_len, 0);
            if (ret == -2){
                printf("Could not send user data to client\n");
                

                ret = sem_wait(&sem_world);
                ERROR_HELPER(ret, "Could not wait sem_world");
                client[i].status = 0;
                // Salvo l'ultima posizione del veicolo e lo elimino dal mondo
                v_p = World_getVehicle(&world, client[i].id);
                client[i].x = v_p->x; 
                client[i].y = v_p->y; 
                client[i].theta = v_p->theta;
                World_detachVehicle(&world, v_p);
                ret = sem_post(&sem_world);
                ERROR_HELPER(ret, "Could not post sem_world");

                ret = close(client[i].socket_TCP);
                if (errno != EBADF) ERROR_HELPER(ret, "Could not close socket");
            }
            else PTHREAD_ERROR_HELPER(ret, "Could not send user data to client\n");

            if (verbosity_level>=DebugTCP) printf("[ALIVE TEXTURE] Packet sent\n");
        }
    }

    free(texture_buffer);
    texture->image = NULL;      // altrimenti mi dealloca anche la texture del client
    Packet_free((PacketHeader *) texture);

	char *test_buf = (char *)malloc(DIM_BUFF*sizeof(char));
	size_t test_len;

	IdPacket* test=(IdPacket*)malloc(sizeof(IdPacket));
	test->header.type=GetId;
	test->id=90;       // valore speciale (ovviamente più grande di MAX_USER_NUM)

	test_len=Packet_serialize(test_buf,&(test->header));
    if (verbosity_level>=DebugTCP) printf("[TEST PACKET] Serialized test packet\n");
	while(1){
        test->id=90;
		ret = send_TCP(socket, test_buf, test_len, 0);
		if (ret == -2){
            

            ret = sem_wait(&sem_world);
            ERROR_HELPER(ret, "Could not wait sem_world");
            client[idx].status = 0;
            // Salvo l'ultima posizione del veicolo e lo elimino dal mondo
            v_p = World_getVehicle(&world, client[idx].id);
            if (verbosity_level>=DebugTCP) printf("[TEST PACKET] Vehicle with id %d disconnected, his last coordinates are "
                                                  "(x,y,theta) = (%f,%f,%f)\n", v_p->id, v_p->x, v_p->y, v_p->theta);
            client[idx].x = v_p->x; 
            client[idx].y = v_p->y; 
            client[idx].theta = v_p->theta;
            World_detachVehicle(&world, v_p);
            ret = sem_post(&sem_world);
            ERROR_HELPER(ret, "Could not post sem_world");

			for(i=0;i<MAX_USER_NUM;i++){
				if(client[i].status==1 && i!=idx){
					test->id=idx;
					test_len=Packet_serialize(test_buf,&(test->header));
					ret = send_TCP(client[i].socket_TCP, test_buf, test_len, 0);   // non mi interessa gestire errori e disconnessioni
				}
			}
            break;
		}
		else PTHREAD_ERROR_HELPER(ret, "Could not send user data to client\n");

		if (verbosity_level>=DebugTCP) printf("[TEST PACKET] Test packet sent\n");

		sleep(2);
	}

    if (verbosity_level>=DebugTCP) printf("[TEST PACKET] Test packets sent\n");

    free(test_buf);
    Packet_free((PacketHeader *) test);
    ret=close(socket);
    PTHREAD_ERROR_HELPER(ret,"Error closing socket\n");
    if (arg) free(arg);
    pthread_exit(NULL);

    /** FINE LAVORO TCP **/

}

void* thread_server_UDP_sender(void* args){


    int ret, socket = 0;
    char *msg = (char *)malloc(DIM_BUFF*sizeof(char));

    //bisogna estrapolare il veicolo identificato dall'id, aggiornarlo basandoci sulle nuove forze applicate, e infilarlo dentro il world_update_packet

    thread_server_UDP_args *arg = (thread_server_UDP_args*) args;

    socket = arg->socket_desc_UDP_server;
    clients* client=arg->list;
    int slen;



	//ad intervalli regolari integrare il mondo,inviare le nuove posizioni di tutti i client a tutti i client
    while(communication) {

        struct sockaddr_in client_addr;

    	if (verbosity_level>=DebugUDP) printf("\n[UDP SENDER] I'm alive!\n");
    	
        ret = sem_wait(&sem_world);
        ERROR_HELPER(ret, "Failed to wait sem_world in thread_UDP_sender");

        if (verbosity_level>=DebugUDP) printf("[UDP SENDER] Wait crossed\n");

        int i,j;
        int num_connected=0;
        for(i=0;i<MAX_USER_NUM;i++){
			if(client[i].status==1){
				num_connected++;
			}
		}

        if (verbosity_level>=DebugUDP) printf("[UDP SENDER] There are %d clients connected at the moment\n", num_connected);

		ClientUpdate* update=(ClientUpdate*)malloc(num_connected*sizeof(ClientUpdate));

        if (verbosity_level>=DebugUDP) printf("[UDP SENDER] Created ClientUpdate packet with size: %lu\n", num_connected*sizeof(ClientUpdate));
        for(i=0,j=0; i<MAX_USER_NUM && j<num_connected; i++){
			if(client[i].status==1){
				Vehicle* v = World_getVehicle(&world,client[i].id);
                if(v!=0){
                    if (verbosity_level>=DebugUDP) printf("[UDP SENDER] Vehicle with id %d extrapolated from the world, its coordinates are "
                                                          "(x,y,theta) = (%f,%f,%f)\n", v->id, v->x, v->y, v->theta);
				    update[j].id=client[i].id;
				    update[j].x=v->x;
				    update[j].y=v->y;
				    update[j].theta=v->theta;
                    if (verbosity_level>=DebugUDP) printf("[UDP SENDER] Inizialized array update's %d entry \n", i);
                }

                j++;
			}
		}

        if (verbosity_level>=DebugUDP) printf("[UDP SENDER] World clients positions updated. Creating WorldUpdatePacket packet\n");

		WorldUpdatePacket* worldup=(WorldUpdatePacket*)malloc(sizeof(WorldUpdatePacket));
		worldup->header.type=WorldUpdate;
		worldup->num_vehicles=num_connected;
		worldup->updates=update;

        if (verbosity_level>=DebugUDP) printf("[UDP SENDER] WorldUpdatePacket created and inizialized. Serializing...\n");

		size_t packet_len=Packet_serialize(msg,&worldup->header);
        
        if (verbosity_level>=DebugUDP) printf("[UDP SENDER] New world has been serialized, sending it to all of the connected\n");

		// invio a tutti i connessi

		if (!communication) break;

		for(i=0;i<MAX_USER_NUM;i++){
            if(verbosity_level>=DebugUDP) printf("[UDP SENDER] %d 's addrees is %d and its status is %d \n",i,client[i].addr.sin_addr.s_addr ,client[i].status);
			if(client[i].status==1 && client[i].addr.sin_addr.s_addr!=0){
                client_addr = client[i].addr;
                slen = sizeof(struct sockaddr);

				ret = send_UDP(socket, msg, packet_len, 0, &client_addr, (socklen_t) slen);
                if (verbosity_level>=DebugUDP && ret) printf("[UDP SENDER] Sending world to %d (id = %d)\n", client_addr.sin_addr.s_addr, client[i].id);
                if (ret == -2) {
                    printf("Could not send user data to client %d\n", i);
                    client[i].status = 0;
                    Vehicle *vec = World_getVehicle(&world, client[i].id);
                    if (vec) {
                        if (verbosity_level>=DebugUDP) printf("[UDP SENDER] Vehicle with id %d disconnected, its last coordinates are "
                                                              "(x,y,theta) = (%f,%f,%f)\n", vec->id, vec->x, vec->y, vec->theta);
                        client[i].x = vec->x;
                        client[i].y = vec->y;
                        client[i].theta = vec->theta;
                        World_detachVehicle(&world, vec);
                    }
                    ret = close(client[i].socket_TCP);
                    if (errno != EBADF) ERROR_HELPER(ret, "Error closing socket"); 
                }

			}
		}

        if (verbosity_level>=DebugUDP) printf("[UDP SENDER] World sent to everyone, done\n");

        //faccio la free delle strutture dati che non mi servono più (tanto verranno ricreate alla prossima ciclata)

        Packet_free((PacketHeader *) worldup);       // Essendo worldupdate verrà liberato anche update contenuto

        //sblocco il semaforo

        ret = sem_post(&sem_world);
        ERROR_HELPER(ret, "Failed to post sem_world in thread_UDP_sender");

        if (verbosity_level>=DebugUDP) printf("[UDP SENDER] Post executed\n");

        usleep(1000);
    }

    free(msg);
    //if (arg) free(arg);     // messo controllo perché arg è condiviso tra i due thread
    pthread_exit(NULL);

}

void* thread_server_UDP_receiver(void* args){

    int ret, socket  = 0;
    char *msg = (char *)malloc(DIM_BUFF*sizeof(char));

    VehicleUpdatePacket *packet;

    thread_server_UDP_args *arg = (thread_server_UDP_args*) args;
    clients* client=arg->list;

    socket = arg->socket_desc_UDP_server;
    struct sockaddr_in client_addr;
    socklen_t slen;
    int bytes_read = 0;

	//ricevere tutte le intenzioni di movimento e le sostituisce nei veicoli


    while (communication) {

        slen = sizeof(struct sockaddr);

        if (verbosity_level>=DebugUDP) printf("\n[UDP RECEIVER] I'm alive!\n");

        ret = recv_UDP_packet(socket, msg, 0, (struct sockaddr *) &client_addr, &slen, &bytes_read);
        if (ret == -2) continue;

        if (!communication) break;

        if (verbosity_level>=DebugUDP) printf("[UDP RECEIVER] Packet received\n");

        PacketHeader* header = (PacketHeader*) msg;

        ret = sem_wait(&sem_world);
        ERROR_HELPER(ret, "Failed to wait sem_world in thread_UDP_receiver");

        // deserializzo il pacchetto appena ricevuto
        packet = (VehicleUpdatePacket *) Packet_deserialize(msg, header->size);

        if (verbosity_level>=DebugUDP) printf("[UDP RECEIVER] Packet deserialized\n");

        if (verbosity_level>=DebugUDP) printf("Rotational force = %f and Translational force = %f \n",packet->rotational_force , packet->translational_force);

        client[packet->id].addr = client_addr;
        if (verbosity_level>=DebugUDP) printf("[UDP RECEIVER] %d client's address updated and it's %d \n",packet->id,client[packet->id].addr.sin_addr.s_addr);

        //sostuisco le sue intenzioni di movimento ricevute nelle sue variabili nel mondo

        Vehicle* v=World_getVehicle(&world,packet->id);
        if(v!=0){
            if (verbosity_level>=DebugUDP) printf("[UDP RECEIVER] Ricevute nuove intenzioni movimento da client %d\n", packet->id);
            v->translational_force_update=packet->translational_force;
            v->rotational_force_update=packet->rotational_force;
        }

        World_update(&world);

		ret = sem_post(&sem_world);
		ERROR_HELPER(ret, "Failed to post sem_world in thread_UDP_receiver");

        if (verbosity_level>=DebugUDP) printf("[UDP RECEIVER] Client's forces updated\n");
        Packet_free((PacketHeader *) packet);
    }

    free(msg);
    //if (arg) free(arg);     // messo controllo perché arg è condiviso tra i due thread
    pthread_exit(NULL);

}


int main(int argc, char **argv) {

    if (argc<3) {
        printf("usage: %s <elevation_image> <texture_image>\n", argv[1]);
        exit(-1);
    }

    if (verbosity_level>=General) printf("DEBUG MODE\n");

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

    ret = sem_init(&sem_world, 0, 1);
    ERROR_HELPER(ret, "Failed to initialization of sem_world");

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

	// inizializzo array di utenti
	for (i = 0; i < MAX_USER_NUM; i++) {
		utenti[i].username[0]='\0';
		utenti[i].password[0]='\0';
	}

	//creo il mondo

	World_init(&world, surface_elevation, surface_texture,  0.5, 0.5, 0.5);

    //definisco struttura per bind
    struct sockaddr_in server_addr_UDP;	//indirizzo del server
    memset(&server_addr_UDP, 0, sizeof(server_addr_UDP));

    //imposto i valori della struttura

    server_addr_UDP.sin_addr.s_addr=INADDR_ANY;
    server_addr_UDP.sin_family=AF_INET;
    server_addr_UDP.sin_port=htons(SERVER_PORT_UDP);

    //creo la socket

    server_socket_UDP=socket(AF_INET,SOCK_DGRAM,0);
    ERROR_HELPER(server_socket_UDP,"error creating socket UDP \n");

    // faccio la bind

    ret=bind(server_socket_UDP,(struct sockaddr*) &server_addr_UDP,sizeof(server_addr_UDP));
    ERROR_HELPER(ret,"error binding socket  UDP \n");

    //creo i thread che si occuperanno di ricevere le forze dai vari client e di inviare un world update packet a tutti i client (UDP)

    pthread_t thread_UDP_receiver;
    pthread_t thread_UDP_sender;

    thread_server_UDP_args* args_UDP=(thread_server_UDP_args*)malloc(sizeof(thread_server_UDP_args));
    args_UDP->socket_desc_UDP_server=server_socket_UDP;
    args_UDP->server_addr_UDP = server_addr_UDP;
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


    //definisco struttura per server socket TCP
    struct sockaddr_in server_addr = {0};

    //imposto i valori delle struttura per accettare connessioni

    server_addr.sin_addr.s_addr=INADDR_ANY;
    server_addr.sin_family=AF_INET;
    server_addr.sin_port=htons(SERVER_PORT_TCP);

    //creo la socket

    server_socket_TCP=socket(AF_INET,SOCK_STREAM,0);
    ERROR_HELPER(server_socket_TCP,"error creating socket \n");

    // setto il campo SO_REUSEADDR per evitare problemi per precedenti crash

    int val_socket_reuse = 1;
    ret = setsockopt(server_socket_TCP, SOL_SOCKET, SO_REUSEADDR, &val_socket_reuse, sizeof(int));
    ERROR_HELPER(ret, "Could not set SO_REUSEADDR to socket");

    // faccio la bind

    ret=bind(server_socket_TCP,(struct sockaddr*) &server_addr,sizeof(struct sockaddr_in));
    ERROR_HELPER(ret,"error binding socket \n");

    // rendo la socket in grado di accettare connessioni

    ret=listen(server_socket_TCP,3);
    ERROR_HELPER(ret,"error setting socket to listen connections \n");

    while(main_var){

        // definisco descrittore della socket con cui parlerò poi con ogni client

        struct sockaddr_in* client_addr= (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
        memset(client_addr,0,sizeof(struct sockaddr_in));
        int client_socket;
        int slen=sizeof(struct sockaddr_in);

        // mi metto in attesa di connessioni

        client_socket=accept(server_socket_TCP,(struct sockaddr*) &client_addr,(socklen_t*)&slen);
        if (client_socket == -1 && errno == EBADF) break;
        ERROR_HELPER(client_socket,"error accepting connections \n");
        
        //lancio il thread che si occuperà di parlare poi con il singolo client che si è connesso

        pthread_t thread;

        thread_server_TCP_args* args_TCP=(thread_server_TCP_args*)malloc(sizeof(thread_server_TCP_args));
        args_TCP->socket_desc_TCP_client = client_socket;
        args_TCP->list=client;
        args_TCP->elevation_map = surface_elevation;
        args_TCP->map = surface_texture;

        ret = pthread_create(&thread, NULL,thread_server_TCP,args_TCP);
        ERROR_HELPER(ret, "Could not create thread");

        ret = pthread_detach(thread);
        ERROR_HELPER(ret, "Could not detach thread");


    }

    // libero l'elevation map, la texture map e la texture del veicolo
    if (surface_elevation) Image_free(surface_elevation);
    if (surface_texture) Image_free(surface_texture);
    if (vehicle_texture) Image_free(vehicle_texture);
    free(args_UDP);

    return 0;
}
