#include <GL/glut.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <semaphore.h>
#include <errno.h>

#include "image.h"
#include "surface.h"
#include "world.h"
#include "vehicle.h"
#include "world_viewer.h"
#include "so_game_protocol.h"
#include "common.h"

int window;
WorldViewer viewer;
World world;
Vehicle* vehicle; // The vehicle
int ret;
//QuitPacket quit_packet;	// packet to handle the exit of client
//sem_t sem_quit_handle;	// handles the quit_packet manipulation which is in critical section
char[64] username;
char[64] password;

//se recv() restituisce 0 o un errore di socket (ENOTCONN et similia), vuol dire che la comunicazione è stata chiusa. Idem per recvfrom
//send() invece, in caso di errore (ENOTCONN et similia), restituisce -1 e setta errno a un certo valore. Idem per sendto

/**
void gestione_quit(){

    //catturato il segnale di quit, inviamo una stringa con dentro "quit" al server, che la riceve ed elimina il client dalla lista dei connessi
    //(di fatto, il server prende lo stato del client contenuto nella lista di client_connected e lo sposta alla fine di client_disonnected)
    while (1){
        printf("Caught quit signal");

        ret = sem_wait(&sem_quit_handle);
        ERROR_HELPER(ret, "sem_wait failed on gestione_quit");
        // CS
        char[17] quit_command = quit_packet -> quit_command;
        socket_desc_TCP = quit_packet -> socket_desc_TCP;
        // CS
        ret = sem_post(&sem_quit_handle);
        ERROR_HELPER(ret, "sem_post failed on gestione_quit");

        quit_command = "quit";
        int msg_len = strlen(quit_command);
        quit_command[msg_len] = '\0';

        //invia il messaggio di quit al server e attende un ack

        while ((ret = send(socket_desc_TCP, quit_command, msg_len, 0) < 0)){
            if (errno == EINTR) continue;
            ERROR_HELPER(-1, "Could not send quit command to server");
        }

        char[17] ack;
        while ((ret = recv(socket_desc_TCP, ack, msg_len, 0) < 0)){
            if (errno == EINTR) continue;
            ERROR_HELPER(-1, "Could not receive ack from server");
        }

        ack[msg_len] = '\0';

        if (!strcmp(ack, "ok")) {
            printf("Error in quit! Trying again...");
            continue;
        }

        else{
        	ret = sem_wait(&sem_quit_handle);
        	ERROR_HELPER(ret, "sem_wait failed on gestione_quit");

            quit_packet -> quit = 1;

            ret = sem_post(&sem_quit_handle);
            ERROR_HELPER(ret, "sem_post failed on gestione_quit");

            printf("Success! Goodbye");
            break;
        }
    }

}**/

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

    thread_client_args arg = (thread_client_args) client_args;
    int socket_UDP = arg->socket_desc_UDP;
    int socket=arg->socket_desc_TCP;
    int id=arg->id;
    Image* map_texture=arg->map_texture;
    Vehicle vehicle=arg->v
    struct sockaddr_in server_UDP = arg->server_addr_UDP;

    //Ricezione degli ImagePacket contenenti id e texture di tutti i client presenti nel mondo
    char[1024] user;
    int msg_len;

    char* finish_command = FINISH_COMMAND;
    size_t finish_command_len = strlen(finish_command);

    while (1){

        //Il server crea una lista collegata di stringhe che ha serializzato da pacchetti e ne invia una per volta, quando riceve quella con dentro scritto
        //"Finish" possiamo fermarci

        while ((ret = recv(socket_desc_TCP, user, msg_len, 0)) < 0){
            if (errno == EINTR) continue;
            else if (errno == ENOTCONN) {
                printf("Server closed connection, could not receive users already in world. Goodbye!");
                exit(0);
            }
            ERROR_HELPER(-1, "Could not receive users already in world");
        }

        if (ret == finish_command_len && !memcmp(user, finish_command, finish_command_len)) break;


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

        ImagePacket* client = Packet_deserialize(&user, msg_len);
        int id = client->id;
        if (client->image == NULL){
            Vehicle* v = World_getVehicle(&w, id);
            World_detachVehicle(&w, v);
        }
        else{
            Vehicle* v = (Vehicle*) malloc(sizeof(Vehicle));
            Vehicle_init(v, &world, id, client->image);
        }
    }

    /**
    //DISCONNESSIONE
	struct sigaction sa;

    //setup the signal handler
    sa.sa_handler = gestione_quit;

    //restart the system call, if at all possible
    sa.sa_flags = SA_RESTART;

    //block every signal during handling
    sigfillset(&sa.sa_mask);

    while(1){
        //intercept SIGQUIT

        if (sigaction(SIGQUIT, &sa, NULL) == -1) ERROR_HELPER(-1, "Error: cannot handle SIQUIT");

    }**/

}

