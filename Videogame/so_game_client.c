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
int ret;
char username[64];
char password[64];

//se recv() restituisce 0 o un errore di socket (ENOTCONN et similia), vuol dire che la comunicazione è stata chiusa. Idem per recvfrom
//send() invece, in caso di errore (ENOTCONN et similia), restituisce -1 e setta errno a un certo valore. Idem per sendto



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

    thread_client_args* arg = (thread_client_args*) client_args;
    int socket=arg->socket_desc_TCP;        //socket TCP
    //int id=arg->id;
    //Image* map_texture=arg->map_texture;
    //Vehicle vehicle=arg->v;

    //Ricezione degli ImagePacket contenenti id e texture di tutti i client presenti nel mondo
    char user[DIM_BUFF];
    int msg_len;

    while (1){

        //Il server invia le celle dell'array dei connessi che non sono messe a NULL.
        ret = recv_TCP(socket, user, sizeof(ImagePacket), 0);
        PTHREAD_ERROR_HELPER(ret, "Could not receive users already in world");

        msg_len=ret;
        user[msg_len]='\0';
        msg_len++;


        // to send its texture
        // sent from server to client
        //       (with type=PostTexture and id=0) to assign the surface texture
        //       (with type=PostElevation and id=0) to assign the surface texture
        //       (with type=PostTexture and id>0) to assign the  texture to vehicle id

        /**typedef struct {
          PacketHeader header;
          int id;
          Image* image;
        } ImagePacket;**/

        PacketHeader* clienth = Packet_deserialize(user, msg_len);
        if (clienth->type==PostTexture){
			ImagePacket* client=(ImagePacket*)clienth;
				
			int id = client->id;


			Vehicle* v = (Vehicle*) malloc(sizeof(Vehicle));
			Vehicle_init(v, &world, id, client->image);
		}
		else if(clienth->type==GetId){
			IdPacket* clientd=(IdPacket*)clienth;
			if(clientd->id>=MAX_USER_NUM);
			else{
				int id=clientd->id;
				Vehicle* v = World_getVehicle(&world, id);
				World_detachVehicle(&world, v);
			}
		}
    }

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

    thread_client_args* arg = (thread_client_args*) client_args;
    int socket_UDP = arg->socket_desc_UDP_M;
    int id=arg->id;
    //Image* map_texture=arg->map_texture;
    Vehicle veicolo=arg->v;
    struct sockaddr_in server_UDP = arg->server_addr_UDP_M;
    int slen = sizeof(server_UDP);


    /**
    Ciclo while che opera fino a quando il client è in funzione.
    **/

    while(1){



    //creazione di un pacchetto di update personale da inviare al server.
        VehicleUpdatePacket* update = (VehicleUpdatePacket*) malloc(sizeof(VehicleUpdatePacket));
        PacketHeader update_head;
        update_head.size = sizeof(VehicleUpdatePacket);
        update_head.type = VehicleUpdate;
        
        update->header=update_head;
        update->translational_force = veicolo.translational_force_update;
        update->rotational_force = veicolo.rotational_force_update;
        update->id = id;

        char vehicle_update[DIM_BUFF];
        int vehicle_update_len = Packet_serialize(vehicle_update, &(update->header));

        ret = send_UDP(socket_UDP, vehicle_update, vehicle_update_len, 0, (struct sockaddr*) &server_UDP, slen);
        PTHREAD_ERROR_HELPER(ret, "Could not send vehicle updates to server");

        Packet_free(&update_head);


    }

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

    thread_client_args* arg = (thread_client_args*) client_args;
    int socket_UDP = arg->socket_desc_UDP_W;
    //int id=arg->id;
    //Image* map_texture=arg->map_texture;
    //Vehicle vehicle=arg->v;
    struct sockaddr_in server_UDP = arg->server_addr_UDP_W;
    int slen = sizeof(server_UDP);


    /**
    Ciclo while che opera fino a quando il client è in funzione.
    **/

    while(1){


    //richiesta di tutti gli update degli altri veicoli, per aggiornare il proprio mondo
    //da sistemare la dimensione

        char world_update[DIM_BUFF];
        char world_update_len[DIM_BUFF];

        //non sappiamo quanto è grande la stringa che ci deve arrivare e che dobbiamo convertire in numero intero
        ret= recv_UDP(socket_UDP,world_update_len, 1,0, (struct sockaddr *) &server_UDP, (socklen_t*) &slen);
        PTHREAD_ERROR_HELPER(-1, "Could not receive size of world update");

        int dimensione_mondo = atoi(world_update_len);

        ret= recv_UDP(socket_UDP,world_update,dimensione_mondo,0, (struct sockaddr *) &server_UDP, (socklen_t*) &slen);
        PTHREAD_ERROR_HELPER(-1, "Could not receive world update");


        world_update[dimensione_mondo] = '\0';
        dimensione_mondo++;

    //estriamo il numero di veicoli e gli update di ogni veicolo

        WorldUpdatePacket* wup = (WorldUpdatePacket*) Packet_deserialize(world_update, dimensione_mondo);
        int num_vehicles = wup->num_vehicles;
        ClientUpdate* client_update = wup->updates; //VETTOREEEEEEEEE di client update

        int i;
        for(i=0;i<num_vehicles;i++){
			ClientUpdate update = *(client_update+i*sizeof(ClientUpdate));


            //estrapoliamo tutti i dati per il singolo veicolo presente nel mondo, identificato da "id"

            int id = update.id;
            float x = update.x;
            float y = update.y;
            //float z = update.camera_to_world[14];
            float theta = update.theta;

            //Aggiornamento veicolo
            Vehicle* v = World_getVehicle(&world, id);
            v->x = x;
            v->y = y;
            v->z = v->camera_to_world[14];
            v->theta = theta;

            printf("Data update!");

        }


    }

    /**uscire dal while, significa che il client si sta disconnettendo. Il server deve salvare il suo stato da qualche parte, per ripristinarlo più avanti
       se il client si connetterà ancora**/

    /**funzioni di send e receive per comunicazione UDP**/
    //sendto(int sockfd, void* buff, size_t #bytes, int flags, const struct sockaddr* to, socklen_t addrlen)
    //recvfrom(int sockfd, void* buff, size_t #bytes, int flags, const struct sockaddr* from, socklen_t addrlen)
}


