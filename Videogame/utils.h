typedef struct thread_server_TCP_args{
    int socket_desc_TCP_client;
    struct ListHead *list;
}thread_server_TCP_args;

typedef struct thread_server_UDP_args{
    int socket_desc_UDP_server;
    struct ListHead *list;
}thread_server_UDP_args;

typedef struct user_table {
	char[64] username;
	char[64] password;
	int id;
}user_table;

typedef struct client_connected{
	int id;
	Image* texture;
	sockadrr_in* addr; //to send data over udp socket to the client
	int socket_TCP  //to send data over tcp to the client(la socket ricevuta dalla accept)
	struct client_connected* previous;
	struct client_connected* next;
}client_connected;

typedef struct movement_intentions{// lista di intenzioni di movimento che si accumula il server quando le riceve e che poi sbobina per aggiornare il mondo
	int id;
	float rotational_force;
	float translational_force;
	struct movement_intentions* previous;
	struct movement_intentions* next;
}movement_intentions;

typedef struct client_disconnected{ //lista dei client che si sono disconnessi, di cui ci salviamo Id e Texture
    int id;
    Image* texture;
    struct client_disconnected* previous;
    struct client_disconnected* next;
}client_disconnected;

//Quando un client si disconnette, lo rimuoviamo dalla lista di client_connected e lo aggiungiamo a quella di client_disconnected