void* thread_listener_udp(void* client_args){   //todo

    /**COMUNICAZIONE UDP**/
    /**

    Client, via UDP invia dei pacchetti VehicleUpdate al server. Il contenuto del VehicleUpdate viene prelevato da desired_force,
    è contenuto nella struttura del veicolo e si mette in attesa di pacchetti WorldUpdate che contengono al
    loro interno una lista collegata. Ricevuti questi pacchetti, smonta la lista collegata all'interno e per ogni elemento della lista,
    preso l'id, preleva dal mondo il veicolo con quell'id e ne aggiorna lo stato. I pacchetti di veicoli ancora non aggiunti al proprio mondo
    vengono ignorati (per necessità).

    **/

    thread_client_args arg = (thread_client_args) client_args;
    int socket_UDP = arg->socket_desc_UDP;
    int socket=arg->socket_desc_TCP;
    int id=arg->id;
    Image* map_texture=arg->map_texture;
    Vehicle vehicle=arg->v
    struct sockaddr_in server_UDP = arg->server_addr_UDP;


    /**
    Ciclo while che opera fino a quando il client è in funzione. Quando non deve più lavorare, riceve un segnale di quit (DA IMPLEMENTARE)
    **/
    while(1){

        /**
    	// use of critical section implementation for instruction: while(quit_packet -> quit == 0){
    	unsigned exit_dute = 0;

    	ret = sem_wait(&sem_quit_handle);
    	ERROR_HELPER(ret, "sem_wait failed in thread_listener_udp");

    	if (quit_packet -> quit != 0) exit_dute = 1;

    	ret = sem_post(&sem_quit_handle);
    	ERROR_HELPER(ret, "sem_post failed in thread_listener_udp");

    	if (exit_dute) break;
        **/

    //creazione di un pacchetto di update personale da inviare al server.
        VehicleUpdatePacket* update = (VehicleUpdatePacket*) malloc(sizeof(VehicleUpdatePacket));
        update->translational_force = v->translational_force_update;
        update->rotational_force = v->rotational_force_update;
        update->header->size = sizeof(VehicleUpdatePacket);
        update->header->type = VehicleUpdate;

        char[1024] vehicle_update;
        int vehicle_update_len = Packet_serialize(vehicle_update, &(update->header));

        while ((ret = sendto(socket_UDP, vehicle_update, vehicle_update_len, 0, (struct sockaddr*) server_UDP, sizeof(server_UDP)) < 0)){
            if (errno == EINTR) continue;
            else if (errno == ENOTCONN) {
                printf("Server closed connection, could not send my update. Goodbye!");
                exit(0);
            }
            ERROR_HELPER(-1, "Could not send vehicle updates to server");
        }

    //richiesta di tutti gli update degli altri veicoli, per aggiornare il proprio mondo

        char[1024] world_update;
        int world_update_len;

        while ((ret = recvfrom(socket_UDP, world_update, world_update_len, 0, (struct sockaddr*) server_UDP, sizeof(server_UDP)) < 0)){
            if (errno == EINTR) continue;
            else if (errno == ENOTCONN) {
                printf("Server closed connection, could not receive world update. Goodbye!");
                exit(0);
            }
            ERROR_HELPER(-1, "Could not receive num vehicles from server");
        }

        world_update[world_update_len] = '\0';

    //estriamo il numero di veicoli e gli update di ogni veicolo

        WorldUpdatePacket* wup = Packet_deserialize(world_update, world_update_len);
        int num_vehicles = wup->num_vehicles;
        ClientUpdate* client_update = wup->updates; //VETTOREEEEEEEEE di client update

        int i;
        for(i=0;i<num_vehicles;i++){
			ClientUpdate update = *(client_update+i*sizeof(ClientUpdate));


            //estrapoliamo tutti i dati per il singolo veicolo presente nel mondo, identificato da "id"

            int id = update->id;
            float x = update->x;
            float y = update->y;
            float z = update->camera_to_world[14];
            float theta = update->theta;

            //Aggiornamento veicolo
            Vehicle* v = World_getVehicle(&world, id);
            v->x = x;
            v->y = y;
            v->z = camera_to_world[14];
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


void keyPressed(unsigned char key, int x, int y)
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
}

int main(int argc, char **argv) {
  if (argc<3) {
    printf("usage: %s <server_address> <player texture>\n", argv[1]);
    exit(-1);
  }





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
  struct sockaddr_in server_addr{0};    //some fields are required to be filled with 0

  //creating a socket TCP
  socket_desc = socket(AF_INET, SOCK_STREAM, 0);
  ERROR_HELPER(socket_desc, "Could not create socket");


  //set up parameters for connection
  server_addr.sin_addr.in_addr = inet_addr(argv[1]);
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(SERVER_PORT_TCP);

  //initiate a connection to the socket
  ret = connect(socket_desc, (struct sockaddr*) &server_addr, sizeof(struct sockaddr_in));
  ERROR_HELPER(ret, "Could not connect to socket");

  //variable for UDP socket
  int socket_desc_UDP;
  struct sockaddr_in server_addr_UDP{0};
  //creating UDP sopcket
  socket_desc_UDP = socket(AF_INET, SOCK_DGRAM, 0);
  ERROR_HELPER(socket_desc_UDP, "Could not create socket udp");
  //set up parameters
  server_addr_UDP.sin_addr.in_addr = inet_addr(argv[1]);
  server_addr_UDP.sin_family = AF_INET;
  server_addr_UDP.sin_port = htons(SERVER_PORT_UDP);
  //bind UDP socket
  ret = bind(socket_desc_UDP, (struct sockaddr*) &server_addr_UDP, sizeof(struct sockaddr_in));
  ERROR_HELPER(ret, "Could not connect to socket (udp)");

  /**
  //inizializziamo il pacchetto di quit, con variabile int quit settata a 0, ovviamente all'inizio non è in sezione critica
  quit_packet -> quit = 0;
  quit_packet -> socket_desc_TCP = socket_desc;
  **/

	/**LOGIN**/
	/** Client inserisce username e password appena si connette:
	*   -se utente non esiste allora i dati che ha inserito vengono usati per registrare l'utente
	*	-variabile login_state ha 3 valori 0 se nuovo utente registrato, 1 se già esistente e -1 se password sbagliata
	* [TOCOMPLETE]
	**/

  	int login_state;		// in this variabile there is the login's state
  	int user_length;
  	int pass_length;

    while (1){
        printf("LOGIN\n Please enter username: ");
        scanf("%s", username);
        user_length = strlen(username);
        if (user_length >= 64) printf("ERROR! username too length, please retry.");
        else break;
    }

	while((ret = send(socket_desc, username, user_length, 0)) < 0) {
		if (errno == EINTR) continue;
        else if (errno == ENOTCONN) {
            printf("Server closed connection, cannot send username. Goodbye!");
            exit(0);
        }
		ERROR_HELPER(ret, "Failed to send login data");
	}
	while((ret = recv(socket_desc, login_state, sizeof(int), 0)) < 0) {
		if (errno == EINTR) continue;
        else if (errno == ENOTCONN) {
            printf("Server closed connection, cannot receive login state. Goodbye!");
            exit(0);
        }
		ERROR_HELPER(ret, "Failed to update login's state");
	}

	if (login_state) printf("\nWelcome back %s.", username);	
	else if (login_state == 0) printf("\nWelcome %s.", username);
	else {
		// Non c'è più posto tra gli user
		printf("\nÈ stato raggiunto il massimo numero di utenti. Riprova più tardi.\n");
		exit(0);
	}

	printf(" Please enter password: ");
	scanf("%s", password);
	printf("\n");
	pass_length = strlen(password);

	while((ret = send(socket_desc, password, pass_length, 0)) < 0) {
		if (errno == EINTR) continue;
        else if (errno == ENOTCONN) {
            printf("Server closed connection, could not send password. Goodbye!");
            exit(0);
        }
		ERROR_HELPER(ret, "Failed to send login data");
	}
	while((ret = recv(socket_desc, &login_state, sizeof(int), 0)) < 0) {
		if (errno == EINTR) continue;
        else if (errno == ENOTCONN) {
            printf("Server closed connection, could not receiver login's state. Goodbye!");
            exit(0);
        }
		ERROR_HELPER(ret, "Failed to update login's state");
	}

	while (login_state == -1) {
		printf("Incorrect Password, please insert it again: ");
		scanf("%s", password);
		printf("\n");
		while((ret = send(socket_desc, password, login_length, 0)) < 0) {
			if (errno == EINTR) continue;
            else if (errno == ENOTCONN) {
                printf("Server closed connection, could not send password. Goodbye!");
                exit(0);
            }
			ERROR_HELPER(ret, "Failed to send login data");
		}

		while((ret = recv(socket_desc, &login_state, 1, 0)) < 0) {
			if (errno == EINTR) continue;
            else if (errno == ENOTCONN) {
                printf("Server closed connection, could not receiver login's state. Goodbye!");
                exit(0);
            }
			ERROR_HELPER(ret, "Failed to receive login's state");
		}
	}

	if (login_state == 0) printf("You're signed up with user: %s, welcome to the game!\n", username);

  	/** Se utente esiste e la password è corretta, allora invia al server un richiesta di tutti i dati salvati nella
  	*	precedente sessione e il server risponde con tali dati
   	*	[TODO]
   	**/

   	else if (login_state == 1) {
   		printf("Login success, welcome back %s\n", username);

   		// invio richiesta di ripristino
   		char[] req = "Respawn\n";
   		int req_length = strlen(req);

		while((ret = send(socket_desc, req, req_length, 0)) < 0) {
			if (errno == EINTR) continue;
            else if (errno == ENOTCONN) {
                printf("Server closed connection, could not send respawn request. Goodbye!");
                exit(0);
            }
			ERROR_HELPER(ret, "Failed to send login data");
		}

		// ricezione stato precedente
		// [TODO]
		char[1024] respawn;
		int respawn_len = 0;

		ImagePacket *texture = (Image *)malloc(sizeof(ImagePacket));
		if (texture == NULL) ERROR_HELPER(-1, "Failed to allocate respawn structure");
		
		while(1) {
			ret = recv(arg->socket_desc, respawn+respawn_len, 1, 0);

			if (ret == -1 && errno == EINTR) continue;
			if (errno == ENOTCONN) ERROR_HELPER(ret, "Server closed connection");

			if (respawn[respawn_len] == '\0') {
				respawn_len++;
				break;
			}

			respawn_len++;
		}

		&(texture->header) = Packet_deserialize(respawn, respawn_len);

		// DA FINIRE E CONTROLLARNE LA CORRETTEZZA
   	}

  }

  //requesting and receving the ID
  IdPacket* request_id=(IdPacket*)malloc(sizeof(IdPacket));
  request_id->header->type=GetId;
  request_id->header->size=sizeof(IdPacket);
  request_id->id=-1;

  char idPacket_request[1024];
  char idPacket[1024];
  size_t idPacket_request_len = Packet_serialize(&idPacket_request,&(request_id->header));
  size_t msg_len;


  while ((ret = send(socket_desc, idPacket_request, idPacket_request_len, 0)) < 0){
    if (errno == EINTR) continue;
    else if (errno == ENOTCONN) {
        printf("Server closed connection, could not send id request. Goodbye!");
        exit(0);
    }
    ERROR_HELPER(-1, "Could not send id request  to socket");
  }

  while ((ret = recv(socket_desc, idPacket, msg_len, 0)) < 0){
    if (errno == EINTR) continue;
    else if (errno == ENOTCONN) {
        printf("Server closed connection, could not receive id. Goodbye");
        exit(0);
    }
    ERROR_HELPER(-1, "Could not read id from socket");
  }

  idPacket[msg_len] = '\0';

  IdPacket* id=Packet_deserialize(&idPacket, msg_len);
  if(id->header->type!=GetId) ERROR_HELPER(-1,"Error in packet type \n");


  // sending my texture
  char texture_for_server[1024];


  ImagePacket* my_texture=malloc(sizeof(ImagePacket));
  texture->header->type=PostTexture;
  texture->header->size=sizeof(ImagePacket);
  texture->id=id->id;
  texture->image=&my_texture_for_server;

  size_t texture_for_server_len = Packet_serialize(&texture_for_server, &(my_texture->header));


  while ((ret = send(socket_desc, texture_for_server, texture_for_server_len, 0) < 0){
      if (errno == EINTR) continue;
      else if (errno == ENOTCONN) {
          printf("Server closed connection, could non send texture_for_server. Goodbye!");
          exit(0);
      }
      ERROR_HELPER(-1, "Could not send my texture for server");
  }
  // receving my texture from server

  char my_texture_from_server[1024];

    while ((ret = recv(socket_desc, my_texture_from_server, msg_len, 0)) < 0){
      if (errno == EINTR) continue;
      else if (errno == ENOTCONN) {
          printf("Server closed connection, could not receive my_texture_from_server. Goodbye!");
          exit(0);
      }
      ERROR_HELPER(-1, "Could not read my texture from socket");
  }

  my_texture_from_server[msg_len] = '\0';

  ImagePacket* my_texture_received=Packet_deserialize(&my_texture_from_server,msg_len);
  if(my_texture_received!=my_texture) ERROR_HELPER(-1,"error in communication: texture not matching! \n");


  //requesting and receving elevation map
  IdPacket* request_elevation=(IdPacket*)malloc(sizeof(IdPacket));
  request_elevation->id=id->id;
  request_elevation->header->size=sizeof(IdPacket);
  request_elevation->header->type=GetElevation;

  char request_elevation_for_server[1024];
  char elevation_map[1024];
  size_t request_elevation_len =Packet_serialize(&request_elevation_for_server, &(request_elevation->header));

  while ((ret = send(socket_desc, request_elevation_for_server, request_elevation_for_server_len, 0) < 0){
      if (errno == EINTR) continue;
      else if (errno == ENOTCONN) {
          printf("Server closed connection, could not send request elevation. Goodbye!");
          exit(0);
      }
      ERROR_HELPER(-1, "Could not send my texture for server");
  }

  while ((ret = recv(socket_desc, elevation_map, msg_len, 0)) < 0){
      if (errno == EINTR) continue;
      else if (errno == ENOTCONN) {
          printf("Server closed connection, could not receive elevation map. Goodbye!");
          exit(0);
      }
      ERROR_HELPER(-1, "Could not read elevation map from socket");
  }

  elevation_map[msg_len] = '\0';
  ImagePacket* elevation= Packet_deserialize(&elevation_map,msg_len);
  if(elevation->header->type!=PostElevation && elevation->id!=0) ERROR_HELPER(-1,"error in communication \n");


  //requesting and receving map
  char request_texture_map_for_server;
  char texture_map[1024];
  IdPacket* request_map=(IdPacket*)malloc(sizeof(IdPacket));
  request_map->header->type=GetTexture;
  request_map->header->size=sizeof(IdPacket);
  request_map->id=id->id;
  size_t request_texture_map_for_server_len=Packet_serialize(&request_texture_map_for_server, &(request_map->header));

    while ((ret = send(socket_desc, request_texture_map_for_server, request_texture_map_for_server_len, 0) < 0){
      if (errno == EINTR) continue;
      else if (errno == ENOTCONN) {
          printf("Server closed connection, could not send texture map request. Goodbye!");
          exit(0);
      }
      ERROR_HELPER(-1, "Could not send my texture for server");
  }

  while ((ret = recv(socket_desc, texture_map, msg_len, 0)) < 0){
      if (errno == EINTR) continue;
      else if (errno == ENOTCONN) {
          printf("Server closed connection, could not receive texture map. Goodbye!");
          exit(0);
      }
      ERROR_HELPER(-1, "Could not read map texture from socket");
  }

  texture_map[msg_len] = '\0';

  ImagePacket* map=Packet_deserialize(&texture_map,msg_len);
  if(map->header->type!=PostTexture && map->id!=0) ERROR_HELPER(-1,"error in protocol \n");



  // these come from the server
  int my_id = id->id;
  Image* map_elevation = elevation->image;
  Image* map_texture = map->image;
  Image* my_texture_from_server = my_texture_received->image;

  // construct the world
  World_init(&world, map_elevation, map_texture, 0.5, 0.5, 0.5);
  vehicle=(Vehicle*) malloc(sizeof(Vehicle));
  Vehicle_init(&vehicle, &world, my_id, my_texture_from_server);
  World_addVehicle(&world, v);

  // spawn a thread that will listen the update messages from
  // the server, and sends back the controls
  // the update for yourself are written in the desired_*_force
  // fields of the vehicle variable
  // when the server notifies a new player has joined the game
  // request the texture and add the player to the pool
  /*FILLME*/
  thread_client_args* args=malloc(sizeof(thread_client_args));
  args->v=vehicle;
  args->id=my_id
  args->socket_desc_TCP=socket_desc;
  args->socket_desc_UDP=socket_desc_UDP;
  args->map_texture=map_texture;
  args->server_addr_UDP = server_addr_UDP;
  pthread_t thread_tcp;
  pthread_t thread_udp;

  /**
  // create the semaphore to manipulate the quit_packet in critical section
  ret = sem_init(&sem_quit_handle, 0, 1);
  ERROR_HELPER(ret, "Could not create squit semaphore");
  **/

  ret = pthread_create(&thread_tcp, NULL, thread_listener_tcp,args);
  ERROR_HELPER(ret, "Could not create thread");

  ret = pthread_detach(&thread);
  ERROR_HELPER(ret, "Could not detach thread");

  ret = pthread_create(&thread_udp, NULL, thread_listener_udp,args);
  ERROR_HELPER(ret, "Could not create thread");

  ret = pthread_detach(&thread);
  ERROR_HELPER(ret, "Could not detach thread");

  WorldViewer_runGlobal(&world, vehicle, &argc, argv);

  // cleanup
  World_destroy(&world);

  /* Devo distruggere il semaforo ma non posso farlo successivamente a causa delle detach
  ret = sem_destroy(&sem_quit_handle);
  ERROR_HELPER(ret, "Could not destroy the semaphore");
  */

  return 0;
}
