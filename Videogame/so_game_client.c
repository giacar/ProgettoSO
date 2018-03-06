
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

int window;
WorldViewer viewer;
World world;
Vehicle* vehicle; // The vehicle

void* thread_listener(void* arg){}//todo


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

  int ret;

  Image* my_texture_for_server;
  // todo: connect to the server
  //   -get ad id
  //   -send your texture to the server (so that all can see you)
  //   -get an elevation map
  //   -get the texture of the surface

  //variables for handling a socket
  int socket_desc;
  struct sockaddr_in server_addr{0};    //some fields are required to be filled with 0

  //creating a socket
  socket_desc = socket(AF_INET, SOCK_STREAM, 0);
  if (socket_desc < 0) printf("Could not create socket");

  //set up parameters for connection
  server_addr.sin_addr.in_addr = inet_addr(SERVER_ADDRESS);
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = SERVER_PORT;

  //initiate a connection to the socket
  ret = connect(socket_desc, (struct sockaddr*) &server_addr, sizeof(struct sockaddr_in));
  if (ret < 0) printf("Could not connect to socket");

  char idPacket[1024];
  size_t idPacket_len = sizeof(idPacket);   //lunghezza del mex id del client
  size_t msg_len;                           //lunghezza del messaggio letto

  while ((ret = recv(socket_desc, idPacket, msg_len, 0)) < 0){
    if (errno == EINTR) continue;
    printf("Could not read id from socket");
  }

  idPacket[msg_len] = '\0';

  IdPacket id = Packet_deserialize(idPacket, idPacket_len);

  char elevation_map[1024];
  size_t elevation_map_len = sizeof(elevation_map);

  while ((ret = recv(socket_desc, elevation_map, msg_len, 0)) < 0){
      if (errno == EINTR) continue;
      printf("Could not read elevation map from socket");
  }

  elevation_map[msg_len] = '\0';

  char texture_map[1024];
  size_t texture_map_len = sizeof(texture_map);

  while ((ret = recv(socket_desc, texture_map, msg_len, 0)) < 0){
      if (errno == EINTR) continue;
      printf("Could not read map texture from socket");
  }

  texture_map[msg_len] = '\0';

  char my_texture[1024];
  size_t my_texture_len = sizeof(my_texture);

  while ((ret = recv(socket_desc, my_texture, msg_len, 0)) < 0){
      if (errno == EINTR) continue;
      printf("Could not read my texture from socket");
  }

  my_texture[msg_len] = '\0';

  // these come from the server
  int my_id = id->id;
  Image* map_elevation = Packet_deserialize(elevation_map, elevation_map_len);
  Image* map_texture = Packet_deserialize(texture_map, texture_map_len);
  Image* my_texture_from_server = Packet_deserialize(my_texture, my_texture_len);

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

  pthread_t thread;
  ret = pthread_create(&thread, NULL, thread_listener, NULL);
  if (ret < 0) printf("Could not create thread");

  ret = pthread_join(&thread, NULL);
  if (ret < 0) printf("Could not join thread");

  WorldViewer_runGlobal(&world, vehicle, &argc, argv);

  // cleanup
  World_destroy(&world);
  return 0;
}