/**void keyPressed(unsigned char key, int x, int y)
{
  switch(key){
  case 27:
    glutDestroyWindow(window);
    exit(0);
  case ' ':
    vehicle->translational_force_update = 0;
    vehicle->rotational_force_update = 0;
    break;
  case '+':
    viewer.zoom *= 1.1f;
    break;
  case '-':
    viewer.zoom /= 1.1f;
    break;
  case '1':
    viewer.view_type = Inside;
    break;
  case '2':
    viewer.view_type = Outside;
    break;
  case '3':
    viewer.view_type = Global;
    break;
  }
}


void specialInput(int key, int x, int y) {
  switch(key){
  case GLUT_KEY_UP:
    vehicle->translational_force_update += 0.1;
    break;
  case GLUT_KEY_DOWN:
    vehicle->translational_force_update -= 0.1;
    break;
  case GLUT_KEY_LEFT:
    vehicle->rotational_force_update += 0.1;
    break;
  case GLUT_KEY_RIGHT:
    vehicle->rotational_force_update -= 0.1;
    break;
  case GLUT_KEY_PAGE_UP:
    viewer.camera_z+=0.1;
    break;
  case GLUT_KEY_PAGE_DOWN:
    viewer.camera_z-=0.1;
    break;
  }
}


void display(void) {
  WorldViewer_draw(&viewer);
}


void reshape(int width, int height) {
  WorldViewer_reshapeViewport(&viewer, width, height);
}

void idle(void) {
  World_update(&world);
  usleep(30000);
  glutPostRedisplay();

  // decay the commands
  vehicle->translational_force_update *= 0.999;
  vehicle->rotational_force_update *= 0.7;
}**/

