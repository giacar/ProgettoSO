#pragma once
#include "vec3.h"
#include "image.h"

struct Surface;
typedef void (*SurfaceDtor)(struct Surface* self);

//! 3d surface, represented as a matrix of 3d points with normals
typedef struct Surface{
  //! linear array of points
  Vec3 *points;

  //! linear array of normals
  Vec3 *normals;

  //! points as a matrix
  Vec3 **point_rows;

  //! normals as a matrix
  Vec3 **normal_rows;

  //! number of rows and of columns
  int rows, cols;
  
  //! step between adjacent points
  float row_scale, col_scale;

  //! number of points =(rows*cols)
  int n_points;

  int gl_list;
  int gl_texture;

  Image* texture;
  SurfaceDtor _destructor; 
} Surface;

//alloates a surface that holds a matrix of rows times cols points
// s should point to a struct Surface
void Surface_init(Surface* s,
		  int rows, int cols,
		  float row_scale, float col_scale);

//frees the memory of the objects in a surface
// s should point to a struct Surface
void Surface_destroy(Surface* s);

//!constructs a surface out of a grid representation of a surface z=f(rows,cols)
//!and computes the normals
//! @param s: a pointer to a valid surface
//! @param m: a matrix of  rows by cols z elements
//! @param x_scale: the step of each row
//! @param x_scale: the step of each col
//! @param z_scale: the value to multiply by m[r,c] to get the elevation of the point

void Surface_fromMatrix(Surface* s, 
			    float** m, int rows, int cols,
			    float row_scale, float col_scale, float z_scale);

int Surface_getTransform(float transform[16], Surface* s,
			  float x, float y, float z, float alpha, int inverse);

