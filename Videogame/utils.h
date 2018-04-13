typedef struct thread_server_TCP_args{
    int socket_desc_TCP_client;
    Image* elevation_map;
    Image* map;
    clients** list;
}thread_server_TCP_args;

typedef struct thread_server_UDP_args{
    int socket_desc_UDP_server_W;
    int socket_desc_UDP_server_M;
    clients** list;
}thread_server_UDP_args;

typedef struct thread_client_args{
    Vehicle v;  //veicolo del client
    int socket_desc_TCP;    //descrittore della socket con cui comunicare col server(TCP)
    int socket_desc_UDP_M;    //descritto socket per UDP per inviare intenzioni movimento
    int socket_desc_UDP_W;    //descritto socket per UDP per ricevere aggiornamenti mondo
    int id;     //id ricevuto dal server
    Image* map_texture; //texture della mappa che andrà aggiornato
    struct sockaddr_in server_addr_UDP_M;  //necessario per la comunicazione UDP_M
    struct sockaddr_in server_addr_UDP_W;  //necessario per la comunicazione UDP_W

}thread_client_args;


typedef struct user_table {
	char[64] username;
	char[64] password;
	//id è la posizione nel vettore
}user_table;

typedef struct clients{
	int id;
	int status //1:connected 0:disconnected
	Image* texture;
	sockadrr_in* addr; //to send data over udp socket to the client
	int socket_TCP  //to send data over tcp to the client(la socket ricevuta dalla accept)
}clients;

typedef struct movement_intentions{// lista di intenzioni di movimento che si accumula il server quando le riceve e che poi sbobina per aggiornare il mondo
	int id;
	float rotational_force;
	float translational_force;
}movement_intentions;



//Quando un client si disconnette, lo rimuoviamo dalla lista di client_connected e lo aggiungiamo a quella di client_disconnected

// Funzione di ricezione TCP

int recv_TCP(int socket, void *buf, size_t len, int flags);

// Funzione di invio TCP

int send_TCP(int socket, const void *buf, size_t len, int flags);

// Funzione di ricezione UDP

int recv_UDP(int socket, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen);

// Funzione di invio UDP

int send_UDP(int socket, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen);

// Funzione per eliminazione semafori
int sem_clean(sem_t sem_utenti, sem_t sem_thread_UDP);
