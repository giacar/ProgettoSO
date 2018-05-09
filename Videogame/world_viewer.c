#include <GL/gl.h>
#include <GL/glut.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>
#include "world_viewer.h"
#include <unistd.h>
#include "image.h"
#include "surface.h"

int window;

WorldViewer viewer;



void keyPressed(unsigned char key, int x, int y)
{
  switch(key){
  case 27:
    glutDestroyWindow(window);
    exit(0);
  case ' ':
    viewer.self->translational_force_update = 0;
    viewer.self->rotational_force_update = 0;
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
    viewer.self->translational_force_update += 0.1;
    break;
  case GLUT_KEY_DOWN:
    viewer.self->translational_force_update -= 0.1;
    break;
  case GLUT_KEY_LEFT:
    viewer.self->rotational_force_update += 0.1;
    break;
  case GLUT_KEY_RIGHT:
    viewer.self->rotational_force_update -= 0.1;
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
  World_update(viewer.world);
  usleep(30000);
  glutPostRedisplay();

  // decay the commands
  viewer.self->translational_force_update *= 0.999;
  viewer.self->rotational_force_update *= 0.7;
}

void Surface_destructor(Surface* s){
  if (s->gl_list>-1)
    glDeleteLists(s->gl_list, 1);
  s->gl_list=-1;
  if (s->gl_texture>-1)
    glDeleteTextures(1, (unsigned int*)&s->gl_texture);
}

void Vehicle_destructor(Vehicle* v){
  if (v->gl_list>-1)
    glDeleteLists(v->gl_list, 1);
  v->gl_list=-1;
  if (v->gl_texture>-1)
    glDeleteTextures(1, (unsigned int*)&v->gl_texture);
}

void drawBox(float l, float w, float h)
{
  GLfloat sx = l*0.5f;
  GLfloat sy = w*0.5f;
  GLfloat sz = h*0.5f;

  glBegin(GL_QUADS);
  // bottom
  glNormal3f( 0.0f, 0.0f,-1.0f);
  glVertex3f(-sx, -sy, -sz);
  glVertex3f(-sx, sy, -sz);
  glVertex3f(sx, sy, -sz);
  glVertex3f(sx, -sy, -sz);

  // top
  glNormal3f( 0.0f, 0.0f,1.0f);
  glTexCoord2f(0,0);
  glVertex3f(-sx, -sy, sz);
  glTexCoord2f(0,1);
  glVertex3f(-sx, sy, sz);
  glTexCoord2f(1,1);
  glVertex3f(sx, sy, sz);
  glTexCoord2f(1,0);
  glVertex3f(sx, -sy, sz);
  // back
  glNormal3f(-1.0f, 0.0f, 0.0f);
  glVertex3f(-sx, -sy, -sz);
  glVertex3f(-sx, sy, -sz);
  glVertex3f(-sx, sy, sz);
  glVertex3f(-sx, -sy, sz);
  // front
  glNormal3f( 1.0f, 0.0f, 0.0f);
  glVertex3f(sx, -sy, -sz);
  glVertex3f(sx, sy, -sz);
  glVertex3f(sx, sy, sz);
  glVertex3f(sx, -sy, sz);
  // left
  glNormal3f( 0.0f, -1.0f, 0.0f);
  glVertex3f(-sx, -sy, -sz);
  glVertex3f(sx, -sy, -sz);
  glVertex3f(sx, -sy, sz);
  glVertex3f(-sx, -sy, sz);
  //right
  glNormal3f( 0.0f, 1.0f, 0.0f);
  glVertex3f(-sx, sy, -sz);
  glVertex3f(sx, sy, -sz);
  glVertex3f(sx, sy, sz);
  glVertex3f(-sx, sy, sz);
  glEnd();
}

int Image_toTexture(Image* src) {
  if (src->type!=RGB8)
    return -1;
   unsigned int surface_texture;
   printf("loading texture in system\n");
   glGenTextures(1, &surface_texture);
   glBindTexture(GL_TEXTURE_2D, surface_texture);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, src->rows, src->cols, 0,
		GL_RGB, GL_UNSIGNED_BYTE, src->data);
   return surface_texture;
}

