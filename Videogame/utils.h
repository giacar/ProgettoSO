typedef struct thread_server_TCP_args{
    int socket_desc_TCP_client;
    client_connected* connected;
    client_disconnected* disconnected;
}thread_server_TCP_args;

typedef struct thread_server_UDP_args{
    int socket_desc_UDP_server;
    client_connected* connected;
    client_disconnected* disconnected;
}thread_server_UDP_args;

typedef struct user_table {
	char[64] username;
	char[64] password;
	//id è la posizione nel vettore
}user_table;

typedef struct client_connected{
	int id;
	Image* texture;
	sockadrr_in* addr; //to send data over udp socket to the client
	int socket_TCP  //to send data over tcp to the client(la socket ricevuta dalla accept)
}client_connected;

typedef struct movement_intentions{// lista di intenzioni di movimento che si accumula il server quando le riceve e che poi sbobina per aggiornare il mondo
	int id;
	float rotational_force;
	float translational_force;
}movement_intentions;

typedef struct client_disconnected{ //lista dei client che si sono disconnessi, di cui ci salviamo Id e Texture
    int id;
    Image* texture;
}client_disconnected;

//Quando un client si disconnette, lo rimuoviamo dalla lista di client_connected e lo aggiungiamo a quella di client_disconnected

// Funzione di ricezione TCP

int recv_TCP(int socket, void *buf, size_t len, int flags);

// Funzione di invio TCP

int send_TCP(int socket, const void *buf, size_t len, int flags);

// Funzione di ricezione UDP

int recv_UDP(int socket, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen);

// Funzione di invio UDP

int send_UDP(int socket, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen);