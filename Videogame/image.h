#ifndef _IMAGE_H_
#define _IMAGE_H_

typedef enum PixelType {
  RGB8=0x0, 
  MONO8=0x1, 
  RGB16=0x2,
  MONO16=0x3, 
  FLOATMONO=0x4, 
  FLOATRGB=0x5
} PixelType;

typedef struct Image {
  int rows, cols, channels;
  unsigned char* data;
  unsigned char ** row_data;
  PixelType type;
} Image;

Image* Image_alloc(int rows, int cols, PixelType type);

void Image_free(Image* img);

Image* Image_convert(Image* src, PixelType type);

int Image_serialize(const Image* img, char* buffer, int size);

Image* Image_deserialize(const char* buffer, int size);

Image* Image_load(const char* filename);

int Image_save(Image* img, const char* filename);
  
  


#endif
