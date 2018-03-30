#pragma once
#include "vehicle.h"

//ia brief desription required
typedef enum {
  GetId=0x1,
  GetTexture=0x2,     //which packet uses this??
  GetElevation=0x3,   //which packet uses this??
  PostTexture=0x4,
  PostElevation=0x5,
  WorldUpdate=0x6,
  VehicleUpdate=0x7
} Type;

typedef struct {
  Type type;
  int size;
} PacketHeader;

// sent from client to server to notify its intentions
typedef struct {
  PacketHeader header;
  float translational_force;
  float rotational_force;
} VehicleForces;

// sent from client to server to ask for an id (id=-1)
// sent from server to client to assign an id
typedef struct {
  PacketHeader header;
  int id;
} IdPacket;

// sent from client to server (with type=PostTexture),
// to send its texture
// sent from server to client
//       (with type=PostTexture and id=0) to assign the surface texture
//       (with type=PostElevation and id=0) to assign the surface texture
//       (with type=PostTexture and id>0) to assign the  texture to vehicle id
typedef struct {
  PacketHeader header;
  int id;
  Image* image;
} ImagePacket;

// sent from client to server, in udp to notify the updates
typedef struct {
  PacketHeader header;
  int id;
  float rotational_force;
  float translational_force;
} VehicleUpdatePacket;

// block of the client updates, id of vehicle
// x,y,theta (read from vehicle id) are position of vehicle
// id is the id of the vehicle
typedef struct {
  int id;
  float x;
  float y;
  float theta;
} ClientUpdate;

typedef struct {
  int quit;
  char[17] quit_command;
  int socket_desc_TCP;
}QuitPacket;

// server world update, send by server (UDP)
typedef struct {
  PacketHeader header;
  int num_vehicles;
  ClientUpdate* updates;
} WorldUpdatePacket;

typedef struct thread_client_args{
    Vehicle v;  //veicolo del client
    int socket_desc_TCP;    //descrittore della socket con cui comunicare col server(TCP)
    int socket_desc_UDP;    //descritto socket per UDP
    int id;     //id ricevuto dal server
    Image* map_texture; //texture della mappa che andrà aggiornato
    struct sockaddr_in server_addr_UDP;  //necessario per la comunicazione UDP

}thread_client_args;

typedef struct NodoStringa{
    char[1024] current;
    NodoStringa* next;
} NodoStringa;


// converts a well formed packet into a string in dest.
// returns the written bytes
// h is the packet to write
int Packet_serialize(char* dest, const PacketHeader* h);

// returns a newly allocated packet read from the buffer
PacketHeader* Packet_deserialize(const char* buffer, int size);

// deletes a packet, freeing memory
void Packet_free(PacketHeader* h);