void Surface_applyTexture(Surface* s, Image* img) {
  s->texture=img;
  printf("applying texture %p to surface %p\n", img, s);
  s->_destructor=Surface_destructor;
  if (s->gl_list>-1)
    glDeleteLists(s->gl_list, 1);
  s->gl_list = -1;
  if (s->gl_texture>-1)
    glDeleteTextures(1, (unsigned int*) &s->gl_texture);
  s->gl_texture = -1;
  if (img)
    s->gl_texture=Image_toTexture(img);
}

void Surface_draw(Surface* s) {
  if (s->gl_list > -1) {
    glCallList(s->gl_list);
    return;
  }

  s->gl_list = glGenLists(1);
  glNewList(s->gl_list, GL_COMPILE_AND_EXECUTE);
  if (s->gl_texture>-1){
    glEnable(GL_TEXTURE_2D);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glBindTexture(GL_TEXTURE_2D, s->gl_texture);
  }

  int r, c;
  for (r=0; r<s->rows-1; r++) {
    Vec3* normals_row_ptr = s->normal_rows[r];
    Vec3* normals_row_next = s->normal_rows[r+1];
    Vec3* points_row_ptr = s->point_rows[r];
    Vec3* points_row_next = s->point_rows[r+1];
    glBegin(GL_TRIANGLE_STRIP);
    for (c=0; c<s->cols-1; c++){
      glNormal3fv(normals_row_ptr->values);
      glTexCoord2f((float) c/s->cols, (float)r/s->rows);
      glVertex3fv(points_row_ptr->values);

      glNormal3fv(normals_row_next->values);
      glTexCoord2f( (float) c/s->cols, (float)(r+1)/s->rows);
      glVertex3fv(points_row_next->values);

      normals_row_ptr++;
      points_row_ptr++;
      normals_row_next++;
      points_row_next++;
      glNormal3fv(normals_row_ptr->values);
      glTexCoord2f((float) (c+1)/s->cols, (float)r/s->rows);
      glVertex3fv(points_row_ptr->values);
      glNormal3fv(normals_row_next->values);
      glTexCoord2f((float) (c+1)/s->cols, (float)(r+1)/s->rows);
      glVertex3fv(points_row_next->values);
    }
    glEnd();
  }

  if (s->gl_texture>-1){
    glDisable(GL_TEXTURE_2D);
  }
  glEndList();

}


void Vehicle_applyTexture(Vehicle* v) {
  printf("applying texture %p to vehicle %p\n", v->texture, v);
  v->_destructor=Vehicle_destructor;
  if (v->gl_list>-1)
    glDeleteLists(v->gl_list, 1);
  v->gl_list = -1;
  if (v->gl_texture>-1)
    glDeleteTextures(1, (unsigned int*) &v->gl_texture);
  v->gl_texture = -1;
  if (v->texture)
    v->gl_texture=Image_toTexture(v->texture);
}

void Vehicle_draw(Vehicle* v){
  if (v->gl_list<0){
    if (v->texture) {
      Vehicle_applyTexture(v);
    }
    v->gl_list = glGenLists(1);
    glNewList(v->gl_list, GL_COMPILE);
    if (v->gl_texture>-1){
      glEnable(GL_TEXTURE_2D);
      glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
      glBindTexture(GL_TEXTURE_2D, v->gl_texture);
    }
    drawBox(0.5,0.5,0.25);
    if (v->gl_texture>-1)
      glDisable(GL_TEXTURE_2D);
    glEndList();
  }

  glPushMatrix();
  glMultMatrixf(v->camera_to_world);
  glTranslatef(0,0,0.3);
  glCallList(v->gl_list);
  glPopMatrix();
}


