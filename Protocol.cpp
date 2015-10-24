// Protocol.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "conio.h"
#include <stdlib.h>
#include "p2p.h"
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>
#include <inttypes.h> /* strtoimax */

// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

#define   BOOTSTRAP_SOCKET 0
#define   PEER1_SOCKET 1
#define   PEER2_SOCKET 2

HANDLE gDoneEvent;
HANDLE hTimerQueue = NULL;
SOCKET ConnectSocket[3] = { INVALID_SOCKET };


void sendTypeAPingMessage();


unsigned int getUniqueMessageId() {

	SYSTEMTIME t;
	GetSystemTime(&t);
	//TODO add a sequence number
	return (3232235797 + PORT_DEFAULT + t.wHour + t.wMinute + t.wSecond + t.wMilliseconds);
}

VOID CALLBACK TimerRoutine(PVOID lpParam, BOOLEAN TimerOrWaitFired)
{
	if (lpParam == NULL)
	{
		printf("TimerRoutine lpParam is NULL\n");
	}
	else
	{
		// lpParam points to the argument; in this case it is an int

		
		if (TimerOrWaitFired)
		{
			printf("\n****************Five seconds passed****************\n\n");
			sendTypeAPingMessage();
		}
		else
		{
			printf("The wait event was signaled.\n");
		}
	}

	SetEvent(gDoneEvent);
}

