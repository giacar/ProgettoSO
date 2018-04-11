// #include <GL/glut.h> // not needed here
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <semaphore.h>
#include <errno.h>

#include "image.h"
#include "surface.h"
#include "world.h"
#include "vehicle.h"
#include "world_viewer.h"
#include "utils.h"
#include "common.h"

user_table utenti[MAX_USER_NUM];
// Creare lista di tutti i client connessi che verranno man mano aggiunti e rimossi
// deve contenere ID texture. Stessa cosa vale con una lista di client disconnessi
// in modo da ripristinare lo stato in caso di un nuovo login
client_connected  online_client[MAX_USER_NUM];    //vettore di puntatori a strutture dati client_connected
client_disconnected  offline_client[MAX_USER_NUM];   //vettore di puntatori a strutture dati client_disconnected


sem_t sem_utenti;
sem_t sem_thread_UDP;

/**
NOTA PER IL SERVER: Quando c'è un errore su una socket di tipo ENOTCONN (sia UDP che TCP), allora vuol dire che il client si è disconnesso (indipendetemente
se la disconnessione è avvenuta in maniera pulita oppure no). Ciò si verifica anche quando la recv e la receive_from restituiscono 0. In tal caso, il server
dovrà salvare lo stato del client che si è disconnesso (semplicemente, si sposta il suo contenuto dalla sua cella di online_client alla sua cella di
offline_client, e poi si mette a NULL la sua cella in online_client.
**/




