#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <semaphore.h>
#include <netinet/in.h>
#include <stdlib.h>

#include "utils.h"
#include "common.h"
#include "so_game_protocol.h"

int recv_TCP_packet(int socket, char* buf, int flags, int* bytes_read) {
	int ret, packet_len, bytes_letti = 0;

	while (bytes_letti < sizeof(PacketHeader)) {
		ret = recv(socket, buf+bytes_letti, sizeof(PacketHeader)-bytes_letti, flags);
		
		if (ret == -1 && errno == EINTR) continue;
		
		if (ret == -1 && (errno == ENOTCONN || errno == EPIPE)) {
			printf("Connection closed. ");
			return -2;
		}
		if (ret == 0) {
			printf("Connection closed. ");
			return 0;
		}

		bytes_letti += ret;
	}

	if(bytes_letti == -1) return 0;		// da usare in caso di interruzioni

	if (verbosity_level>=DebugTCP) printf("[RECV_TCP_PACKET] Bytes read (header) = %d\n", bytes_letti);

	PacketHeader *head = (PacketHeader*)buf;
	packet_len = head->size;

	if (verbosity_level>=DebugTCP) printf("[RECV_TCP_PACKET] Header size = %d\n",packet_len);

	if (verbosity_level>=DebugTCP) printf("[RECV_TCP_PACKET] Need to receive %d bytes more\n", packet_len-bytes_letti);

	while (bytes_letti < packet_len) {
		ret = recv(socket, buf+bytes_letti, packet_len-bytes_letti, flags);
		
		if (ret == -1 && errno == EINTR) continue;

		if (ret == -1 && (errno == ENOTCONN || errno == EPIPE)) {
			printf("Connection closed. ");
			return -2;
		}

		if (ret == 0) {
		printf("Connection closed. ");
		return 0;
		}

		bytes_letti += ret;
	}
    
    *bytes_read = bytes_letti;
    ret = bytes_letti;

	return ret;
}

int recv_TCP(int socket, char *buf, size_t len, int flags) {
	int ret, bytes_read = 0, finito = 0;

	// Ricezione conoscendo la dimensione (len > 1)

	if (len > 1) {

		while (bytes_read < len) {
			ret = recv(socket, buf+bytes_read, len-bytes_read, flags);

			if (ret == -1 && errno == EINTR) continue;

			if (ret == -1 && (errno == ENOTCONN || errno == EPIPE)) {
				printf("Connection closed. ");
				return -2;
			}

			else if (ret == 0) {
				printf("Connection error. ");
				return -2;
			}

			bytes_read += ret;
		}
		ret = bytes_read;

	}

	// Ricezione non conoscendo la dimensione (len == 1)

	else {

		while (!finito) {
			ret = recv(socket, buf+bytes_read, 1, flags);

			if (verbosity_level>=DebugTCP) printf("[RECV_TCP] Received byte: %d\n", (int)buf[bytes_read]);

			if (ret == -1 && errno == EINTR) continue;
			if (ret == -1 && (errno == ENOTCONN || errno == EPIPE)) {
				printf("Connection closed. ");
				return -2;
			}
			if (ret == 0) {
				printf("Connection error. ");
				return -2;
			}

			if (buf[bytes_read] == '\n' || buf[bytes_read] == '\0') {
				if (verbosity_level>=DebugTCP) printf("[RECV_TCP] End of the string\n");
				finito = 1;
			}

			bytes_read++;
		}
		ret = bytes_read;

	}

	return ret;
}

int send_TCP(int socket, const char *buf, size_t len, int flags) {
	int ret, bytes_sent = 0, finito = 0;

	while (!finito) {

		ret = send(socket, buf+bytes_sent, len-bytes_sent, flags);

		if (ret == -1 && errno == EINTR) continue;

		if (ret == -1 && (errno == ENOTCONN || errno == EPIPE)) {
			printf("Connection closed. ");
			return -2;
		}

		bytes_sent += ret;

		if (bytes_sent == len) finito = 1;

	}

	ret = bytes_sent;

	return ret;
}

int recv_UDP_packet(int socket, char *buf, int flags, struct sockaddr *src_addr, socklen_t *addrlen, int* bytes_read) {
	int ret = 1, bytes_letti= 0;
	
	while ((ret = recvfrom(socket, buf, DIM_BUFF, flags, src_addr, addrlen)) < 0) {
		
		if (ret == -1 && errno == EINTR) continue;

		if (ret == -1 && (errno == ENOTCONN || errno == EPIPE)) {
			printf("Connection closed.\n");
			return -2;
		}
	}

	bytes_letti += ret;

	if (verbosity_level>=DebugUDP) {
		PacketHeader* p = (PacketHeader *) buf; 
		printf("[RECV_UDP_PACKET] Packet size = %d <=> Bytes read = %d\n", p->size, bytes_letti);
	}

	*bytes_read = bytes_letti;
	ret = bytes_letti;

	return ret;
}

int send_UDP(int socket, const char *buf, size_t len, int flags, struct sockaddr_in* dest_addr, int addrlen) {
	int ret, bytes_sent = 0;

	while (1) {
		ret = sendto(socket, buf+bytes_sent, len-bytes_sent, flags, (const struct sockaddr*) dest_addr, (socklen_t) addrlen);

		if (ret == -1 && errno == EINTR) continue;
		if (ret == -1 && (errno == ENOTCONN || errno == EPIPE)) {
			printf("Connection closed. ");
			return -2;
		}

		bytes_sent += ret;

		if (bytes_sent == len) break;

	}

	ret = bytes_sent;

	return ret;
}

int sem_clean(sem_t sem_utenti, sem_t sem_thread_UDP, sem_t sem_online){
    int ret;

    ret = sem_destroy(&sem_utenti);
  	if (ret == -1){ printf("Could not destroy sem_utenti");
  				   	exit(EXIT_FAILURE);}

    ret = sem_destroy(&sem_thread_UDP);
    if (ret == -1){ printf("Could not destroy sem_thread_UDP");
    				exit(EXIT_FAILURE);}

    ret = sem_destroy(&sem_online);
    if (ret == -1){printf("Could not destroy sem_online");
				   exit(EXIT_FAILURE);}

	return 0;

}
