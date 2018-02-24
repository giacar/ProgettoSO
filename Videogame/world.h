#pragma once
#include <sys/time.h>
#include "image.h"
#include "surface.h"
#include "vehicle.h"
#include "linked_list.h"

typedef struct World {
  ListHead vehicles; // list of vehicles
  Surface ground;    // surface

  // stuff
  float dt;
  struct timeval last_update;
  float time_scale;
} World;

int World_init(World* w,
	       Image* surface_elevation,
	       Image* surface_texture,
	       float x_step, 
	       float y_step, 
	       float z_step);

void World_destroy(World* w);

void World_update(World* w);

Vehicle* World_getVehicle(World* w, int vehicle_id);

Vehicle* World_addVehicle(World* w, Vehicle* v);

Vehicle* World_detachVehicle(World* w, Vehicle* v);


