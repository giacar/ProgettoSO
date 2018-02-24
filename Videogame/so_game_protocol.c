#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "so_game_protocol.h"

// converts a packet into a (preallocated) buffer
int Packet_serialize(char* dest, const PacketHeader* h){
  char* dest_end=dest;
  switch(h->type){
    case GetId:
    case GetTexture:
    case GetElevation:
    {
      const IdPacket* id_packet=(IdPacket*) h;
      memcpy(dest, id_packet, sizeof(IdPacket));
      dest_end+=sizeof(IdPacket);
      break;
    }
    case PostTexture:
    case PostElevation:
    {
      printf("cast\n");
      const ImagePacket* img_packet=(ImagePacket*) h;
      printf("memcopy\n");
      memcpy(dest,img_packet,sizeof(ImagePacket));
      // the image is invalid, we need to read it from the buffer
      printf("forward address\n");
      dest_end+=sizeof(ImagePacket);
      printf("image serialization");
      dest_end+=Image_serialize(img_packet->image, dest_end, 1024*1024);
      printf("end\n");
      break;
    }
    case WorldUpdate:
    {
      const WorldUpdatePacket* world_packet=(WorldUpdatePacket*) h;
      memcpy(dest, world_packet, sizeof(WorldUpdatePacket));
      dest_end+=sizeof(WorldUpdatePacket);
      memcpy(dest_end, world_packet->updates, world_packet->num_vehicles*sizeof(ClientUpdate));
      dest_end+=world_packet->num_vehicles*sizeof(ClientUpdate);
      break;
    }
    case VehicleUpdate:
    {
      VehicleUpdatePacket* vehicle_packet=(VehicleUpdatePacket*) h;
      memcpy(dest, vehicle_packet, sizeof(VehicleUpdatePacket));
      dest_end+=sizeof(VehicleUpdatePacket);
      break;
    }
  }

  PacketHeader* dest_header=(PacketHeader*)dest;
  dest_header->size=dest_end-dest;
  return dest_header->size;
}



//reads a packet from a preallocated buffer
PacketHeader* Packet_deserialize(const char* buffer, int size){
  const PacketHeader* h=(PacketHeader*) buffer;
  switch(h->type) {
    case GetId:
    case GetTexture:
    case GetElevation:
    {
      IdPacket* id_packet=(IdPacket*) malloc(sizeof(IdPacket));
      memcpy(id_packet, buffer, sizeof(IdPacket));
      return (PacketHeader*)id_packet;
    }
    case PostTexture:
    case PostElevation:
    {
      ImagePacket* img_packet=(ImagePacket*) malloc(sizeof(ImagePacket));
      memcpy(img_packet,buffer,sizeof(ImagePacket));
      // the image is invalid, we need to read it from the buffer
      size-=sizeof(ImagePacket);
      buffer+=sizeof(ImagePacket);
      img_packet->image=Image_deserialize(buffer, size);
      if (! img_packet->image){
        free(img_packet);
        return 0;
      }
      return (PacketHeader*)img_packet;
    }
    case WorldUpdate:
    {
      WorldUpdatePacket* world_packet=(WorldUpdatePacket*) malloc(sizeof(WorldUpdatePacket));
      memcpy(world_packet, buffer, sizeof(WorldUpdatePacket));
      // we get the number of clients
      world_packet->updates=(ClientUpdate*)malloc(world_packet->num_vehicles*sizeof(ClientUpdate));
      buffer+=sizeof(WorldUpdatePacket);
      memcpy(world_packet->updates, buffer, world_packet->num_vehicles*sizeof(ClientUpdate));
      return (PacketHeader*) world_packet;
    }
    case VehicleUpdate:
    { 
      VehicleUpdatePacket* vehicle_packet=(VehicleUpdatePacket*) malloc(sizeof(VehicleUpdatePacket));
      memcpy(vehicle_packet, buffer, sizeof(VehicleUpdatePacket));
      return(PacketHeader*) vehicle_packet;
    }
  }
  return 0;
}

void Packet_free(PacketHeader* h) {
  switch(h->type){
  case GetId:
  case GetTexture:
  case GetElevation:
  case VehicleUpdate:
  {
    free(h);
    return;
  }
  case WorldUpdate:
  {
    WorldUpdatePacket* world_packet=(WorldUpdatePacket*) h;
    if (world_packet->num_vehicles)
      free(world_packet->updates);
    free(world_packet);
    return;
  }
  case PostTexture:
  case PostElevation:
  {
    ImagePacket* img_packet=(ImagePacket*) h;
    if (img_packet->image){
      Image_free(img_packet->image);
    }
    free (img_packet);
  }
  }
}