int myTimer(HANDLE hTimerQueue) {

	HANDLE hTimer = NULL;

	// Use an event object to track the TimerRoutine execution
	gDoneEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (NULL == gDoneEvent)
	{
		printf("CreateEvent failed (%d)\n", GetLastError());
		return 1;
	}

	// Create the timer queue.
	hTimerQueue = CreateTimerQueue();
	if (NULL == hTimerQueue)
	{
		printf("CreateTimerQueue failed (%d)\n", GetLastError());
		return 2;
	}

	// Set a timer to call the timer routine in 10 seconds.
	int arg = 123;
	if (!CreateTimerQueueTimer(&hTimer, hTimerQueue,
		(WAITORTIMERCALLBACK)TimerRoutine, &arg, 10000, 5000, 0))
	{
		printf("CreateTimerQueueTimer failed (%d)\n", GetLastError());
		return 3;
	}


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

P2P_h getQueryHitMessageHeader(uint32_t msg_id) {
	P2P_h p2pHeaderQuery;
	p2pHeaderQuery.version = 1;
	p2pHeaderQuery.ttl = 5;
	p2pHeaderQuery.reserved = 0;
	p2pHeaderQuery.org_port = htons(PORT_DEFAULT);
	p2pHeaderQuery.org_ip = htonl(3232235797);
	p2pHeaderQuery.msg_type = MSG_QHIT;
	p2pHeaderQuery.msg_id = htonl(msg_id);
	p2pHeaderQuery.length = 0;
	return p2pHeaderQuery;
}

P2P_h getPongTypeAMessage(uint32_t msg_id) {
	//create a PONG message
	P2P_h p2pHeader;
	p2pHeader.version = 1;
	p2pHeader.ttl = 1;
	p2pHeader.reserved = 0;
	p2pHeader.org_port = htons(PORT_DEFAULT);
	p2pHeader.org_ip = htonl(3232235797);
	p2pHeader.msg_type = MSG_PONG;
	p2pHeader.msg_id = htonl(msg_id);
	p2pHeader.length = htons(0);
	return p2pHeader;

}

P2P_h getPingTypeAMessage() {
	//create a JOIN message
	P2P_h p2pHeader;
	p2pHeader.version = 1;
	p2pHeader.ttl = 1;
	p2pHeader.reserved = 0;
	p2pHeader.org_port = htons(PORT_DEFAULT);
	p2pHeader.org_ip = htonl(3232235797);
	p2pHeader.msg_type = MSG_PING;
	p2pHeader.msg_id = htonl(getUniqueMessageId());
	p2pHeader.length = htons(0);
	return p2pHeader;

}

int initializeConnection(char* ip, char* port, int socketNo) {

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
	iResult = getaddrinfo(ip, port, &hints, &result);
	if (iResult != 0) {
		printf("getaddrinfo failed with error: %d\n", iResult);
		WSACleanup();

	}
	// Attempt to connect to an address until one succeeds
	for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {

		// Create a SOCKET for connecting to server
		ConnectSocket[socketNo] = socket(ptr->ai_family, ptr->ai_socktype,
			ptr->ai_protocol);
		if (ConnectSocket[socketNo] == INVALID_SOCKET) {
			printf("socket failed with error: %ld\n", WSAGetLastError());
			WSACleanup();
			return -1;
		}

		// Connect to server.
		iResult = connect(ConnectSocket[socketNo], ptr->ai_addr, (int)ptr->ai_addrlen);
		if (iResult == SOCKET_ERROR) {
			closesocket(ConnectSocket[socketNo]);
			ConnectSocket[socketNo] = INVALID_SOCKET;
			continue;
		}
		break;
	}

	freeaddrinfo(result);

	if (ConnectSocket[socketNo] == INVALID_SOCKET) {
		printf("Unable to connect to server!\n");
		WSACleanup();
		return -1;

	}
	return 0;

}

int send_join_request(int socketNo) {
	P2P_h p2pHeader = getJoinRequestMessage();
	char frame[HLEN];
	memcpy(frame, &p2pHeader, sizeof(frame));
	// Send join request message
	int iResult = send(ConnectSocket[socketNo], frame, HLEN, 0);
	if (iResult == SOCKET_ERROR) {
		printf("send failed with error: %d\n", WSAGetLastError());
		closesocket(ConnectSocket[socketNo]);
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
	return ntohs(join_result.status) == JOIN_ACC;

}

void extend_network(char *ip, char *port, int socketNo) {

	if (initializeConnection(ip, port, socketNo) == -1) {
		printf("The connection could not be established on socket: %d with Ip:  %s  ", socketNo, ip);
	}
	else {
		if (send_join_request(socketNo) == 0) {
			printf("Join request was sent to %s on socket %d. Waiting for join response. \n", ip, socketNo);
		}
	}

}

int msg_id;
void sendQuery(int socketNo) {
	char query_message[11] = "vm_testkey";
	query_message[2] = (socketNo + 1) + '0';
	query_message[sizeof(query_message) - 1] = '\0';

	P2P_h p2pHeaderQuery = getQueryMessageHeader(strlen(query_message));
	msg_id = p2pHeaderQuery.msg_id;

	//Create buffer that can hold both.
	char combined[HLEN + sizeof(query_message)];

	//Copy arrays in individually.
	memcpy(combined, &p2pHeaderQuery, HLEN);
	memcpy(combined + HLEN, query_message, sizeof(query_message));

	int iResult2 = send(ConnectSocket[socketNo], combined, sizeof(combined), 0);
	if (iResult2 == SOCKET_ERROR) {
		printf("send failed with error: %d\n", WSAGetLastError());
		closesocket(ConnectSocket[socketNo]);
		WSACleanup();
		return;
	}
	else {
		printf("Bytes sent from query: %d\n\n", iResult2);
	}

}

void send_PongAMessage(P2P_h pongA, int socketNo) {
	//Create buffer that can hold both.
	char pongString[HLEN];

	//Copy array
	memcpy(pongString, &pongA, HLEN);

	int iResult2 = send(ConnectSocket[socketNo], pongString, sizeof(pongString), 0);
	if (iResult2 == SOCKET_ERROR) {
		printf("send failed with error: %d\n", WSAGetLastError());
		closesocket(ConnectSocket[socketNo]);
		WSACleanup();
		return;
	}
	else {
		printf("Sent Pong A to: %d \n\n", socketNo);
	}

}

void handlePongResponse(int socketNo, char querybuf[], P2P_h pingA) {

	P2P_h header;
	memcpy(&header, querybuf, HLEN);
	if (header.msg_type == MSG_PONG) {
		if (header.msg_id == pingA.msg_id) {
			printf("Received PONG TYPE A to %d.\n", socketNo);
		}
		else {
			printf("Received PONG A with different msg_id");
		}
	}
	else {
		printf("different kind of msg %u", header.msg_type);
	}
}

void sendTypeAPingMessage() {

	int iResult[3];
	char querybuf[HLEN];
	for (int socketNo = 0; socketNo < 3; socketNo++) {
		if (ConnectSocket[socketNo] != INVALID_SOCKET) {
			P2P_h pingA = getPingTypeAMessage();
			char pingString[HLEN];
			//Copy array
			memcpy(pingString, &pingA, HLEN);

			int iResult2 = send(ConnectSocket[socketNo], pingString, sizeof(pingString), 0);
			
			//wait to receive the pong
			iResult[socketNo] = recv(ConnectSocket[socketNo], querybuf, HLEN, 0);
			if (iResult[socketNo] > 0) {
				//handlePongResponse(socketNo, querybuf, pingA);
			}
			else if (iResult[socketNo] == 0)
				printf("Connection closed: %d \n", socketNo);
			else
				printf("recv failed ** with error: %d\n", WSAGetLastError());
			//-----------------------------------------------



			if (iResult2 == SOCKET_ERROR) {
				printf("send failed with error: %d\n", WSAGetLastError());
				closesocket(ConnectSocket[socketNo]);
				WSACleanup();
				return;
			}
			else {
				printf("Sent Ping A to: %d \n\n ", socketNo);
			}

			
			
		}
	}
}

void printQueryResult(int socketNo, char *recvbuf) {

	printf("HIT for a query request from %d \n", socketNo);

	P2P_hit_front hit_front;
	memcpy(&hit_front, recvbuf + HLEN, 4);
	printf("Matched Resources: %d\n", ntohs(hit_front.entry_size));
	printf("SBZ is: %d\n", ntohs(hit_front.sbz));

	P2P_hit_entry hit_entry;
	memcpy(&hit_entry, recvbuf + HLEN + 4, 8);
	printf("Resource Id: %d \n", ntohs(hit_entry.resourceId));
	printf("Resource Value: 0x%x\n\n", ntohl(hit_entry.resourceValue));

}

void sendQueryHit(char *recvbuf, P2P_h received_header, int socketNo) {

	int length = received_header.length;
	P2P_query query;
	memcpy(&query, recvbuf + HLEN, length);
	if (strcmp(query.search_criteria, "roxyvlad\0") == 0) {
		P2P_h queryHit = getQueryHitMessageHeader(received_header.msg_id);

		P2P_hit_front hit_front;
		hit_front.entry_size = htons((uint16_t)1);
		hit_front.sbz = htons((uint16_t)0);

		P2P_hit_entry hit_entry;
		uint16_t resourceId;
		memcpy(&resourceId, query.search_criteria, sizeof(uint16_t));
		hit_entry.resourceId = htons(resourceId);
		hit_entry.resourceValue = htonl(MYVALUE);
		hit_entry.sbz = 0;

		//Create buffer that can hold all.
		char combined[HLEN + sizeof(P2P_hit_front) + sizeof(P2P_hit_entry)];
		//Copy arrays in individually.
		memcpy(combined, &queryHit, HLEN);
		memcpy(combined + HLEN, &hit_entry, sizeof(P2P_hit_entry));
		memcpy(combined + HLEN + sizeof(P2P_hit_entry), &hit_front, sizeof(P2P_hit_front));
		int iResult2 = send(ConnectSocket[socketNo], combined, sizeof(combined), 0);
		if (iResult2 == SOCKET_ERROR) {
			printf("send failed with error: %d\n", WSAGetLastError());
			closesocket(ConnectSocket[socketNo]);
			WSACleanup();
			return;
		}
		else {
			printf("Bytes sent from query: %d\n\n", iResult2);
		}


	}
}

void process_receive(char* recvbuf, int socketNo) {
	P2P_h received_header;
	int iResult[3];
	char querybuf[64];
	memcpy(&received_header, recvbuf, HLEN);
	if (isGunutellaPackage(received_header)) {

		switch (received_header.msg_type) {

		case MSG_JOIN:
			if (htons(received_header.length) == JOINLEN) {
				printf("A join response was received from %d \n", socketNo);
				if (!processJoinResponseBody(recvbuf)) {
					break;
				}
				else {
					sendQuery(socketNo);
					//iResult[socketNo] = recv(ConnectSocket[socketNo], querybuf, 64, 0);
					//printQueryResult(socketNo, querybuf);
					//sendTypeAPingMessage();
				}
			} break;


		case MSG_QHIT:
			if (ntohl(received_header.msg_id == msg_id)) {
				printQueryResult(socketNo, recvbuf);
			}
			break;
		case MSG_QUERY:
			printf("Somebody asked for our data!!!! %d \n", socketNo);
			if (received_header.ttl > 1) {
				//TODO forward query
			}
			sendQueryHit(recvbuf, received_header, socketNo);
			break;
		case MSG_PING:
			if (received_header.ttl == 1) {
				printf("PING TYPE A received from socket:%d\n", socketNo);
				P2P_h pongA = getPongTypeAMessage(htonl(received_header.msg_id));
				send_PongAMessage(pongA, socketNo);

			}
			if (received_header.ttl > 1) {
				printf("PING TYPE B received from socket:%d\n", socketNo);
			}
			break;

		default:
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
			break;
		}
	}
}

void receive() {

	int iResult[3];
	int socketNo = -1;
	char recvbuf[3][64];

	// Receive 
	do {
		socketNo = ++socketNo % 3;
		iResult[socketNo] = recv(ConnectSocket[socketNo], recvbuf[socketNo], 64, 0);
		if (iResult[socketNo] > 0) {
			process_receive(recvbuf[socketNo], socketNo);

		}
		else if (iResult[socketNo] == 0)
			printf("Connection closed\n");
		else
			printf("recv failed with error: %d\n", WSAGetLastError());

	} while (iResult[socketNo] > 0);

	// cleanup
	for (int i = 0; i < 3; i++) {
		closesocket(ConnectSocket[i]);
	}
	WSACleanup();


}

int main(int argc, char** argv)
{

	myTimer(hTimerQueue);

	printf("EXTENDING MY NETWORK.............................\n\n");

	char *bootstrap_ip = "130.233.195.30";
	char *port = "6346";
	//connect to bootstrap node first and send JOIN message
	extend_network(bootstrap_ip, port, BOOTSTRAP_SOCKET);

	char* ip2 = "130.233.195.31";
	extend_network(ip2, port, PEER1_SOCKET);
	//receive(PEER1_SOCKET);

	char* ip3 = "130.233.195.32";
	extend_network(ip3, port, PEER2_SOCKET);
	//receive(PEER2_SOCKET);

	//receive from peers
	receive();

	// Wait for the timer-queue thread to complete using an event 
	// object. The thread will signal the event at that time.

	if (WaitForSingleObject(gDoneEvent, INFINITE) != WAIT_OBJECT_0)
		printf("WaitForSingleObject failed (%d)\n", GetLastError());

	puts("\n\nDONE\n\n");

	getchar();

	CloseHandle(gDoneEvent);

	// Delete all timers in the timer queue.
	if (!DeleteTimerQueue(hTimerQueue))
		printf("DeleteTimerQueue failed (%d)\n", GetLastError());

	getchar();

	return 0;
}