void WorldViewer_init(WorldViewer* viewer,
		      World* w,
		      Vehicle* self){
  viewer->world=w;
  viewer->camera_z = 1;
  viewer->zoom = 1;
  viewer->view_type=Inside;
  viewer->window_width = viewer->window_height = 1;
  viewer->self=0;
  // initializes the GL context

  GLfloat light_diffuse[] = {1.0, 1.0, 1.0, 0.0};  // White diffuse light.
  GLfloat light_position[] = {100.0, 100.0, 100.0, 0.0};  //Infinite light location
  glEnable(GL_SMOOTH);
  glShadeModel(GL_SMOOTH);
  glPolygonMode( GL_FRONT, GL_FILL );
  glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
  glLightfv(GL_LIGHT0, GL_POSITION, light_position);
  glEnable(GL_LIGHT0);
  glEnable(GL_LIGHTING);
  glEnable(GL_DEPTH_TEST);
  Surface_applyTexture(&viewer->world->ground, viewer->world->ground.texture);
  ListItem* item=viewer->world->vehicles.first;
  while(item){
    Vehicle* v=(Vehicle*)item;
    Vehicle_applyTexture(v);
    if (v==self){
      viewer->self=self;
    }
    item=item->next;
  }
  assert("not self in list of vehicles, aborting" && viewer->self);
}

void WorldViewer_draw(WorldViewer* viewer){
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective( /* field of view in degree */ 70.0,
		  /* aspect ratio */ (float)viewer->window_width/(float)viewer->window_height,
		  /* Z near */ 0.5, /* Z far */ 100.0);

  float camera_distance = 1./viewer->zoom;
  Surface* ground=&viewer->world->ground;
  switch (viewer->view_type) {
  case Inside:
    gluLookAt(-camera_distance,0,viewer->camera_z*camera_distance,
	      0,0,0,
	      0,0,1);
    glRotatef(-viewer->self->theta*180.0/M_PI, 0, 0, 1);
    glTranslatef(-viewer->self->x, -viewer->self->y, -viewer->self->z);
    break;
  case Outside:
    gluLookAt(-camera_distance,0,viewer->camera_z*camera_distance,
	      0,0,0,
	      0,0,1);
    glMultMatrixf(viewer->self->world_to_camera);
    break;
  case Global:
    glLoadIdentity();
    gluLookAt(.5*ground->rows*ground->row_scale,
	      .5*ground->cols*ground->col_scale,
	      viewer->camera_z*10,
	      .5*ground->rows*ground->row_scale,
	      .5*ground->cols*ground->col_scale,
	      0,
	      1,0,0);
    break;
  }
  glMatrixMode(GL_MODELVIEW);
  Surface_draw(&viewer->world->ground);
  ListItem* item=viewer->world->vehicles.first;
  while(item){
    Vehicle* v=(Vehicle*)item;
    Vehicle_draw(v);
    item=item->next;
  }
  glutSwapBuffers();
}

void WorldViewer_destroy(WorldViewer* viewer){
}

void WorldViewer_reshapeViewport(WorldViewer* viewer, int width, int height){
  viewer->window_width  = width;
  viewer->window_height = height;
  glViewport(0, 0, width, height);
}


void WorldViewer_run(WorldViewer* viewer,
		     World* world,
		     Vehicle* self,
		     int* argc_ptr, char** argv){



}

void WorldViewer_runGlobal(World* world,
			   Vehicle* self,
			   int* argc_ptr, char** argv) {
  // initialize GL
  glutInit(argc_ptr, argv);
  glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
  glutCreateWindow("main");

  // set the callbacks
  glutDisplayFunc(display);
  glutIdleFunc(idle);
  glutSpecialFunc(specialInput);
  glutKeyboardFunc(keyPressed);
  glutReshapeFunc(reshape);
  WorldViewer_init(&viewer, world, self);

  // run the main GL loop
  glutMainLoop();

}
