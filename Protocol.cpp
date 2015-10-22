// Protocol.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "conio.h"
#include <stdlib.h>
#include "p2p.h"
#include <ws2tcpip.h>

#include <inttypes.h> /* strtoimax */

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


P2P_h getJoinRequestMessage() {
	//create a JOIN message
	P2P_h p2pHeader;
	p2pHeader.version = 1;
	p2pHeader.ttl = 3;
	p2pHeader.reserved = 0;
	p2pHeader.org_port = htons(PORT_DEFAULT);
	p2pHeader.org_ip = htonl(3232235797);
	p2pHeader.msg_type = MSG_JOIN;
	p2pHeader.msg_id = htonl(getUniqueMessageId());
	p2pHeader.length = htons(0);
	return p2pHeader;

}
P2P_h getQueryMessageHeader(int queryLength) {
	P2P_h p2pHeaderQuery;
	p2pHeaderQuery.version = 1;
	p2pHeaderQuery.ttl = 1;
	p2pHeaderQuery.reserved = 0;
	p2pHeaderQuery.org_port = htons(PORT_DEFAULT);
	p2pHeaderQuery.org_ip = htonl(3232235797);
	p2pHeaderQuery.msg_type = MSG_QUERY;
	p2pHeaderQuery.msg_id = htonl(getUniqueMessageId());
	p2pHeaderQuery.length = htons(queryLength);
	return p2pHeaderQuery;
}

SOCKET ConnectSocket = INVALID_SOCKET;
int initializeConnection() {

	WSADATA wsaData;
	struct addrinfo *result = NULL, *ptr = NULL, hints;
	int iResult;


	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed with error: %d\n", iResult);
		return -1;

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

	}
	// Attempt to connect to an address until one succeeds
	for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {

		// Create a SOCKET for connecting to server
		ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype,
			ptr->ai_protocol);
		if (ConnectSocket == INVALID_SOCKET) {
			printf("socket failed with error: %ld\n", WSAGetLastError());
			WSACleanup();
			return -1;
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
		return -1;

	}
	return 0;

}

int send_join_request() {
	P2P_h p2pHeader = getJoinRequestMessage();
	char frame[HLEN];
	memcpy(frame, &p2pHeader, sizeof(frame));
	// Send join request message
	int iResult = send(ConnectSocket, frame, HLEN, 0);
	if (iResult == SOCKET_ERROR) {
		printf("send failed with error: %d\n", WSAGetLastError());
		closesocket(ConnectSocket);
		WSACleanup();
		return 1;

	}
	return 0;
}

boolean isGunutellaPackage(P2P_h received_header) {
	return received_header.version == P_VERSION && received_header.ttl > 0 && received_header.ttl <= MAX_TTL;
}

int processJoinResponseBody(char* recvbuf) {
	P2P_join join_result;

	memcpy(&join_result, recvbuf + HLEN, JOINLEN);
	if (ntohs(join_result.status) == JOIN_ACC) {
		printf("Bootstrap process succeeded!! \n");
		return 0;

	}
	else {
		printf("Bootstrap process failed!!");
		return -1;
	}
}


int msg_id;
void sendQuery() {
	char query_message[] = "vm1testkey";
	query_message[sizeof(query_message) - 1] = '\0';

	P2P_h p2pHeaderQuery = getQueryMessageHeader(strlen(query_message));
	msg_id = p2pHeaderQuery.msg_id;

	//Create buffer that can hold both.
	char combined[HLEN + sizeof(query_message)];

	//Copy arrays in individually.
	memcpy(combined, &p2pHeaderQuery, HLEN);
	memcpy(combined + HLEN, query_message, sizeof(query_message));

	int iResult2 = send(ConnectSocket, combined, sizeof(combined), 0);
	if (iResult2 == SOCKET_ERROR) {
		printf("send failed with error: %d\n", WSAGetLastError());
		closesocket(ConnectSocket);
		WSACleanup();
		return;
	}
	else {
		printf("Bytes sent from query: %d\n\n", iResult2);
	}

}

int main(int argc, char** argv)
{
	if (initializeConnection() == -1) {
		printf("The connection could not be established!");
	}
	else {
		if (send_join_request() == 0) {
			printf("Join request was sent. Waiting for join response. \n");
		}
	}

	int iResult;
	P2P_h received_header;
	char recvbuf[64];
	bool connected = false;
	// Receive 
	do {
		iResult = recv(ConnectSocket, recvbuf, 64, 0);
		if (iResult > 0) {
			memcpy(&received_header, recvbuf, HLEN);
			if (isGunutellaPackage(received_header)) {

				if (received_header.msg_type == MSG_JOIN) {
					if (htons(received_header.length) == JOINLEN) {
						printf("A join response was received. \n");
						if (processJoinResponseBody(recvbuf) == -1) {
							break;
						}
						else {
							sendQuery();
						}

					}

				}
				else if (received_header.msg_type == MSG_QHIT) {
					printf("HIT! \n");
					if (ntohl(received_header.msg_id == msg_id)) {
						printf("HIT for a query request \n");
						
						P2P_hit_front hit_front;
						memcpy(&hit_front, recvbuf+HLEN, 4);
						printf("Matched Resources: %d \n", ntohs(hit_front.entry_size));
						printf("SBZ is: %d \n ", ntohs(hit_front.sbz));

						P2P_hit_entry hit_entry;
						memcpy(&hit_entry, recvbuf + HLEN + 4, 8);
						printf("Resource Id: %d \n", ntohs(hit_entry.resourceId));
						printf("Resource Value: 0x%x \n", ntohl(hit_entry.resourceValue));
						

					}




				}
				else if (received_header.msg_type == MSG_PING) {
					printf("PING\n");
				}
				else {
					printf("OTHER TYPE OF MESG:  \n");
					printf("Length: %u", received_header.length);
					printf("\n");
					printf("MessageType: %u", received_header.msg_type);
					printf("\n");
					printf("0x%02x, ", received_header.msg_type);
					printf("\n");
					printf("Message ID: %u", received_header.msg_id);
					printf("\n");
					printf("TTL: %u", received_header.ttl);
					printf("\n--------------------");
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