void* thread_server_TCP(void* thread_server_TCP_args){

    int ret;

    //implementare protoccollo login e aggiungere il nuovo client al mondo ed alla lista dei client connessi
    thread_server_TCP_args *arg = (thread_server_TCP_args *)args;
    int socket=arg->socket_desc_TCP_client;
    Image* elevation_map = arg->elevation_map;      //elevation map
    Image* map = arg->map;                          //map

    // Strutture dati per il Login
    char user_att[64];
    char pass_att[64];
    char pass_giusta[64];
    int login_reply;
    int id_utente = -1;
    int login_status;        //questa variabile serve per far capire al server se il client che si è connesso è un nuovo utente oppure si era già registrato

    //l'utilizzo della sola variabile login_reply non permette di identificare i due tipi di client, perchè viene modificata per altri scopi

    // Ricezione dell'user

    ret = recv_TCP(socket, user_att, 1, 0);
    if (ret == -2){
        printf("Could not receive client username\n");
        pthread_exit(0);
    }
    else PTHREAD_ERROR_HELPER(ret, "Failed to read username from client");

    ret = sem_wait(&sem_utenti);
    ERROR_HELPER(ret, "Error in sem_utenti wait");

	// Verifico se user già registrato
	int idx = -1;
	for (int i = 0; utenti[i] != NULL; i++) {
		if (!strcmp(user_att,utenti[i].username)) {
			idx = i;
		}
	}

	// Nuovo user
	if (idx == -1) {
		// inserisco user nel primo slot libero
		login_reply = 0;	// È un nuovo user
        login_status = login_reply;
		int i;
		for (i = 0; i < MAX_USER_NUM && utenti[i].username != NULL; i++);

		/**
			Slot utenti disponibili terminati
			Devo informare il client che gli slot sono terminati, in modo che termini e che il thread corrente termini
		**/
		if (i >= MAX_USER_NUM) login_reply = -1;

		else {
			idx = i;
			strcpy(utenti[idx].username,user_att);
		}

		ret = sem_post(&sem_utenti);
		ERROR_HELPER(ret, "Error in sem_utenti post");

		// informo il client che è un nuovo user o che gli slot sono terminati

        ret = send_TCP(socket, &login_reply, sizeof(int), 0);
        if (ret == -2) {
            printf("Could not send login_reply to client\n");
            pthread_exit(0);
        }
        else PTHREAD_ERROR_HELPER(ret, "Failed to send login_reply to client");

		if (login_reply == -1){
            ret=close(socket);
            pthread_exit(-1);		// Slot user terminati devo terminare il thread corrente
        }

        //ricezione password

        ret = recv_TCP(socket, pass_att, 1, 0);
        if (ret == -2){
             printf("Could not receive password from client\n");
             pthread_exit(0);
        }
        else PTHREAD_ERROR_HELPER(ret, "Failed to read password from client");



		// registrazione password ed id
		ret = sem_wait(&sem_utenti);
        PTHREAD_ERROR_HELPER(ret, "Error in sem_utenti wait: failed to register username & password");

		strcpy(utenti[idx].password,pass_att);


		ret = sem_post(&sem_utenti);
        ERROR_HELPER(ret, "Error in sem_utenti post: failed to register username & password");

		id_utente = idx;
	}

	// Già registrato
	else if (idx > -1) {
        strcpy(pass_giusta,utenti[idx].password);

		ret = sem_post(&sem_utenti);
        ERROR_HELPER(ret, "Error in sem_utenti post");

		// informo il client che è già registrato
		login_reply = 1;
        login_status = login_reply;

        ret = send_TCP(socket, &login_reply, sizeof(int), 0);
        if (ret == -2) {
            printf("Could not send login_reply to client\n");
            pthread_exit(0);
        }
        else PTHREAD_ERROR_HELPER(ret, "Failed to send login_reply to client");

		//ricezione password
		do {

            ret = recv_TCP(socket, pass_att, 1, 0);
            if (ret == -2) {
                printf("Could not receive password from client\n");
                pthread_exit(0);
            }
            else PTHREAD_ERROR_HELPER(ret, "Failed to read password from client");

			// password giusta
			if (!strcmp(pass_att,pass_giusta)) {
				login_reply = 1;

                ret = send(socket, &login_reply, sizeof(int), 0);
                if (ret == -2) {
                    printf("Could not send login_reply to client\n");
                    pthread_exit(0);
                }
			}

			else {
				login_reply = -1;

                ret = send_TCP(socket, &login_reply, sizeof(int), 0);
                if (ret == -2){
                     printf("Could not send login_reply to client\n");
                     pthread_exit(0);
                }
                else PTHREAD_ERROR_HELPER(ret, "Failed to send login_reply to client");
			}
		} while (strcmp(pass_att,pass_giusta));



	}

    /**OK! Il client si è connesso, adesso ha bisogno di sapere le informazioni (texture) di tutti gli altri client che sono connessi nel mondo.
    Spediamo al client ogni cella dell'array di client connessi che non è messa a NULL, sottoforma di ImagePacket.
    **/

    client_connected* temp;     //serve per salvare lo stato del client al momento della disconnessione

    int j;
    char client_connesso[DIM_BUFF];
    size_t msg_len;
    ImagePacket* client = (ImagePacket*) malloc(sizeof(ImagePacket));
    PacketHeader img_head;
    client->header=img_head;
    client->header->type = PostTexture;
    client->header->size = sizeof(ImagePacket);

    for (j = 0; j < MAX_USER_NUM; j++){
        if (client_connected[j] != NULL && j != idx){
            client->id = j;
            client->texture = client_connected[j]->texture;

            msg_len = Packet_serialize(client_connesso, &(client->header));

            ret = send_TCP(socket, client_connesso, msg_len, 0);
            if (ret == -2){
                printf("Could not send data over socket\n");
                pthread_exit(0);
            }
            else PTHREAD_ERROR_HELPER(ret, "Could not send data over socket");
        }
    }


	//entrare nel loop di ricezione richiesta e comunicazione nuove connessioni / disconnessioni

    //nuovo utente: richiesta di id

    if (login_status == 0){

        char idPacket[DIM_BUFF];

        ret = recv_TCP(socket, idPacket, sizeof(idPacket), 0);
        if (ret == -2){
            printf("Could not receive id request from client\n");
            pthread_exit(0);
        }
        PTHREAD_ERROR_HELPER(ret, "Could not receive id request from client");

        msg_len = ret;
        idPacket[msg_len] = '\0';

        IdPacket* id = Packet_deserialize(idPacket, msg_len);
        if (id->header->type != GetId) PTHREAD_ERROR_HELPER(-1, "Error in packet type (client id)");

        if (id->id == -1){
            id->id = idx;
            size_t idPacket_response_len;
            idPacket_response_len = Packet_serialize(idPacket, &(id->header));

            ret = send_TCP(socket, idPacket, idPacket_response_len, 0);
            if (ret == -2){
                printf("Could not send id response to client\n");
                pthread_exit(0);
            }
            PTHREAD_ERROR_HELPER(ret, "Could not send id response to client\n");


        }
        else{
            PTHREAD_ERROR_HELPER(-1, "Error in id packet request!");
            pthread_exit(0);
        }

        //nuovo utente: invia la sua texture

        char texture_utente[DIM_BUFF];

        ret = recv_TCP(socket, texture_utente, sizeof(ImagePacket), 0);
        if (ret == -2){
            printf("Could not receive client texture\n");
            pthread_exit(0);
        }
        PTHREAD_ERROR_HELPER(ret, "Could not receive client texture");

        msg_len = ret;
        texture_utente[msg_len] = '\0';

        ImagePacket* client_texture = Packet_deserialize(texture_utente, msg_len);
        if (client_texture->header->type!=GetTexture) PTHREAD_ERROR_HELPER(-1, "Error in client texture packet!");

        client_connected[idx]->id = idx;
        client_connected[idx]->texture = client_texture->texture;

        size_t texture_utente_len;
        texture_utente_len = Packet_serialize(texture_utente, &(client_texture->header));

        ret = send_TCP(socket, texture_utente, texture_utente_len, 0);
        if (ret == -2){
            printf("Could not send client texture\n");
            temp = client_connected[idx];
            client_connected[idx] = NULL;
            client_disconnected[idx] = temp;
            pthread_exit(0);
        }
        else PTHREAD_ERROR_HELPER(ret, "Could not send client texture\n");


    }

    else if (login_status == 1){

        /**client invia un IdPacket, con id settato a -1, noi rispondiamo con un ImagePacket, contenente la sua texture (salvata precedentemente) e il suo id
         * in pratica gli inviamo tutto insieme
        **/

        char idPacket_buf[DIM_BUFF];
        size_t idPacket_buf_len;

        ret = recv_TCP(socket, idPacket_buf, sizeof(IdPacket), 0);
        if (ret == -2) {
            printf("Could not receive IdPacket from client\n");
            pthread_exit(0);
        }
        else PTHREAD_ERROR_HELPER(ret, "Could not receive IdPacket from client");

        msg_len = ret;
        idPacket_buf[msg_len] = '\0';

        IdPacket* received = Packet_deserialize(idPacket_buf, msg_len);
        if (received->header->type != GetTexture) PTHREAD_ERROR_HELPER(-1, "Connection error (texture request)");

        ImagePacket* client_texture = (ImagePacket*) malloc(sizeof(ImagePacket));
        PacketHeader client_header;
        client_texture->header = &client_header;
        client_texture->id = idx;
        client_texture->texture = client_connected[idx];
        client_texture->header->type = PostTexture;
        client_texture->header->size = sizeof(ImagePacket);

        idPacket_buf_len = Packet_serialize(idPacket_buf, &(client_texture->header));

        ret = send_TCP(socket, idPacket_buf, idPacket_buf_len, 0);
        if (ret == -2) {
            printf("Could not send client texture and id\n");
            //pthread_exit(0);
        }
        else PTHREAD_ERROR_HELPER(ret, "Could not send client texture and id");
    }

    //utente invia la richiesta di elevation map

    char elevation_map_buffer[DIM_BUFF];
    size_t elevation_map_len;

    ret = recv_TCP(socket, elevation_map_buffer, sizeof(ImagePacket), 0);
    if (ret == -2) {
        printf("Could not receive elevation map request\n");
        temp = client_connected[idx];
        client_connected[idx] = NULL;
        client_disconnected[idx] = temp;
        //pthread_exit(0);
    }
    else PTHREAD_ERROR_HELPER(ret, "Could not receive elevation map request");

    msg_len = ret;
    elevation_map_buffer[msg_len] = '\0';

    IdPacket* elevation= Packet_deserialize(elevation_map_buffer,msg_len);
    if(elevation->header->type!=GetElevation) ERROR_HELPER(-1,"error in communication \n");

    int id = elevation->id;

    ImagePacket* ele_map = (ImagePacket*) malloc(sizeof(ImagePacket));
    PacketHeader elevation_header;
    ele_map->header = &elevation_header;
    ele_map->texture = elevation_map;
    ele_map->id = id;
    ele_map->header->type = PostElevation;
    ele_map->header->size = sizeof(ImagePacket);

    elevation_map_len = Packet_serialize(elevation_map_buffer, &(ele_map->header));

    ret = send_TCP(socket, elevation_map, elevation_map_len, 0);
    if (ret == -2) {
        printf("Could not send elevation map\n");
        temp = client_connected[idx];
        client_connected[idx] = NULL;
        client_disconnected[idx] = temp;
        //pthread_exit(0);
    }
    else PTHREAD_ERROR_HELPER(ret, "Could not send elevation map to client");

    //ricezione richiesta mappa e invio mappa

    char map_buffer[DIM_BUFF];
    size_t map_len;

    ret = recv_TCP(socket, map_buffer, sizeof(IdPacket), 0);
    if (ret == -2) {
        printf("Could not receive map request\n");
        temp = client_connected[idx];
        client_connected[idx] = NULL;
        client_disconnected[idx] = temp;
        //pthread_exit(0);
    }
    else PTHREAD_ERROR_HELPER(ret, "Could not receive map request");

    msg_len = ret;
    map[msg_len] = '\0';

    IdPacket* map = Packet_deserialize(map_buffer, msg_len);
    if(map->header->type!=GetMap)   ERROR_HELPER(-1, "Connection error (map request)");

    id = map->id;

    //invio mappa
    ImagePacket* map_packet = (ImagePacket*) malloc(sizeof(ImagePacket));
    PacketHeader map_header;
    map_packet->header = &map_header;
    map_packet->id = id;
    map_packet->texture = map;
    map_packet->header->type = PostTexture;
    map_packet->header->size = sizeof(ImagePacket);

    map_len = Packet_serialize(map_buffer, &(map_packet->header));

    ret = send_TCP(socket, map_buffer, map_len, 0);
    if (ret == -2){
        printf("Could not send map to client\n");
        temp = client_connected[idx];
        client_connected[idx] = NULL;
        client_disconnected[idx] = temp;
        //pthread_exit(0);
    }
    else PTHREAD_ERROR_HELPER(ret, "Could not send map to client\n");

    /** Ultimata la connessione e l'inizializzazione del client, c'è bisogno di inviargli lo stato di tutti gli altri client già connessi [TODO] **/

    ImagePacket* client_alive = (ImagePacket*) malloc(sizeof(ImagePacket));
    PacketHeader client_alive_header;
    client_alive->header = &(client_alive_header);
    client_alive->header->type=PostTexture;
    client_alive->header->size=sizeof(ImagePacket);

    char client_alive_buf[DIM_BUFF];
    size_t alive_len;

    //inviamo tutto il client_connected: le celle messe a NULL il client le eliminerà

    for (i = 0; i < MAX_USER_NUM; i++){
        if (i != idx){
            client_alive->id = client_connected[i]->id;
            client_alive->texture = client_connected[i]->texture;

            alive_len = Packet_serialize(client_alive_buf, &(client_alive->header));

            ret = send_TCP(socket, client_alive_buf, alive_len, 0);
            if (ret == -2){
                printf("Could not send user data to client\n");
                temp = client_connected[idx];
                client_connected[idx] = NULL;
                client_disconnected[idx] = temp;
                pthread_exit(0);
            }
            else PTHREAD_ERROR_HELPER(ret, "Could not send user data to client\n");
        }
    }

    /** FINE LAVORO TCP **/




}

