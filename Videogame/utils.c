#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <semaphore.h>
#include <netinet/in.h>

#include "utils.h"
#include "common.h"

int recv_TCP(int socket, char *buf, size_t len, int flags) {
	int ret, bytes_read = 0, finito = 0;

	// Ricezione conoscendo la dimensione (len > 1)

	if (len > 1) {

		do {

			ret = recv(socket, buf, len, flags);

		} while (ret == -1 && errno == EINTR);

		if (ret == -1 && errno == ENOTCONN) {
			printf("Connection closed. ");
			return -2;
		}

	}

	// Ricezione non conoscendo la dimensione (len == 1)

	else {
		
		while (!finito) {
			ret = recv(socket, buf+bytes_read, 1, flags);

			if (DEBUG) printf("[RECV_TCP] Ho ricevuto byte\n");

			if (errno == EINTR) continue;
			if (errno == ENOTCONN) {
				printf("Connection closed. ");
				return -2;
			}

			if (buf[bytes_read] == '\n' || buf[bytes_read] == '\0') {
				if (DEBUG) printf("[RECV_TCP] Fine stringa\n");
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

		if (errno == EINTR) {
			bytes_sent += ret;
			continue;
		}

		if (errno == ENOTCONN) {
			printf("Connection closed. ");
			return -2;
		}

		bytes_sent += ret;

		if (bytes_sent == len) finito = 1;

	}

	ret = bytes_sent;

	return ret;
}

int recv_UDP(int socket, char *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen) {
	int ret, bytes_read = 0;

	// Ricezione conoscendo la dimensione (len > 1)
	if (len > 1) {

		do {

			ret = recvfrom(socket, buf, len, flags, src_addr, addrlen);

		} while (ret == -1 && errno == EINTR);

		if (errno == ENOTCONN) {
			printf("Connection closed. ");
			return -2;
		}

	}

	// Ricezione non conoscendo la dimensione (len == 1)

	else {

		while (1) {

			ret = recv(socket, buf+bytes_read, len, flags);

			if (errno == EINTR) continue;
			if (errno == ENOTCONN) {
				printf("Connection closed. ");
				return -2;
			}

			if (buf[bytes_read] == '\n' || buf[bytes_read] == '\0'/* DA CONTROLLARE LA CORRETTEZZA */) {
				buf[bytes_read] = '\0';
				bytes_read++;
				break;
			}

			bytes_read++;

		}

		ret = bytes_read;

	}

	return ret;
}

int send_UDP(int socket, const char *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen) {
	int ret, bytes_sent = 0;

	while (1) {

		ret = sendto(socket, buf+bytes_sent, len-bytes_sent, flags, dest_addr, addrlen);

		if (errno == EINTR) {
			bytes_sent += ret;
			continue;
		}

		if (errno == ENOTCONN) {
			printf("Connection closed. ");
			return -2;
		}

		bytes_sent += ret;

		if (bytes_sent == len) break;

	}

	ret = bytes_sent;

	return ret;
}

/**int sem_clean(sem_t sem_utenti, sem_t sem_thread_UDP){
    int ret;

    ret = sem_destroy(&sem_utenti);
    if (ret == -1) printf("Could not destroy sem_utenti");

    ret = sem_destroy(&sem_thread_UDP);
    if (ret == -1) printf("Could not destroy sem_thread_UDP");

}**/
// DA CONTROLLARNE LA CORRETTEZZA
