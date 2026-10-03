#include "Network/UDP_ClientLinux.hpp"

int main() {
	GLVM::core::UDP_ClientLinux clientLinux;
	clientLinux.response();
	char* massageFromServer = clientLinux.receive();
	if ( massageFromServer == nullptr ) {
		printf("No answer from the server\n");
		return 1;
	}
	printf("Message from server: %s\n", massageFromServer);

	return 0;
}
