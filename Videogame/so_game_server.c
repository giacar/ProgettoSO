
// #include <GL/glut.h> // not needed here
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "image.h"
#include "surface.h"
#include "world.h"
#include "vehicle.h"
#include "world_viewer.h"


void* thread_server_TCP(void* thread_server_TCP_args){
	
	//implementare protoccollo login e aggiungere il nuovo client al mondo ed alla lista dei client connessi
	
	
	
	
	
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
  
  //TODO creare lista di tutti i client connessi che verranno man mano aggiunti e rimossi
  // deve contenere ID texture 
  
  int ret;
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
	 
  thread_server_UDP_args* args=(thread_server_UDP_args*)malloc(sizeof(thread_server_UDP_args));
   
  //creo il thread	 
  ret = pthread_create(&thread, NULL,thread_server_TCP,args);
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
	 
	 thread_server_TCP_args* args=(thread_server_TCP_args*)malloc(sizeof(thread_server_TCP_args));
	 
	 
	 ret = pthread_create(&thread, NULL,thread_server_TCP,args);
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
