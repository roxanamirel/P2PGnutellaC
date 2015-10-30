#include "stdafx.h"
#include <stdio.h>
#include <winsock2.h>
#include "p2p.h"

#include <ws2tcpip.h>
#include <windows.h>
#include <inttypes.h>
#include "GnutellaHeader.h"
#include "Params.h"

#pragma comment(lib, "ws2_32.lib") //Winsock Library

static int activeNeighbours = 0;

SOCKET master, new_socket, client_socket[TOTAL_POSSIBLE_NEIGHBOURS], s;

Neighbour *neighbourArray = new Neighbour[TOTAL_POSSIBLE_NEIGHBOURS];

RecentQuery *queries = new RecentQuery[TOTAL_POSSIBLE_NEIGHBOURS];
int recentQueryNo = 0;

uint32_t msg_id;

HANDLE gDoneEvent;
HANDLE hTimerQueue = NULL;

void sendTypeAPingMessage();
void sendTypeBPingMessage();

VOID CALLBACK TimerRoutine(PVOID lpParam, BOOLEAN TimerOrWaitFired)
{
	if (lpParam == NULL) {
		printf("TimerRoutine lpParam is NULL\n");
	}
	else {
		// lpParam points to the argument; in this case it is an int
		if (TimerOrWaitFired) {
			printf("\n****************Five seconds passed****************\n\n");
			sendTypeAPingMessage();
			sendTypeBPingMessage();
		}
		else {
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

int initializeConnection(char* ip, char* port, int socketNo) {

	struct addrinfo *result = NULL, *ptr = NULL, hints;
	int iResult;

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	// Resolve the server address and port
	iResult = getaddrinfo(ip, port, &hints, &result);
	if (iResult != 0) {
		printf("[initializeConnection] getaddrinfo failed with error: %d\n", iResult);
		//WSACleanup();
	}

	// Attempt to connect to an address until one succeeds
	for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {

		// Create a SOCKET for connecting to server
		client_socket[socketNo] = socket(ptr->ai_family, ptr->ai_socktype,
			ptr->ai_protocol);
		if (client_socket[socketNo] == INVALID_SOCKET) {
			printf("[initializeConnection] socket failed with error: %ld\n", WSAGetLastError());
			//WSACleanup();
			return -1;
		}

		// Connect to server.
		iResult = connect(client_socket[socketNo], ptr->ai_addr, (int)ptr->ai_addrlen);
		if (iResult == SOCKET_ERROR) {
			printf("[initializeConnection] Connection Error: %ld\n", WSAGetLastError());
			closesocket(client_socket[socketNo]);
			client_socket[socketNo] = INVALID_SOCKET;
			continue;
		}
		break;
	}
	freeaddrinfo(result);

	if (client_socket[socketNo] == INVALID_SOCKET) {
		printf("[initializeConnection] Unable to connect to server!\n");
		//WSACleanup();
		return -1;
	}
	return 0;
}

void send_join_request(int socketNo) {

	P2P_h p2pHeader = getJoinRequestMessage();
	char frame[HLEN];
	memcpy(frame, &p2pHeader, sizeof(frame));

	send(client_socket[socketNo], frame, HLEN, 0);
}

void extend_network(char *ip, char *port, int socketNo) {

	if (initializeConnection(ip, port, socketNo) == -1) {
		printf("The connection could not be established on socket: %d with Ip:  %s  ", socketNo, ip);
	}
	else {
		printf("Trying to extend network with %s on port %s and socket %u\n\n", ip, port, socketNo);

		printf("Join request was sent to %s on socket %d. Waiting for join response. \n", ip, socketNo);
		send_join_request(socketNo);
	}
}

int processJoinResponseBody(char* recvbuf) {

	P2P_join join_result;

	memcpy(&join_result, recvbuf + HLEN, JOINLEN);
	return ntohs(join_result.status) == JOIN_ACC;
}

void send_PongAMessage(P2P_h pongA, int socketNo) {

	//Create buffer that can hold both.
	char pongString[HLEN];

	//Copy array
	memcpy(pongString, &pongA, HLEN);

	printf("Sent Pong A to: %d\n\n", socketNo);
	send(client_socket[socketNo], pongString, sizeof(pongString), 0);
}

void sendQuery(int socketNo, char query_key[]) {

	printf("\n\n[Query] Looking for the key from socket %d\n", socketNo);
	printf("[Query] Search after key: %s\n\n", query_key);

	P2P_h p2pHeaderQuery = getQueryMessageHeader(strlen(query_key));
	msg_id = p2pHeaderQuery.msg_id;

	//Create buffer that can hold both.
	char combined[HLEN + OUR_SEARCH_CRITERIA_LENGTH];

	//Copy arrays in individually.
	memcpy(combined, &p2pHeaderQuery, HLEN);
	memcpy(combined + HLEN, query_key, strlen(query_key));

	int iResult = send(client_socket[socketNo], combined, sizeof(combined), 0);
	if (iResult == SOCKET_ERROR) {
		printf("send failed with error: %d\n", WSAGetLastError());
		closesocket(client_socket[socketNo]);
		WSACleanup();
		return;
	}
	else {
		printf("Bytes sent from query: %d\n\n", iResult);
	}
}
void printQueryResult(int socketNo, char *recvbuf) {

	printf("HIT for a query request from %d \n", socketNo);

	P2P_hit_front hit_front;
	memcpy(&hit_front, recvbuf + HLEN, sizeof(P2P_hit_front));
	printf("Matched Resources: %d\n", ntohs(hit_front.entry_size));
	printf("SBZ is: %d\n", ntohs(hit_front.sbz));

	P2P_hit_entry hit_entry;
	memcpy(&hit_entry, recvbuf + HLEN + sizeof(P2P_hit_front), sizeof(P2P_hit_entry));
	printf("Resource Id: %d \n", ntohs(hit_entry.resourceId));
	printf("Resource Value: 0x%x\n\n", ntohl(hit_entry.resourceValue));
}

void sendTypeAPingMessage() {

	char querybuf[HLEN];
	for (int socketNo = 0; socketNo < activeNeighbours; socketNo++) {
		if (client_socket[socketNo] != INVALID_SOCKET) {
			P2P_h pingA = getPingTypeAMessage();
			char pingString[HLEN];
			//Copy array
			memcpy(pingString, &pingA, HLEN);
			printf("Sent Ping A to %d with msg_id = %u\n\n", socketNo, htonl(pingA.msg_id));
			send(client_socket[socketNo], pingString, sizeof(pingString), 0);
		}
	}
}

void sendTypeBPingMessage() {

	char querybuf[HLEN];
	for (int socketNo = 0; socketNo < activeNeighbours; socketNo++) {
		if (client_socket[socketNo] != INVALID_SOCKET) {
			P2P_h pingB = getPingTypeAMessage();
			pingB.ttl = 3;
			char pingString[HLEN];
			//Copy array
			memcpy(pingString, &pingB, HLEN);
			printf("Sent Ping B to %d with msg_id = %u\n\n", socketNo, htonl(pingB.msg_id));
			send(client_socket[socketNo], pingString, sizeof(pingString), 0);
		}
	}
}

void handlePongResponse(int socketNo, P2P_h header, char *recvbuf) {

	if (header.length == 0) {
		printf("Received PONG TYPE A from %d with msg_id = %u.\n", socketNo, ntohl(header.msg_id));
	}
	else {
		printf("\nReceived PONG TYPE B from %d.\n", socketNo);
		P2P_pong_front pong_front;
		memcpy(&pong_front, recvbuf + HLEN, 4);
		printf("Entry size = %d\n", ntohs(pong_front.entry_size));
		printf("SBZ is: %d\n", ntohs(pong_front.sbz));

		for (int i = 0; i < ntohs(pong_front.entry_size); i++) {
			P2P_pong_entry pong_entry;
			memcpy(&pong_entry, recvbuf + HLEN + 4 + (i * 8), 8);
			printf("IP Address of %d entry = %u.%u.%u.%u with port = %u\n\n",
				i,
				(unsigned int)pong_entry.ip.S_un.S_un_b.s_b1,
				(unsigned int)pong_entry.ip.S_un.S_un_b.s_b2,
				(unsigned int)pong_entry.ip.S_un.S_un_b.s_b3,
				(unsigned int)pong_entry.ip.S_un.S_un_b.s_b4,
				ntohs(pong_entry.port));


			char ip[15];
			sprintf_s(ip, "%u.%u.%u.%u",
				(unsigned int)pong_entry.ip.S_un.S_un_b.s_b1,
				(unsigned int)pong_entry.ip.S_un.S_un_b.s_b2,
				(unsigned int)pong_entry.ip.S_un.S_un_b.s_b3,
				(unsigned int)pong_entry.ip.S_un.S_un_b.s_b4);

			char port[10];
			sprintf_s(port, "%u", (unsigned int)ntohs(pong_entry.port));

			bool exists = false;
			struct in_addr ip_addrr;
			ip_addrr.s_addr = MY_IP;


			for (int j = 0; j < activeNeighbours; j++) {
				printf("%s vs %s\n", neighbourArray[j].rec_ip, ip);
				if (strcmp(neighbourArray[j].rec_ip, ip) == 0) {
					exists = true;
				}
			}
			if (!exists /*&& strstr(ip, "130.233") > 0 /*&& strcmp(inet_ntoa(ip_addrr), ip) != 0*/) {
				extend_network(ip, port, activeNeighbours);
			}
		}
	}
}

void send_PongBMessage(P2P_h header, int socketNo) {
	P2P_h pongHeader = getPongBHeader(header.msg_id);
	P2P_pong_front pongFront;
	uint16_t size = activeNeighbours > PONGB_MAX_ENTRY ? PONGB_MAX_ENTRY : activeNeighbours;
	pongFront.entry_size = htons(size);
	pongFront.sbz = htons(0);
	//Create buffer that can hold all.
	char combined[HLEN + sizeof(P2P_pong_front) + PONGB_MAX_ENTRY *sizeof(P2P_hit_entry)];
	memcpy(combined, &pongHeader, HLEN);
	memcpy(combined + HLEN, &pongFront, sizeof(pongFront));
	for (int i = 0; i < size; i++) {
		P2P_pong_entry pongEntry;
		DWORD ip = inet_addr(neighbourArray[i].rec_ip);
		struct in_addr paddr;
		paddr.S_un.S_addr = ip;
		pongEntry.ip = paddr;

		if (ip != header.org_ip) {
			uint16_t port;
			sscanf(neighbourArray[i].rec_port, "%d", &port);
			pongEntry.port = htons(port);
			pongEntry.sbz = htons(0);
			memcpy(combined + HLEN + sizeof(P2P_pong_front) + i * sizeof(P2P_pong_entry), &pongEntry, sizeof(P2P_pong_entry));
		}
	}
	int iResult = send(client_socket[socketNo], combined, sizeof(combined), 0);
	if (iResult == SOCKET_ERROR) {
		printf("send failed with error: %d\n", WSAGetLastError());
		closesocket(client_socket[socketNo]);
		WSACleanup();
		return;
	}
	else {
		printf("SENT PONG B to: %d. No of bytes sent: %d\n\n", socketNo ,iResult);
	}

}


void forwardQuery(P2P_h header, char * recvbuf) {

	uint32_t ip = header.org_ip;
	struct in_addr ip_addr;
	ip_addr.s_addr = ip;

	char port[15];
	sprintf_s(port, "%u", ntohs(header.org_port));

	header.ttl--;
	header.org_ip = htonl(MY_IP);
	header.org_port = htons(MY_PORT);
	char combined[HLEN + OUR_SEARCH_CRITERIA_LENGTH];
	memcpy(combined, &header, HLEN);
	const int x = ntohs(header.length);
	char query_key2[OUR_SEARCH_CRITERIA_LENGTH];
	memcpy(query_key2, recvbuf + HLEN, OUR_SEARCH_CRITERIA_LENGTH);
	query_key2[sizeof(query_key2) - 1] = '\0';
	memcpy(combined + HLEN, query_key2, OUR_SEARCH_CRITERIA_LENGTH);
	printf("Looking for %s\n", query_key2);

	for (int i = 0; i < activeNeighbours; i++) {
		//TODO eventually check if same ip and diffferent port?
		if (strcmp(neighbourArray[i].rec_ip, inet_ntoa(ip_addr)) != 0) {
			printf("Forwarding query to: %s \n", neighbourArray[i].rec_ip);
			int iResult = send(client_socket[i], combined, sizeof(combined), 0);
			if (iResult == SOCKET_ERROR) {
				printf("send failed with error: %d\n", WSAGetLastError());
				closesocket(client_socket[i]);
				WSACleanup();
				return;
			}
			else {
				printf("Bytes sent from query: %d\n\n", iResult);
			}
		}
	}

}

void checkQueryHit(char *recvbuf, P2P_h received_header, int socketNo) {

	int length = ntohs(received_header.length);
	P2P_query query;

	memcpy(&query, recvbuf + HLEN, length);
	char my_key[11] = "rm1testkey";

	char subbuff[11];
	memcpy(subbuff, &query.search_criteria, 10);
	subbuff[10] = '\0';

	if (strcmp(subbuff, my_key) == 0) {
		printf("indeed for this key he asked\n");
		P2P_h queryHit = getQueryHitMessageHeader(received_header.msg_id);

		P2P_hit_front hit_front;
		hit_front.entry_size = htons((uint16_t)1);
		hit_front.sbz = htons((uint16_t)0);

		P2P_hit_entry hit_entry;
		uint16_t resourceId;
		memcpy(&resourceId, subbuff, sizeof(uint16_t));
		hit_entry.resourceId = htons(resourceId);
		hit_entry.resourceValue = htonl(MYVALUE);
		hit_entry.sbz = 0;

		//Create buffer that can hold all.
		char combined[HLEN + sizeof(P2P_hit_front) + sizeof(P2P_hit_entry)];
		//Copy arrays in individually.
		memcpy(combined, &queryHit, HLEN);
		memcpy(combined + HLEN, &hit_front, sizeof(P2P_hit_front));
		memcpy(combined + HLEN + sizeof(P2P_hit_front), &hit_entry, sizeof(P2P_hit_entry));
		int iResult = send(client_socket[socketNo], combined, sizeof(combined), 0);
		if (iResult == SOCKET_ERROR) {
			printf("send failed with error: %d\n", WSAGetLastError());
			closesocket(client_socket[socketNo]);
			WSACleanup();
			return;
		}
		else {
			printf("Bytes sent from query: %d\n\n", iResult);
		}
	}
	else {
		printf("No HIT here, forwarding....\n");
	}
}
void sendJoinResponse(uint32_t msg_id, int socketNo) {
	P2P_h joinResponse = getJoinResponseHeader(msg_id);
	P2P_join join_responseBody;
	join_responseBody.status = htons(JOIN_ACC);

	//Create buffer that can hold both.
	char combined[HLEN + JOINLEN];

	//Copy arrays in individually.
	memcpy(combined, &joinResponse, HLEN);
	memcpy(combined + HLEN, &join_responseBody, JOINLEN);

	int iResult2 = send(client_socket[socketNo], combined, sizeof(combined), 0);
	if (iResult2 == SOCKET_ERROR) {
		printf("send failed with error: %d\n", WSAGetLastError());
		closesocket(client_socket[socketNo]);
		WSACleanup();
		return;
	}
	else {
		printf("A have accepted a new peer!: %d\n\n", iResult2);
	}

}
void forwardQueryHit(char * recvbuf, P2P_h header) {
	//Create buffer that can hold all.
	bool found = false;
	int socketNo;
	for (int i = 0; i < recentQueryNo; i++) {
		if (header.msg_id == queries[i].msg_id) {
			found = true;
			socketNo = queries[i].socketNo;
		}
	}
	if (found) {
		P2P_h hitHeader = getQueryHitMessageHeader(header.msg_id);
		P2P_hit_front hit_front;
		memcpy(&hit_front, recvbuf + HLEN, sizeof(P2P_hit_front));

		// Create buffer that can hold all.
		char combined[HLEN + sizeof(P2P_hit_front) + MAX_HIT_ENTRY * sizeof(P2P_hit_entry)];
		memcpy(combined, &hitHeader, HLEN);
		memcpy(combined + HLEN, &hit_front, sizeof(P2P_hit_front));

		for (int i = 0; i < ntohs(hit_front.entry_size);i++) {
			P2P_hit_entry hit_entry;
			memcpy(&hit_entry, recvbuf + HLEN + sizeof(P2P_hit_front), sizeof(P2P_hit_entry));

			printf("Forwarded Resource Value: 0x%x \n\n", ntohl(hit_entry.resourceValue));
			
			//add hit entry to buffer
			memcpy(combined + HLEN + sizeof(P2P_hit_front)+ i * sizeof(P2P_hit_entry), &hit_entry, sizeof(P2P_hit_entry));
		}

		int iResult = send(client_socket[socketNo], combined, sizeof(combined), 0);
		if (iResult == SOCKET_ERROR) {
			printf("send failed with error: %d\n", WSAGetLastError());
			closesocket(client_socket[socketNo]);
			WSACleanup();
			return;
		}
		else {
			printf("Bytes sent from query: %d\n\n", iResult);
		}
	}
}
void process_receive(char* recvbuf, int socketNo) {

	P2P_h received_header;
	int iResult[TOTAL_POSSIBLE_NEIGHBOURS];
	char querybuf[64];
	memcpy(&received_header, recvbuf, HLEN);

	if (isGunutellaPackage(received_header)) {

		uint32_t ip = received_header.org_ip;
		struct in_addr ip_addr;
		ip_addr.s_addr = ip;

		char port[15];
		sprintf_s(port, "%u", ntohs(received_header.org_port));

		switch (received_header.msg_type) {

		case MSG_JOIN:
			if (htons(received_header.length) == JOINLEN) {
				printf("A join response was received from %s on port = %s.\n", inet_ntoa(ip_addr), port);
				if (processJoinResponseBody(recvbuf)) {
					if (shouldSendQuery) {

						query_key[sizeof(query_key) - 1] = '\0';
						sendQuery(socketNo, query_key);
						shouldSendQuery = false;
					}
				}
			}
			else {
				printf("A join request was received from %s on port = %s.\n", inet_ntoa(ip_addr), port);
				sendJoinResponse(ntohl(received_header.msg_id), socketNo);
			}
			strcpy(neighbourArray[activeNeighbours].rec_ip, inet_ntoa(ip_addr));
			strcpy(neighbourArray[activeNeighbours].rec_port, port);
			activeNeighbours = activeNeighbours + 1;
			for (int j = 0; j < activeNeighbours; j++) {
				printf("%s -- %s\n", neighbourArray[j].rec_ip, neighbourArray[j].rec_port);
			}
			sendTypeBPingMessage();
			break;

		case MSG_QHIT:
			
			if (ntohl(received_header.msg_id == msg_id)) {
				printQueryResult(socketNo, recvbuf);
			}
			else {
				
				printf("Received query hit after having forwarded\n");
				forwardQueryHit(recvbuf, received_header);
			}
			break;

		case MSG_QUERY:
			printf("%s on port = %s asked for some data.\n", inet_ntoa(ip_addr), port);
			checkQueryHit(recvbuf, received_header, socketNo);
			if (received_header.ttl > 1) {
				forwardQuery(received_header, recvbuf);
				queries[recentQueryNo].msg_id = received_header.msg_id;
				queries[recentQueryNo].socketNo = socketNo;
				recentQueryNo++;
			}
			break;

		case MSG_PING:
			if (received_header.ttl == 1) {
				printf("PING TYPE A received from %s on port = %s\n", inet_ntoa(ip_addr), port);
				P2P_h pongA = getPongTypeAMessage(htonl(received_header.msg_id));
				send_PongAMessage(pongA, socketNo);
			}
			if (received_header.ttl > 1) {
				printf("PING TYPE B received from %s on port = %s\n", inet_ntoa(ip_addr), port);
				send_PongBMessage(received_header, socketNo);
			}
			break;

		case MSG_PONG:
			handlePongResponse(socketNo, received_header, recvbuf);
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

int main(int argc, char *argv[])
{
	WSADATA wsa;
	struct sockaddr_in server, address;
	int max_clients = 30, activity, addrlen, i, valread;

	//size of our receive buffer, this is string length.
	int MAXRECV = 1024;
	//set of socket descriptors
	fd_set readfds;
	//1 extra for null character, string termination
	char *buffer;
	buffer = (char*)malloc((MAXRECV + 1) * sizeof(char));

	for (i = 0; i < TOTAL_POSSIBLE_NEIGHBOURS; i++)
	{
		client_socket[i] = 0;
	}

	printf("\nInitialising Winsock...");
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		printf("Failed. Error Code : %d", WSAGetLastError());
		exit(EXIT_FAILURE);
	}

	printf("Initialised.\n");

	//Create a socket
	if ((master = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
	{
		printf("Could not create socket : %d", WSAGetLastError());
		exit(EXIT_FAILURE);
	}

	printf("Socket created.\n");

	//Prepare the sockaddr_in structure
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(MY_PORT);

	//Bind
	if (bind(master, (struct sockaddr *)&server, sizeof(server)) == SOCKET_ERROR)
	{
		printf("Bind failed with error code : %d", WSAGetLastError());
		exit(EXIT_FAILURE);
	}

	puts("Bind done");

	//Listen to incoming connections
	listen(master, 3);


	//Accept and incoming connection
	puts("Waiting for incoming connections...\n\n");

	addrlen = sizeof(struct sockaddr_in);


	
	extend_network(bootstrap_ip, port, activeNeighbours);
	myTimer(hTimerQueue);

	while (TRUE)
	{
		//clear the socket fd set
		FD_ZERO(&readfds);
		//add master socket to fd set
		FD_SET(master, &readfds);
		//add child sockets to fd set
		for (i = 0; i < 5; i++)
		{
			s = client_socket[i];
			if (s > 0)
			{
				FD_SET(s, &readfds);
			}
		}

		//wait for an activity on any of the sockets, timeout is NULL , so wait indefinitely
		activity = select(0, &readfds, NULL, NULL, NULL);

		if (activity == SOCKET_ERROR)
		{
			printf("[activity] select call failed with error code : %d", WSAGetLastError());
			getchar();
			exit(EXIT_FAILURE);
		}

		//If something happened on the master socket , then its an incoming connection
		if (FD_ISSET(master, &readfds))
		{
			if ((new_socket = accept(master, (struct sockaddr *)&address, (int *)&addrlen)) < 0)
			{
				perror("accept");
				getchar();
				exit(EXIT_FAILURE);
			}

			//inform user of socket number - used in send and receive commands
			printf("New connection , socket fd is %d , ip is : %s , port : %d \n", new_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));

			//add new socket to array of sockets
			for (i = 0; i < max_clients; i++)
			{
				if (client_socket[i] == 0)
				{
					client_socket[i] = new_socket;
					printf("Adding to list of sockets at index %d \n", i);
					break;
				}
			}
		}

		//else its some IO operation on some other socket :)
		for (i = 0; i < max_clients; i++)
		{
			s = client_socket[i];
			//if client presend in read sockets             
			if (FD_ISSET(s, &readfds))
			{
				//get details of the client
				getpeername(s, (struct sockaddr*)&address, (int*)&addrlen);

				//Check if it was for closing , and also read the incoming message
				//recv does not place a null terminator at the end of the string (whilst printf %s assumes there is one).
				valread = recv(s, buffer, MAXRECV, 0);

				if (valread == SOCKET_ERROR)
				{
					int error_code = WSAGetLastError();
					if (error_code == WSAECONNRESET)
					{
						//Somebody disconnected , get his details and print
						printf("Host disconnected unexpectedly , ip %s , port %d \n", inet_ntoa(address.sin_addr), ntohs(address.sin_port));

						//Close the socket and mark as 0 in list for reuse
						closesocket(s);
						client_socket[i] = 0;
					}
					else
					{
						printf("recv failed with error code : %d", error_code);
					}
				}
				if (valread == 0)
				{
					//Somebody disconnected , get his details and print
					printf("Host disconnected , ip %s , port %d \n", inet_ntoa(address.sin_addr), ntohs(address.sin_port));

					//Close the socket and mark as 0 in list for reuse
					closesocket(s);
					client_socket[i] = 0;
				}

				//Echo back the message that came in
				else
				{
					//add null character, if you want to use with printf/puts or other string handling functions
					process_receive(buffer, i);
				}
			}
		}
	}

	if (WaitForSingleObject(gDoneEvent, INFINITE) != WAIT_OBJECT_0)
		printf("WaitForSingleObject failed (%d)\n", GetLastError());

	closesocket(s);
	WSACleanup();

	getchar();

	return 0;
}