int main(int argc, char **argv) {
	if (argc<3) {
		printf("usage: %s <server_address> <player texture>\n", argv[1]);
		exit(-1);
	}

    if (DEBUG) printf("DEBUG MODE\n");

	printf("loading texture image from %s ... ", argv[2]);
	Image* my_texture = Image_load(argv[2]);
	if (my_texture) {
		printf("Done! \n");
	} else {
		printf("Fail! \n");
	}




	Image* my_texture_for_server = my_texture;
	// todo: connect to the server
	//   -get ad id
	//   -send your texture to the server (so that all can see you)
	//   -get an elevation map
	//   -get the texture of the surface

	//variables for handling a socket
	int socket_desc;
	struct sockaddr_in server_addr = {0};    //some fields are required to be filled with 0

	//creating a socket TCP
	socket_desc = socket(AF_INET, SOCK_STREAM, 0);
	ERROR_HELPER(socket_desc, "Could not create socket");


	//set up parameters for connection
	server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(SERVER_PORT_TCP);

	//initiate a connection to the socket
	ret = connect(socket_desc, (struct sockaddr*) &server_addr, sizeof(struct sockaddr_in));
	ERROR_HELPER(ret, "Could not connect to socket");

	//variable for UDP_M socket
	int socket_desc_UDP_M;
	struct sockaddr_in server_addr_UDP_M = {0};
	//creating UDP sopcket
	socket_desc_UDP_M = socket(AF_INET, SOCK_DGRAM, 0);
	ERROR_HELPER(socket_desc_UDP_M, "Could not create socket udp");
	//set up parameters
	server_addr_UDP_M.sin_addr.s_addr = inet_addr("127.0.0.1");
	server_addr_UDP_M.sin_family = AF_INET;
	server_addr_UDP_M.sin_port = htons(SERVER_PORT_UDP_M);
	//bind UDP socket
	/*
	ret = bind(socket_desc_UDP_M, (struct sockaddr*) &server_addr_UDP_M, sizeof(struct sockaddr_in));
	ERROR_HELPER(ret, "Could not connect to socket (udp bind M)");*/

	//variable for UDP_W socket
	int socket_desc_UDP_W;
	struct sockaddr_in server_addr_UDP_W = {0};
	//creating UDP sopcket
	socket_desc_UDP_W = socket(AF_INET, SOCK_DGRAM, 0);
	ERROR_HELPER(socket_desc_UDP_W, "Could not create socket udp");
	//set up parameters
	server_addr_UDP_W.sin_addr.s_addr = inet_addr("127.0.0.1");
	server_addr_UDP_W.sin_family = AF_INET;
	server_addr_UDP_W.sin_port = htons(SERVER_PORT_UDP_W);
	//bind UDP socket
	ret = bind(socket_desc_UDP_W, (struct sockaddr*) &server_addr_UDP_W, sizeof(struct sockaddr_in));
	ERROR_HELPER(ret, "Could not connect to socket (udp bind W)");


	/**LOGIN**/
	/** Client inserisce username e password appena si connette:
	*   -se utente non esiste allora i dati che ha inserito vengono usati per registrare l'utente
	*	-variabile login_state ha 3 valori 0 se nuovo utente registrato, 1 se già esistente e -1 se password sbagliata
	* [TOCOMPLETE]
	**/

  	char stato_login[DIM_BUFF];		// in this variabile there is the login's state
  	int user_length;
  	int pass_length;
    int login_state;

    while (1) {
        printf("LOGIN\nPlease enter username: ");
        ret = scanf("%s", username);
        if (DEBUG) printf("[LOGIN] scanf fatta: %s\n", username);
        user_length = strlen(username);
        if (user_length > 64) printf("ERROR! username too length, please retry.\n");
        else break;
    }

    if (DEBUG) printf("[LOGIN] username inserito\n");

	ret = send_TCP(socket_desc, username, user_length+1, 0);
	ERROR_HELPER(ret, "Failed to send login data");

	if (DEBUG) printf("[LOGIN] username INVIATO\n");

	ret = recv_TCP(socket_desc, stato_login, 1, 0);
	ERROR_HELPER(ret, "Failed to update login's state");

	if (DEBUG) printf("[LOGIN] login state per username ricevuto\n");

    login_state = atoi(stato_login);

	if (login_state) printf("\nWelcome back %s.", username);
	else if (login_state == 0) printf("\nWelcome %s.", username);
	else {
		// Non c'è più posto tra gli user
		printf("\nÈ stato raggiunto il massimo numero di utenti. Riprova più tardi.\n");
		exit(0);
	}

	printf(" Please enter password: ");
	ret = scanf("%s", password);
	printf("\n");
	if (DEBUG) printf("[LOGIN] scanf fatta: %s\n", password);
	pass_length = strlen(password);

	ret = send_TCP(socket_desc, password, pass_length+1, 0);
	ERROR_HELPER(ret, "Failed to send login data");

	if (DEBUG) printf("[LOGIN] password INVIATA\n");

	ret = recv_TCP(socket_desc, stato_login, 1, 0);
	ERROR_HELPER(ret, "Failed to update login's state");

	if (DEBUG) printf("[LOGIN] login state per password ricevuta\n");

    login_state = atoi(stato_login);

	while (login_state == -1) {
		printf("Incorrect Password, please insert it again: ");
		ret = scanf("%s", password);
		printf("\n");
		pass_length = strlen(password);

		ret = send_TCP(socket_desc, password, pass_length+1, 0);
		ERROR_HELPER(ret, "Failed to send login data");

		if (DEBUG) printf("[LOGIN] Nuova password inviata\n");;

		ret = recv_TCP(socket_desc, stato_login, 1, 0);
		ERROR_HELPER(ret, "Failed to receive login's state");

		if (DEBUG) printf("[LOGIN] login state della password ricevuto\n");;

        login_state = atoi(stato_login);
	}

	int my_id = -2;
	Image* my_texture_from_server = NULL;
    size_t msg_len;                 //utile dichiararla qui, visto che andrà usata in ogni caso

	if (login_state == 0){
		printf("You're signed up with user: %s, welcome to the game!\n", username);	//password salvata

		//requesting and receving the ID
		IdPacket* request_id=(IdPacket*)malloc(sizeof(IdPacket));
		PacketHeader id_head;
		request_id->header=id_head;
		request_id->header.type=GetId;
		request_id->header.size=sizeof(IdPacket);
		request_id->id=-1;

		char idPacket_request[DIM_BUFF];
		char idPacket[DIM_BUFF];
		size_t idPacket_request_len = Packet_serialize(idPacket_request,&(request_id->header));

		ret = send_TCP(socket_desc, idPacket_request, idPacket_request_len, 0);
		ERROR_HELPER(ret, "Could not send id request  to socket");

		ret = recv_TCP(socket_desc, idPacket, sizeof(idPacket), 0);
		ERROR_HELPER(ret, "Could not read id from socket");

		msg_len=ret;
		idPacket[msg_len] = '\0';
        msg_len++;

		IdPacket* id = (IdPacket*) Packet_deserialize(idPacket, msg_len);
		if (id->header.type!=GetId) ERROR_HELPER(-1,"Error in packet type \n");


		// sending my texture
		char texture_for_server[DIM_BUFF];


		ImagePacket* my_texture=malloc(sizeof(ImagePacket));
		PacketHeader img_head;
		my_texture->header=img_head;
		my_texture->header.type=PostTexture;
		my_texture->header.size=sizeof(ImagePacket);
		my_texture->id=id->id;
		my_texture->image=my_texture_for_server;

		size_t texture_for_server_len = Packet_serialize(texture_for_server, &(my_texture->header));

		ret = send_TCP(socket_desc, texture_for_server, texture_for_server_len, 0);
		ERROR_HELPER(ret, "Could not send my texture for server");

        Packet_free(&img_head);

		// receving my texture from server

		char my_texture_server[DIM_BUFF];

		ret = recv_TCP(socket_desc, my_texture_server, sizeof(ImagePacket), 0);
		ERROR_HELPER(ret, "Could not read my texture from socket");

		msg_len=ret;
		my_texture_server[msg_len] = '\0';
        msg_len++;

		ImagePacket* my_texture_received = (ImagePacket*) Packet_deserialize(my_texture_server,msg_len);
		if(my_texture_received!=my_texture) ERROR_HELPER(-1,"error in communication: texture not matching! \n");

		// these come from the server
		my_id = id->id;
		my_texture_from_server = my_texture_received->image;


	}

  	/** Se utente esiste e la password è corretta, allora il server gli invia il suo id, così il client potrà utilizzarlo successivamente quando spedirà
  	 * l'IdPacket per ricevere la texture
   	 *	[TODO]
   	 **/

   	else if (login_state == 1) {
   		printf("Login success, welcome back %s\n", username);
		//requesting and receving texture and id
		IdPacket* request_texture=(IdPacket*)malloc(sizeof(IdPacket));
		PacketHeader request_texture_head;
		request_texture->header=request_texture_head;
		request_texture->id=-1; //ancora non lo conosco lo scopro nella risposta
		request_texture->header.size=sizeof(IdPacket);
		request_texture->header.type=GetTexture;

		char request_texture_for_server[DIM_BUFF];
		char my_texture[DIM_BUFF];
		size_t request_texture_len =Packet_serialize(request_texture_for_server, &(request_texture->header));

		ret = send_TCP(socket_desc, request_texture_for_server, request_texture_len, 0);
		ERROR_HELPER(ret, "Could not send my texture for server");

        Packet_free(&request_texture_head);

		ret = recv_TCP(socket_desc, my_texture,sizeof(ImagePacket), 0);
		ERROR_HELPER(ret, "Could not read my texture from socket");

		msg_len=ret;
		my_texture[msg_len] = '\0';
		msg_len++;

		ImagePacket* my_texture_received = (ImagePacket*) Packet_deserialize(my_texture,msg_len);
		if(my_texture_received->header.type!=PostTexture && my_texture_received->id==0) ERROR_HELPER(-1,"error in communication \n");

		// these come from the server
		my_id = my_texture_received->id;
		my_texture_from_server = my_texture_received->image;


		// ricezione ID in modo da inserirla successivamente nel ID packet
		// [TODO]


		// DA FINIRE E CONTROLLARNE LA CORRETTEZZA
	}


  	/**
  	Se il client è un nuovo utente manderà la richiesta dell'id al server con un IdPacket col campo id settato a -1. Altrimenti, quel campo id sarà settato
  	al suo id che aveva prima di disconnettersi. Tutto ciò serve per far capire al server da dove deve estrapolare la texture: se id = -1 allora riceve la
  	texture dal client e gliela reinvia. Altrimenti, lui la prende dalla sua cella di client_connected (o disconnected //DA DEFINIRE!) e gliela reinvia.
	**/




	//requesting and receving elevation map
	IdPacket* request_elevation=(IdPacket*)malloc(sizeof(IdPacket));
	PacketHeader request_elevation_head;
	request_elevation->header=request_elevation_head;
	request_elevation->id=my_id;
	request_elevation->header.size=sizeof(IdPacket);
	request_elevation->header.type=GetElevation;

	char request_elevation_for_server[DIM_BUFF];
	char elevation_map[DIM_BUFF];
	size_t request_elevation_len =Packet_serialize(request_elevation_for_server, &(request_elevation->header));


	ret = send_TCP(socket_desc, request_elevation_for_server, request_elevation_len, 0);
	ERROR_HELPER(ret, "Could not send my texture for server");

    Packet_free(&request_elevation_head);

	ret = recv_TCP(socket_desc, elevation_map,sizeof(ImagePacket), 0);
	ERROR_HELPER(ret, "Could not read elevation map from socket");

	msg_len=ret;
	elevation_map[msg_len] = '\0';
	msg_len++;
	ImagePacket* elevation = (ImagePacket*) Packet_deserialize(elevation_map,msg_len);
	if(elevation->header.type!=PostElevation && elevation->id!=0) ERROR_HELPER(-1,"error in communication \n");


	//requesting and receving map
	char request_texture_map_for_server[DIM_BUFF];
	char texture_map[DIM_BUFF];
	IdPacket* request_map=(IdPacket*)malloc(sizeof(IdPacket));
	PacketHeader request_map_head;
	request_map->header=request_map_head;
	request_map->header.type=GetTexture;
	request_map->header.size=sizeof(IdPacket);
	request_map->id=my_id;
	size_t request_texture_map_for_server_len=Packet_serialize(request_texture_map_for_server, &(request_map->header));


  	ret = send_TCP(socket_desc, request_texture_map_for_server, request_texture_map_for_server_len, 0);
  	ERROR_HELPER(ret, "Could not send my texture for server");

    Packet_free(&request_map_head);

  	ret = recv_TCP(socket_desc, texture_map, sizeof(ImagePacket), 0);
  	ERROR_HELPER(ret, "Could not read map texture from socket");

	msg_len=ret;
	texture_map[msg_len] = '\0';
	msg_len++;

	ImagePacket* map = (ImagePacket*) Packet_deserialize(texture_map,msg_len);
	if(map->header.type!=PostTexture && map->id!=0) ERROR_HELPER(-1,"error in protocol \n");



	// these come from the server

	Image* map_elevation = elevation->image;
	Image* map_texture = map->image;


	// construct the world
	World_init(&world, map_elevation, map_texture, 0.5, 0.5, 0.5);
	vehicle=(Vehicle*) malloc(sizeof(Vehicle));
	Vehicle_init(vehicle, &world, my_id, my_texture_from_server);
	World_addVehicle(&world, vehicle);

	// spawn a thread that will listen the update messages from
	// the server, and sends back the controls
	// the update for yourself are written in the desired_*_force
	// fields of the vehicle variable
	// when the server notifies a new player has joined the game
	// request the texture and add the player to the pool
	/*FILLME*/
	thread_client_args* args=malloc(sizeof(thread_client_args));
	args->v=*vehicle;
	args->id=my_id;
	args->socket_desc_TCP=socket_desc;
	args->socket_desc_UDP_M=socket_desc_UDP_M;
	args->socket_desc_UDP_W=socket_desc_UDP_W;
	args->map_texture=map_texture;
	args->server_addr_UDP_M = server_addr_UDP_M;
	args->server_addr_UDP_W = server_addr_UDP_W;
	pthread_t thread_tcp;
	pthread_t thread_udp_M;
	pthread_t thread_udp_W;


	ret = pthread_create(&thread_tcp, NULL, thread_listener_tcp, (void*) args);
	ERROR_HELPER(ret, "Could not create thread");

	ret = pthread_detach(thread_tcp);
	ERROR_HELPER(ret, "Could not detach thread");

	ret = pthread_create(&thread_udp_M, NULL, thread_listener_udp_M, (void*) args);
	ERROR_HELPER(ret, "Could not create thread");

	ret = pthread_detach(thread_udp_M);
	ERROR_HELPER(ret, "Could not detach thread");

	ret = pthread_create(&thread_udp_W, NULL, thread_listener_udp_W, (void*) args);
	ERROR_HELPER(ret, "Could not create thread");

	ret = pthread_detach(thread_udp_W);
	ERROR_HELPER(ret, "Could not detach thread");

	WorldViewer_runGlobal(&world, vehicle, &argc, argv);

	// cleanup
	World_destroy(&world);

	return 0;
}