void* thread_server_UDP(void* thread_server_UDP_args){

	//ricevere tutte le intenzioni di movimento e salvarle nella lista dei movimenti da effettuare





	//ad intervalli regolari integrare il mondo  svuorare lista movimenti ed inviare le nuove poszioni di tutti i clientn a tutti i client







}


int main(int argc, char **argv) {
    if (argc<3) {
        printf("usage: %s <elevation_image> <texture_image>\n", argv[1]);
        exit(-1);
    }
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

    // Inizializzo i due array di client connessi e disconnessi

    int i;
    for (i = 0; i < MAX_USER_NUM; i++){
        online_client[i] = NULL;
        offline_client[i] = NULL;
    }

    int ret;

	// inizializzo i semafori di mutex per online client e offline client list e per tabella utenti

	ret = sem_init(&sem_utenti, 1, 0);
	ERROR_HELPER(ret, "Failed to initialization of sem_utenti");

	// inizializzo array di utenti
	for (i = 0; i < MAX_USER_NUM; i++) {
		utenti[i].username = NULL;
		utenti[i].password = NULL;
		utenti[i].id = -1;
	}

    //definisco descrittore server socket e struttura per bind

    int server_socket_UDP;
    struct server_addr_UDP {0};

    //imposto i valori della struttura

    server_addr_UDP.sin_addr.in_addr=INADDR_ANY;
    server_addr_UDP.sin_family=AF_INET;
    server_addr_UDP.sin_port=htons(SERVER_PORT_UDP);

    //creo la socket

    server_socket_UDP=socket(AF_INET,SOCK_DGRAM,0);
    ERROR_HELPER(server_socket_UDP,"error creating socket UDP \n");

    // faccio la bind

    ret=bind(server_socket_UDP,(struct sockaddr*) &server_addr_UDP,sizeof(struct sockaddr_in);
    ERROR_HELPER(ret,"error binding socket  UDP \n");

    //creo i thread che si occuperanno di ricevere le forze dai vari client e di inviare un world update packet a tutti i client (UDP)

    ret = sem_init(&sem_thread_UDP, 1, 0);
	ERROR_HELPER(ret, "Failed to initialization of sem_thread_UDP");


    pthread_t thread_UDP_receiver;
    pthread_t thread_UDP_sender;

    thread_server_UDP_args* args_UDP=(thread_server_UDP_args*)malloc(sizeof(thread_server_UDP_args));
    args_UDP->socket_desc_UDP_server=server_socket_UDP;
    args_UDP->connected=online_client;
    args_UDP->disconnected=offline_client;

    //creo i thread

    ret = pthread_create(&thread_UDP_receiver, NULL,thread_server_UDP_receiver,args_UDP);
    ERROR_HELPER(ret, "Could not create thread");

    ret = pthread_create(&thread_UDP_sender, NULL, thread_server_UDP_sender, args_UDP);
    ERROR_HELPER(ret, "Could not create thread");

    // impongo di non aspettare terminazione in modo da non bloccare tutto il programma

    ret = pthread_detach(thread);
    ERROR_HELPER(ret, "Could not detach thread");

    ret = pthread_detach(thread);
    ERROR_HELPER(ret, "Could not detach thread");


    //definisco descrittore server socket TCP

    int server_socket_TCP;
    struct server_addr {0};

    //imposto i valori delle struttura per accettare connessioni

    server_addr.sin_addr.in_addr=INADDR_ANY;
    server_addr.sin_family=AF_INET;
    server_addr.sin_port=htons(SERVER_PORT_TCP);

    //creo la socket

    server_socket_TCP=socket(AF_INET,SOCK_STREAM,0);
    ERROR_HELPER(server_socket_TCP,"error creating socket \n");

    // faccio la bind

    ret=bind(server_socket_TCP,(struct sockaddr*) &server_addr,sizeof(struct sockaddr_in);
    ERROR_HELPER(ret,"error binding socket \n");

    // rendo la socket in grado di accettare connessioni

    ret=listen(server_socket_TCP,3);
    ERROR_HELPER(ret,"error setting socket to listen connections \n");

    while(1){

        // definisco descrittore della socket con cui parlerò poi con ogni client

        struct sockaddr_in*  client_addr {0};
        int client_socket;

        // mi metto in attesa di connessioni

        client_socket=accept(server_socket_TCP,(struct sockaddr*) &client_addr,sizeof(struct sockadrr_in));
        ERROR_HELPER(client_socket,"error accepting connections \n");

        //lancio il thread che si occuperà di parlare poi con il singolo client che si è connesso

        pthread_t thread;

        thread_server_TCP_args* args_TCP=(thread_server_TCP_args*)malloc(sizeof(thread_server_TCP_args));
        args_TCP->socket_desc_TCP_client = client_socket;
        args_TCP->connected=online_client;
        args_TCP->disconnected=offline_client;
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
