#pragma once
#include "world.h"
#include "vehicle.h"

// call this to start the visualization of the stuff.
// This will block the program, and terminate when pressing esc on the viewport
void WorldViewer_runGlobal(World* world,
			   Vehicle* self,
			   int* argc, char** argv);

