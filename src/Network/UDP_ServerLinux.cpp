#include "Network/UDP_ServerLinux.hpp"

namespace GLVM::core
{
	UDP_ServerLinux::UDP_ServerLinux( unsigned long port ) : port((unsigned short)port) {
		if ( port == 0 || port > 65535 ) {
			fprintf(stderr, "UDP server: invalid port %lu\n", port);
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

		/// Socket addresss filling
		memset(&serverAddress, 0, sizeof(serverAddress));
		memset(&clientAddress, 0, sizeof(clientAddress));
		serverAddress.sin_family = AF_INET;
		serverAddress.sin_addr.s_addr = INADDR_ANY;           ///< Receive messages from any addresses
		serverAddress.sin_port = htons(this->port);           ///< htons() function converts the unsigned short integer hostshort from host byte order to network byte order

		/** Socket to port binding
			ip   - specific machine address
			port - specific application intrance
		**/
		if (bind(socketFileDescriptor, (const struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) {
			perror("Bind failed");
			close(socketFileDescriptor);
			exit(EXIT_FAILURE);
		}
	}

	char* UDP_ServerLinux::receive() {
		clientAddressLength = sizeof(clientAddress);
		/// One byte is kept for the terminating '\0'; a longer datagram is truncated by the kernel
		const ssize_t n = recvfrom(socketFileDescriptor, buffer, maxBufferSize - 1, 0, (struct sockaddr *)&clientAddress, &clientAddressLength);
        if (n < 0) {
            perror("Receive failed");
			hasClientAddress = false;
			buffer[0] = '\0';
			return nullptr;
        }

		hasClientAddress = clientAddressLength == sizeof(clientAddress) && clientAddress.sin_family == AF_INET;
		buffer[n] = '\0';  ///< Complete string
		return buffer;
	}

	void UDP_ServerLinux::response() {
		if ( !hasClientAddress )
			return;

		// Sending response to client
        const char *response = "Message received!";
        if ( sendto(socketFileDescriptor, response, strlen(response), 0, (const struct sockaddr *)&clientAddress, clientAddressLength) < 0 )
			perror("Send failed");
	}

	UDP_ServerLinux::~UDP_ServerLinux() {
		/// Socket closing
		if ( socketFileDescriptor >= 0 )
			close(socketFileDescriptor);
	}
}; ///< namespace GLVM::core
