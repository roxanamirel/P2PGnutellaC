// Protocol.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "conio.h"
#include <stdlib.h>
#include "p2p.h"
#include <ws2tcpip.h>


// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")


unsigned int getUniqueMessageId() {

	SYSTEMTIME t;
	GetSystemTime(&t);
	//TODO add a sequence number
	return (3232235797 + PORT_DEFAULT + t.wHour + t.wMinute + t.wSecond + t.wMilliseconds);
}

int main(int argc, char** argv)
{
	//create a JOIN message
	P2P_h p2pHeader;
	p2pHeader.version = 1;
	p2pHeader.ttl = 1;
	p2pHeader.reserved = 0;
	p2pHeader.org_port = PORT_DEFAULT;
	p2pHeader.org_ip = 3232235797;
	p2pHeader.msg_type = MSG_JOIN;
	p2pHeader.msg_id = getUniqueMessageId();
	p2pHeader.length = 0;

	SOCKET ConnectSocket = INVALID_SOCKET;
	WSADATA wsaData;
	struct addrinfo *result = NULL,
		*ptr = NULL,
		hints;
	int iResult;


	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed with error: %d\n", iResult);
		return 1;
	}

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	// Resolve the server address and port
	iResult = getaddrinfo("130.233.195.30", "6346", &hints, &result);
	if (iResult != 0) {
		printf("getaddrinfo failed with error: %d\n", iResult);
		WSACleanup();
		return 1;
	}


	// Attempt to connect to an address until one succeeds
	for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {

		// Create a SOCKET for connecting to server
		ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype,
			ptr->ai_protocol);
		if (ConnectSocket == INVALID_SOCKET) {
			printf("socket failed with error: %ld\n", WSAGetLastError());
			WSACleanup();
			return 1;
		}

		// Connect to server.
		iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
		if (iResult == SOCKET_ERROR) {
			closesocket(ConnectSocket);
			ConnectSocket = INVALID_SOCKET;
			continue;
		}
		break;
	}

	freeaddrinfo(result);

	if (ConnectSocket == INVALID_SOCKET) {
		printf("Unable to connect to server!\n");
		WSACleanup();
		return 1;
	}
	char frame[HLEN];
	memcpy(frame, &p2pHeader, sizeof(frame));
	// Send an initial buffer
	iResult = send(ConnectSocket, frame, HLEN, 0);
	if (iResult == SOCKET_ERROR) {
		printf("send failed with error: %d\n", WSAGetLastError());
		closesocket(ConnectSocket);
		WSACleanup();
		return 1;
	}



	int queryNo = 1;

	P2P_h join_response;
	int recvbuflen = sizeof(join_response);
	char recvbuf[sizeof(join_response)];
	// Receive until the peer closes the connection
	do {

		iResult = recv(ConnectSocket, recvbuf, recvbuflen, 0);

		if (iResult > 0) {
			printf("Bytes received: %d\n", iResult);
			memcpy(&join_response, recvbuf, sizeof(recvbuf));
			if (join_response.version == 1 && join_response.ttl > 0 && join_response.ttl <= 5) {
				//(0x0200)16 = (512)10
				if (join_response.msg_type == MSG_JOIN) {
					if (join_response.length == 512) {
						printf("Length: %u", join_response.length);
						printf("\n");
						printf("MessageType: %u", join_response.msg_type);
						printf("\n");
						printf("Message ID: %u", join_response.msg_id);
						printf("\n");
						printf("\n");
						//---------------------------------------------------------------------
						if (queryNo == 1) {
							queryNo--;
							//create a QUERY message
							char query_message[] = "vm1testkeyp";
							query_message[11] = NULL;
							

							P2P_h p2pHeaderQuery;
							p2pHeader.version = 1;
							p2pHeader.ttl = 3;
							p2pHeader.reserved = 0;
							p2pHeader.org_port = PORT_DEFAULT;
							p2pHeader.org_ip = 3232235797;
							p2pHeader.msg_type = MSG_QUERY;
							p2pHeader.msg_id = getUniqueMessageId();
							p2pHeader.length = sizeof(query_message);

							//----------------------------------
							char frame2[HLEN+1];
							const int size_total_frame = HLEN + sizeof(query_message);
							char total_frame[size_total_frame];
												
							memcpy(frame2, &p2pHeaderQuery, sizeof(frame2));
							
							frame2[sizeof(frame2) - 1] = NULL;
							strcpy_s(total_frame, frame2);
							strcat_s(total_frame, query_message);
							
							puts("SENDING: ");
							puts(total_frame);
							// Send an initial buffer
							int iResult2 = send(ConnectSocket, total_frame, HLEN + sizeof(query_message), 0);
							if (iResult2 == SOCKET_ERROR) {
								printf("send failed with error: %d\n", WSAGetLastError());
								closesocket(ConnectSocket);
								WSACleanup();
								return 1;
							}
							else {
								printf("Bytes sent from query: %d\n", iResult2);


							}
						}
					}

					else {
						printf("Join was not possible");
					}
				}
				if (join_response.msg_type == MSG_QHIT) {
					printf("HIIIIIIIIIIIIIIIIIT");
				}
				if (join_response.msg_type == MSG_PING) {
					printf("PING\n");
				}
			}


		}

		else if (iResult == 0)
			printf("Connection closed\n");
		else
			printf("recv failed with error: %d\n", WSAGetLastError());

	} while (iResult > 0);

	// cleanup
	closesocket(ConnectSocket);
	WSACleanup();





	getchar();

	return 0;
}



