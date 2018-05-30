#pragma once
#include <netinet/in.h>
#include "surface.h"
#include "image.h"
#include "linked_list.h"
#include "vehicle.h"
#include "common.h"

typedef struct clients{
	int id;
	int status; //1:connected 0:disconnected -1 NON INIZIALIZZATO
	Image* texture;
	struct sockaddr_in* addr; //to send data over udp socket to the client
	int socket_TCP;  //to send data over tcp to the client(la socket ricevuta dalla accept)
}clients;

typedef struct thread_server_TCP_args{
    int socket_desc_TCP_client;
    Image* elevation_map;
    Image* map;
    clients* list;
}thread_server_TCP_args;

typedef struct thread_server_UDP_args{
    int socket_desc_UDP_server;
    struct sockaddr_in server_addr_UDP; //necessario per la comunicazione UDP
    clients* list;
}thread_server_UDP_args;

typedef struct thread_client_args{
    Vehicle v;  //veicolo del client
    int socket_desc_TCP;    //descrittore della socket con cui comunicare col server(TCP)
    int socket_desc_UDP;    //descritto socket per UDP 
    int id;     //id ricevuto dal server
    Image* map_texture; //texture della mappa che andrà aggiornato
    struct sockaddr_in server_addr_UDP;  //necessario per la comunicazione UDP

}thread_client_args;


typedef struct user_table {
	char username[64];
	char password[64];
	//id è la posizione nel vettore
}user_table;

typedef struct movement_intentions{// lista di intenzioni di movimento che si accumula il server quando le riceve e che poi sbobina per aggiornare il mondo
	int id;
	float rotational_force;
	float translational_force;
}movement_intentions;



//Quando un client si disconnette, lo rimuoviamo dalla lista di client_connected e lo aggiungiamo a quella di client_disconnected

// Funzione di ricezione TCP

int recv_TCP(int socket, char *buf, size_t len, int flags);

// Funzione di ricezione TCP per i pacchetti

int recv_TCP_packet(int socket, char* buf, int flags, int* bytes_read);

// Funzione di invio TCP

int send_TCP(int socket, const char *buf, size_t len, int flags);

// Funzione di ricezione UDP

int recv_UDP(int socket, char *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen);

// Funzione di ricezione TCP per i pacchetti

int recv_UDP_packet(int socket, char *buf, int flags, struct sockaddr *src_addr, socklen_t *addrlen, int* bytes_read);

// Funzione di invio UDP

int send_UDP(int socket, const char *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen);

// Funzione per eliminazione semafori
int sem_clean(sem_t sem_utenti, sem_t sem_thread_UDP, sem_t sem_online);

// Funzione per chiusura socket
int close_sockets_server(int server_socket_TCP, int server_socket_UDP);
