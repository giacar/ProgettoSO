
#include <GL/glut.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

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

void* thread_listener(void* client_args){   //todo

    /**COMUNICAZIONE UDP**/
    /**

    Client, via UDP invia dei pacchetti VehicleUpdate al server. Il contenuto del VehicleUpdate viene prelevato da desired_force,
    è contenuto nella struttura del veicolo e si mette in attesa di pacchetti WorldUpdate che contengono al
    loro interno una lista collegata. Ricevuti questi pacchetti, smonta la lista collegata all'interno e per ogni elemento della lista,
    preso l'id, preleva dal mondo il veicolo con quell'id e ne aggiorna lo stato. I pacchetti di veicoli ancora non aggiunti al proprio mondo
    vengono ignorati (per necessità).

    **/


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

    VehicleUpdatePacket* update = (VehicleUpdatePacket*) malloc(sizeof(VehicleUpdatePacket));
    update->translational_force = v->translational_force_update;
    update->rotational_force = v->rotational_force_update;
    update->header->size = sizeof(VehicleUpdatePacket);
    update->header->type = VehicleUpdate;

    char[1024] vehicle_update;
    int vehicle_update_len = serialize(&vehicle_update, update);

    while((ret = sendto(socket_UDP, vehicle_update, vehicle_update_len, 0, (struct sockaddr*) server_UDP, sizeof(server_UDP)) < 0)){
        if (errno == EINTR) continue;
        ERROR_HELPER(-1, "Could not send vehicle updates to server");
    }

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

  /**LOGIN**/
  /** Client inserisce username e password appena si connette:
   *    -se utente non esiste allora i dati che ha inserito vengono usati per registrare l'utente
   * [TODO]
   **/

  /**   -se utente esiste ma la password non è corretta, si apre un while nel quale l'utente può inserire la password corretta
   * [TODO]
   **/

  /**   -se utente esiste e la password è corretta, allora invia al server un richiesta di tutti i dati salvati nella precedente sessione e il server
   *    risponde con tali dati
   * [TODO]
   **/

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


  //requesting and receving the ID
  IdPacket* request_id=(IdPacket*)malloc(sizeof(IdPacket));
  request_id->header->type=GetId;
  request_id->header->size=sizeof(IdPacket);
  request_id->id=-1;

  char idPacket_request[1024];
  char idPacket[1024];
  size_t idPacket_request_len = Packet_serialize(&idPacket_request,request_id);
  size_t msg_len;


    while ((ret = send(socket_desc, idPacket_request, idPacket_request_len, 0)) < 0){
    if (errno == EINTR) continue;
    ERROR_HELPER(-1, "Could not send id request  to socket");
  }

  while ((ret = recv(socket_desc, idPacket, msg_len, 0)) < 0){
    if (errno == EINTR) continue;
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

  size_t texture_for_server_len = Packet_serialize(&texture_for_server, my_texture);


  while ((ret = send(socket_desc, texture_for_server, texture_for_server_len, 0) < 0){
      if (errno == EINTR) continue;
      ERROR_HELPER(-1, "Could not send my texture for server");
  }
  // receving my texture from server

  char my_texture_from_server[1024];

    while ((ret = recv(socket_desc, my_texture_from_server, msg_len, 0)) < 0){
      if (errno == EINTR) continue;
      ERROR_HELPER(-1, "Could not read my texture from socket");
  }

  my_texture_from_server[msg_len] = '\0';

  ImagePacket* my_texture_received=Packet_deserialize(&my_texture_from_server,msg_len);
  if(my_texture_received!=my_texture) ERROR_HELPER(-1,"error in communication \n");


  //requesting and receving elevation map
  PacketHeader* request_elevation=(PacketHeader*)malloc(sizeof(PacketHeader));
  request_elevation->size=sizeof(PacketHeader);
  request_elevation->type=GetElevation;

  char request_elevation_for_server[1024];
  char elevation_map[1024];
  size_t request_elevation_len =Packet_serialize(&request_elevation_for_server,request_elevation);

  while ((ret = send(socket_desc, request_elevation_for_server, request_elevation_for_server_len, 0) < 0){
      if (errno == EINTR) continue;
      ERROR_HELPER(-1, "Could not send my texture for server");
  }

  while ((ret = recv(socket_desc, elevation_map, msg_len, 0)) < 0){
      if (errno == EINTR) continue;
      ERROR_HELPER(-1, "Could not read elevation map from socket");
  }

  elevation_map[msg_len] = '\0';
  ImagePacket* elevation= Packet_deserialize(&elevation_map,msg_len);
  if(elevation->header->type!=PostElevation && elevation->id!=0) ERROR_HELPER(-1,"error in communication \n");


  //requesting and receving map
  char request_texture_map_for_server;
  char texture_map[1024];
  PacketHeader* request_map=(PacketHeader*)malloc(sizeof(PacketHeader));
  request_map->type=GetTexture;
  request_map->size=sizeof(PacketHeader);
  size_t request_texture_map_for_server_len=Packet_serialize(&request_texture_map_for_server,request_map);

    while ((ret = send(socket_desc, request_texture_map_for_server, request_texture_map_for_server_len, 0) < 0){
      if (errno == EINTR) continue;
      ERROR_HELPER(-1, "Could not send my texture for server");
  }

  while ((ret = recv(socket_desc, texture_map, msg_len, 0)) < 0){
      if (errno == EINTR) continue;
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
  pthread_t thread;
  ret = pthread_create(&thread, NULL, thread_listener,args);
  ERROR_HELPER(ret, "Could not create thread");

  ret = pthread_detach(&thread);
  ERROR_HELPER(ret, "Could not detach thread");

  WorldViewer_runGlobal(&world, vehicle, &argc, argv);

  // cleanup
  World_destroy(&world);
  return 0;
}
