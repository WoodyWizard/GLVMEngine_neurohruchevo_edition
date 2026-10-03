#include "Network/UDP_ClientLinux.hpp"
#include <sys/time.h>

namespace GLVM::core
{
	UDP_ClientLinux::UDP_ClientLinux( unsigned long port, const char* serverIP ) : port((unsigned short)port), serverIP(serverIP) {
		if ( port == 0 || port > 65535 ) {
			fprintf(stderr, "UDP client: invalid port %lu\n", port);
			exit(EXIT_FAILURE);
		}

		/** UDP-socket creation
			@param AF_INET    protocol family (IPv4)
			@param SOCK_DGRAM socket type
			@param 0          protocol type. If we pass 0 then function will depending on socket type (For SOCK_DGRAM its UDP)
		**/
		if ((socketFileDescriptor = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
			perror("Socket creation failed");
			exit(EXIT_FAILURE);
		}

		/// A lost datagram must not block the caller forever
		struct timeval timeout = { receiveTimeoutSeconds, 0 };
		if ( setsockopt(socketFileDescriptor, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0 )
			perror("setsockopt(SO_RCVTIMEO) failed");

		/// Socket addresss filling
		memset(&serverAddress, 0, sizeof(serverAddress));
		serverAddress.sin_family = AF_INET;
		serverAddress.sin_port = htons(this->port);                ///< htons() function converts the unsigned short integer hostshort from host byte order to network byte order
		if ( serverIP == nullptr || inet_pton(AF_INET, serverIP, &serverAddress.sin_addr) != 1 ) {
			fprintf(stderr, "UDP client: invalid IPv4 address \"%s\"\n", serverIP ? serverIP : "(null)");
			close(socketFileDescriptor);
			exit(EXIT_FAILURE);
		}
	}

	char* UDP_ClientLinux::receive() {
		/// Receiving response from server
		sockaddr_in senderAddress;
		socklen_t senderAddressLength = sizeof(senderAddress);
		/// One byte is kept for the terminating '\0'; a longer datagram is truncated by the kernel
		const ssize_t n = recvfrom(socketFileDescriptor, buffer, maxBufferSize - 1, 0, (struct sockaddr *)&senderAddress, &senderAddressLength);
		if ( n < 0 ) {
			perror("Receive failed");
			buffer[0] = '\0';
			return nullptr;
		}

		if ( senderAddress.sin_addr.s_addr != serverAddress.sin_addr.s_addr || senderAddress.sin_port != serverAddress.sin_port ) {
			buffer[0] = '\0';
			return nullptr;                                       ///< Datagram from someone else than the server
		}

		buffer[n] = '\0';                             ///< Complete string
		return buffer;
	}

	void UDP_ClientLinux::response() {
		/// Sending message to server
		if ( sendto(socketFileDescriptor, message, strlen(message), 0, (const struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0 )
			perror("Send failed");
	}

	UDP_ClientLinux::~UDP_ClientLinux() {
		/// Socket closing
		if ( socketFileDescriptor >= 0 )
			close(socketFileDescriptor);
	}
}; ///< namespace GLVM::core
