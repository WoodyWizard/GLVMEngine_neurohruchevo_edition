#ifndef UDP_SERVER_LINUX
#define UDP_SERVER_LINUX

#ifdef __linux__
#include <arpa/inet.h>
#include <unistd.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


namespace GLVM::core
{
	struct UDP_ServerLinux {
		unsigned short port;
		static const unsigned int  maxBufferSize = 1024;
		int            socketFileDescriptor = -1;
		sockaddr_in    serverAddress;
		sockaddr_in    clientAddress;
		socklen_t      clientAddressLength = sizeof(clientAddress);
		bool           hasClientAddress = false;           ///< clientAddress is valid (a datagram was received)
		char           buffer[maxBufferSize];

		UDP_ServerLinux( unsigned long port = 8080 );
		UDP_ServerLinux( const UDP_ServerLinux& ) = delete;
		UDP_ServerLinux& operator=( const UDP_ServerLinux& ) = delete;
		char* receive();                                   ///< Blocks; returns a NUL-terminated datagram or nullptr on error
		void response();
		~UDP_ServerLinux();
	};
}; // namespace GLVM::core

#endif
