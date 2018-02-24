#include "image.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

static const int MAX_SIZE=1024*1024;
  
void Image_free(Image* img) {
  free(img->data);
  free(img->row_data);
  free(img);
}

Image* Image_alloc(int rows, int cols, PixelType type){
  int bpp=1;
  int channels = 1;
  switch(type) {
  case MONO8: 
    bpp=1; 
    channels = 1; 
    break;
  case MONO16: 
    bpp=2; 
    channels = 1; 
    break;
  case RGB8: 
    bpp=3; 
    channels = 3; 
    break;
  case RGB16: 
    bpp=6; 
    channels = 3; 
    break;
  case FLOATMONO: 
    bpp=sizeof(float); 
    channels = 1; 
    break;
  case FLOATRGB: 
    bpp=sizeof(float)*3; 
    channels = 3; 
    break;
  }

  Image* img = malloc(sizeof(Image));
  img->rows = rows;
  img->cols = cols;					
  img->channels = channels;
  img->data = (unsigned char*) malloc(rows*cols*bpp);
  img->row_data = (unsigned char**) malloc(sizeof(unsigned char*)*rows);
  unsigned char* base_data = img->data;
  for (size_t i=0; i<rows; i++){
    img->row_data[i]=base_data;
    base_data += cols*bpp;
  }
  return img;
}


int getLine(char* dest, const char* src, size_t src_size) {
  int k=0;
  char* dest_end=dest;
  while(k<src_size && (*src!='\n' || *src==EOF || *src==0)){
    *dest_end=*src;
    ++dest_end;
    ++src;
    ++k;
  }
  ++k;
  *dest_end='\n';
  return k;
}

int Image_serialize(const Image* img, char* buffer, int size){
  char* magic_number=0;
  int bpp;
  int maxval=0;
  switch(img->type){
  case MONO8:
    magic_number="P5";
    bpp=1;
    maxval=255;
    break;
  case MONO16:
    magic_number="P5";
    bpp=2;
    maxval=65535;
    break;
  case RGB8:
    magic_number="P6";
    bpp=3;
    maxval=255;
    break;
  case RGB16:
    magic_number="P6";
    bpp=6;
    maxval=65535;
    break;
  default:
    return 0;
  }
  char* buffer_end=buffer;
  buffer_end+=sprintf(buffer_end, "%s\n",magic_number);
  buffer_end+=sprintf(buffer_end, "%d %d\n",img->rows, img->cols);
  buffer_end+=sprintf(buffer_end, "%d\n",maxval);
  int remaining_size=size-(buffer_end-buffer);
  int bytes_to_write=bpp*img->rows*img->cols;
  if (bytes_to_write>remaining_size)
    return 0;
  memcpy(buffer_end,img->data,bytes_to_write);
  buffer_end+=bytes_to_write;
  return (buffer_end-buffer);
}

Image* Image_deserialize(const char* buffer, int size) {
  char magic_number[100];
  int rows, cols;
  int bpp=1;
  char  line [1024];
  int char_read=getLine(line, buffer, size);
  if (!char_read)
    return 0;
  buffer+=char_read;
  size-=char_read;
  
  sscanf(line, "%s", magic_number);
  do{
    char_read=getLine(line, buffer, size);
    if (!char_read)
      return 0;
    buffer+=char_read;
    size-=char_read;
    printf("read");
  } while(line[0]=='#');

  sscanf(line, "%d %d\n", &cols, &rows);
  printf("rows:%d, cols: %d\n", rows, cols);
  printf("magic number: [%s]\n", magic_number);

  int maxval;
  char_read=getLine(line, buffer, size);
  if (!char_read)
    return 0;
  buffer+=char_read;
  size-=char_read;
  sscanf(line, "%d\n", &maxval);
  
  PixelType type=MONO8;
  if (! strcmp("P5", magic_number)) {
    if (maxval>255) {
      bpp=2;
      type = MONO16;
    }
  } else if (! strcmp("P6", magic_number)){
    if (maxval>255) {
      bpp=6;
      type = RGB16;
    } else {
      type = RGB8;
      bpp=3;
    }
  } else
    return 0;
  int bytes_to_read=bpp*rows*cols;
  if (bytes_to_read<size)
    return 0;
  Image* img = Image_alloc(rows, cols, type);
  memcpy(img->data,buffer,bytes_to_read);
  img->type=type;
  return img;
}


Image* Image_load(const char* filename) {
  char buffer[MAX_SIZE];
  int fd=open(filename, O_RDONLY);
  if (fd<0)
    return 0;
  int size=read(fd, buffer, MAX_SIZE);
  close(fd);
  return Image_deserialize(buffer, size);
}


int Image_save(Image* img, const char* filename) {
  char buffer[MAX_SIZE];
  int fd=open(filename, O_RDWR|O_CREAT, 0666);
  if (fd<0){
    printf("save error, cant open file\n");
    return 0;
  }
  int size=Image_serialize(img, buffer, MAX_SIZE);
  write(fd, buffer, size);
  close(fd);
  return 1;
}


Image* Image_convert(Image* src, PixelType type){
  
  float scale;
  if (src->type == MONO8 && type == FLOATMONO) {
    scale=1./255;
  } else if (src->type == MONO16 && type == FLOATMONO){
    scale=1./6000;
  } else if (src->type == RGB8 && type == FLOATRGB){
    scale=1./255;
  } else if (src->type == RGB16 && type == FLOATRGB){
    scale=1./65535;
  } else return 0;
  
  Image* img=Image_alloc(src->rows, src->cols, type);
  
  for (int r=0; r<src->rows; r++){
    float* destptr=(float*) img->row_data[r];
    unsigned char* srcptr = src->row_data[r];
    for (int c=0; c<src->cols; c++){
      for (int i=0; i<src->channels; i++){
	if (src->type==MONO16 || src->type==RGB16){
	  unsigned int c_up = *srcptr;
	  srcptr++;
	  unsigned int  c_low = *srcptr;
	  srcptr++;
	  int v=(c_up<<8) + c_low;
	  *destptr=scale * v;
	  destptr++;
	} else {
	  *destptr=scale * (*srcptr);
	  srcptr++;
	  destptr++;
	}
      }
    }
  }
  return img;
}

