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


/**
NOTA PER IL SERVER: Quando c'è un errore su una socket di tipo ENOTCONN (sia UDP che TCP), allora vuol dire che il client si è disconnesso (indipendetemente
se la disconnessione è avvenuta in maniera pulita oppure no). Ciò si verifica anche quando la recv e la receive_from restituiscono 0. In tal caso, il server
dovrà salvare lo stato del client che si è disconnesso (semplicemente, si sposta il suo contenuto dalla sua cella di online_client alla sua cella di
offline_client, e poi si mette a NULL la sua cella in online_client.
**/




void* thread_server_TCP(void* thread_server_TCP_args){

	int ret, bytes_read = 0;

	//implementare protoccollo login e aggiungere il nuovo client al mondo ed alla lista dei client connessi
	thread_server_TCP_args *arg = (thread_server_TCP_args *)args;
	int socket=args->socket_desc_TCP_client;

	// Strutture dati per il Login
	char[64] user_att;
	char[64] pass_att;
	char[64] pass_giusta;
	int login_reply;
	int id_utente = -1;

	// Ricezione dell'user
	while (1) {
		ret = recv(arg->socket_desc_TCP_client, user_att+bytes_read, 1, 0);

		if (ret == -1 && errno == EINTR) continue;
        ERROR_HELPER(ret, "Failed to read username from client");

		if (user_att[bytes_read] == '\n') {
			user_att[bytes_read]='\0';
			bytes_read++;
			break;
		}

		bytes_read++;
	}

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
		while ((ret = send(arg->socket_desc_TCP_client, login_reply, sizeof(int), 0)) < 0) {
			if (errno == EINTR) continue;
			ERROR_HELPER(ret, "Failed to send login_reply to client");
		}

		if (login_reply == -1){
			 ret=close(socket);
			 pthread_exit(-1);		// Slot user terminati devo terminare il thread corrente
			 
		 }

		//ricezione password
		bytes_read = 0;
		while (1) {
			ret = recv(arg->socket_desc_TCP_client, pass_att+bytes_read, 1, 0);

			if (ret == -1 && errno == EINTR) continue;
		    ERROR_HELPER(ret, "Failed to read password from client");

			if (user_att[bytes_read] == '\n') {
				user_att[bytes_read]='\0';
				bytes_read++;
				break;
			}

			bytes_read++;
		}

		// registrazione password ed id
		ret = sem_wait(&sem_utenti);
        ERROR_HELPER(ret, "Error in sem_utenti wait: failed to register username & password");

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
		while ((ret = send(arg->socket_desc_TCP_client, &login_reply, sizeof(int), 0)) < 0) {
			if (errno == EINTR) continue;
			ERROR_HELPER(-1, "Failed to send login_reply to client");
		}

		//ricezione password
		do {
			bytes_read = 0;
			while (1) {
				ret = recv(arg->socket_desc_TCP_client, pass_att+bytes_read, 1, 0);

				if (ret == -1 && errno == EINTR) continue;
			    ERROR_HELPER(ret, "Failed to read password from client");

				if (user_att[bytes_read] == '\n') {
					user_att[bytes_read]='\0';
					bytes_read++;
					break;
				}

				bytes_read++;
			}

			// password giusta
			if (!strcmp(pass_att,pass_giusta)) {
				login_reply = 1;
				while ((ret = send(arg->socket_desc_TCP_client, &login_reply, sizeof(int), 0)) < 0) {
					if (errno == EINTR) continue;
					ERROR_HELPER(-1, "Failed to send login_reply to client");
				}
				break;
			}

			else {
				login_reply = -1;
				while ((ret = send(arg->socket_desc_TCP_client, &login_reply, sizeof(int), 0)) < 0) {
					if (errno == EINTR) continue;
					ERROR_HELPER(-1, "Failed to send login_reply to client");
				}
			}
		} while (strcmp(pass_att,pass_giusta));



	}

    /**OK! Il client si è connesso, adesso ha bisogno di sapere le informazioni (texture) di tutti gli altri client che sono connessi nel mondo.
    Spediamo al client ogni cella dell'array di client connessi che non è messa a NULL, sottoforma di ImagePacket.
    Da questo momento in poi, se il client si disconnette, in qualsiasi modo, c'è bisogno di salvare il suo stato
    **/

    int j;
    char[1000000] client_connesso;
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

            while ((ret = send(arg->socket_desc_TCP_client, client_connesso, msg_len, 0))<0){
                if (errno == EINTR) continue;
                else if (errno == ENOTCONN) {
                    printf("Client closed connection\n");
                    client_connected* temp = client_connected[idx];
                    client_connected[idx] = NULL;
                    client_disconnected[idx] = temp;
                }
                ERROR_HELPER(-1, "Could not send data over socket");
            }
        }
    }

    //invio del messaggio "Finish" che determina la fine dell'invio dello stato di tutti gli altri connessi

    client_connesso = "Finish";
    msg_len = strlen(client_connesso);

    while((ret = send(arg->socket_desc_TCP_client, client_connesso, msg_len, 0))<0){
        if (errno == EINTR) continue;
        else if (errno == ENOTCONN) {
            printf("Client closed connection\n");
            client_connected* temp = client_connected[idx];
            client_connected[idx] = NULL;
            client_disconnected[idx] = temp;
        }
        ERROR_HELPER(-1, "Could not send data over socket");
    }





	//entrare nel loop di ricezione richiesta e comunicazione nuove connessioni / disconnessioni







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
  Image* surface_elevation = Image_load(elevation_filename);
  if (surface_elevation) {
    printf("Done! \n");
  } else {
    printf("Fail! \n");
  }


  printf("loading texture image from %s ... ", texture_filename);
  Image* surface_texture = Image_load(texture_filename);
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

  //creo il thread che si occuperà di ricevere via UDP le forze e che invierà le nuove poiszioni dopo aver integrato il mondo

  pthread_t thread;

  thread_server_UDP_args* args_UDP=(thread_server_UDP_args*)malloc(sizeof(thread_server_UDP_args));
  	 args_UDP->socket_desc_UDP_server=server_socket_UDP;
	 args_UDP->connected=online_client;
	 args_UDP->disconnected=offline_client;

  //creo il thread

  ret = pthread_create(&thread, NULL,thread_server_UDP,args_UDP);
  ERROR_HELPER(ret, "Could not create thread");

  // impongo di non aspettare terminazione in modo da non bloccare tutto il programma

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

	 client_socket=accept(server_socket_TCP,(struct sockaddr*) &client_addr,sizeof(struct sockadrr_in);
	 ERROR_HELPER(client_socket,"error accepting connections \n");

	 //lancio il thread che si occuperà di parlare poi con il singolo client che si è connesso

	 pthread_t thread;

	 thread_server_TCP_args* args_TCP=(thread_server_TCP_args*)malloc(sizeof(thread_server_TCP_args));
	 args_TCP->socket_desc_TCP_client = client_socket;
	 args_TCP->connected=online_client;
	 args_TCP->disconnected=offline_client;

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
