typedef struct thread_server_TCP_args{
    int socket_desc_TCP_client;
    //TODO lista di tutti i client connessi con relative texture
}thread_server_TCP_args;

typedef struct thread_server_UDP_args{
    int socket_desc_UDP_server;
    //TODO lista di tutti i client connessi con relative texture
}thread_server_UDP_args;



typedef struct client_connected{
	int id;
	Image* texture;
	sockadrr_in* addr; //to send data over udp socket to the client
	int socket_TCP  //to send data over tcp to the client(la socket ricevuta dalla accept)
	client_connected* previous;
	client_connected* next;
}client_connected;

typedef struct movement_intentions{// lista di intenzioni di movimento che si accumula il server quando le riceve e che poi sbobina per aggiornare il mondo
	int id;
	float rotational_force;
	float translational_force;
	movement_intentions* previous;
	movement_intentions* next;
}movement_intentions;


