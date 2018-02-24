
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

int window;
WorldViewer viewer;
World world;
Vehicle* vehicle; // The vehicle

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
  
  Image* my_texture_for_server;
  // todo: connect to the server
  //   -get ad id
  //   -send your texture to the server (so that all can see you)
  //   -get an elevation map
  //   -get the texture of the surface

  // these come from the server
  int my_id;
  Image* map_elevation;
  Image* map_texture;
  Image* my_texture_from_server;

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

  WorldViewer_runGlobal(&world, vehicle, &argc, argv);

  // cleanup
  World_destroy(&world);
  return 0;             
}
