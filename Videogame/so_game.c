#include <math.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "image.h"
#include "surface.h"
#include "world.h"
#include "vehicle.h"
#include "world_viewer.h"
#include <pthread.h>

World world;
Vehicle* vehicle; // The vehicle

typedef struct {
  volatile int run;
  World* world;
} UpdaterArgs;

// this is the updater threas that takes care of refreshing the agent position
// in your client it is not needed, but you will
// have to copy the x,y,theta fields from the world update packet
// to the vehicles having the corrsponding id
void* updater_thread(void* args_){
  UpdaterArgs* args=(UpdaterArgs*)args_;
  while(args->run){
    World_update(args->world);
    usleep(30000);
  }
  return 0;
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

    // construct the world
  World_init(&world, surface_elevation, surface_texture,  0.5, 0.5, 0.5);

  // create a vehicle
  vehicle=(Vehicle*) malloc(sizeof(Vehicle));
  Vehicle_init(vehicle, &world, 0, vehicle_texture);

  // add it to the world
  World_addVehicle(&world, vehicle);

  pthread_t runner_thread;
  pthread_attr_t runner_attrs;
  UpdaterArgs runner_args={
    .run=1,
    .world=&world
  };
  
  pthread_attr_init(&runner_attrs);
  pthread_create(&runner_thread, &runner_attrs, updater_thread, &runner_args);
  WorldViewer_runGlobal(&world, vehicle, &argc, argv);
  runner_args.run=0;
  void* retval;
  pthread_join(runner_thread, &retval);
  
  // check out the images not needed anymore
  Image_free(vehicle_texture);
  Image_free(surface_texture);
  Image_free(surface_elevation);

  // cleanup
  World_destroy(&world);
  return 0;             
}
