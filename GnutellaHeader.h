#include "p2p.h" 

#pragma once

boolean isGunutellaPackage(P2P_h received_header) {
	return received_header.version == P_VERSION && received_header.ttl > 0 && received_header.ttl <= MAX_TTL;
}

unsigned int getUniqueMessageId() {

	SYSTEMTIME t;
	GetSystemTime(&t);
	//TODO add a sequence number
	return (MY_IP + MY_PORT + 5 + t.wHour + t.wMinute + t.wSecond + t.wMilliseconds);
}

P2P_h getJoinReponseMessage(uint32_t ip, uint16_t port) {
	//create a JOIN message
	P2P_h p2pHeader;
	p2pHeader.version = 1;
	p2pHeader.ttl = 1;
	p2pHeader.reserved = 0;
	p2pHeader.org_port = port;
	p2pHeader.org_ip = ip;
	p2pHeader.msg_type = MSG_JOIN;
	p2pHeader.msg_id = htonl(getUniqueMessageId());
	p2pHeader.length = htons(JOINLEN);
	return p2pHeader;
}

P2P_h getJoinRequestMessage() {
	//create a JOIN message
	P2P_h p2pHeader;
	p2pHeader.version = 1;
	p2pHeader.ttl = 3;
	p2pHeader.reserved = 0;
	p2pHeader.org_port = htons(MY_PORT);
	p2pHeader.org_ip = htonl(MY_IP);
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
	p2pHeaderQuery.org_port = htons(MY_PORT);
	p2pHeaderQuery.org_ip = htonl(MY_IP);
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
	p2pHeaderQuery.org_port = htons(MY_PORT);
	p2pHeaderQuery.org_ip = htonl(MY_IP);
	p2pHeaderQuery.msg_type = MSG_QHIT;
	p2pHeaderQuery.msg_id = msg_id;
	p2pHeaderQuery.length = 0;
	return p2pHeaderQuery;
}

P2P_h getPongTypeAMessage(uint32_t msg_id) {
	//create a PONG message
	P2P_h p2pHeader;
	p2pHeader.version = 1;
	p2pHeader.ttl = 1;
	p2pHeader.reserved = 0;
	p2pHeader.org_port = htons(MY_PORT);
	p2pHeader.org_ip = htonl(MY_IP);
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
	p2pHeader.org_port = htons(MY_PORT);
	p2pHeader.org_ip = htonl(MY_IP);
	p2pHeader.msg_type = MSG_PING;
	p2pHeader.msg_id = htonl(getUniqueMessageId());
	p2pHeader.length = htons(0);
	return p2pHeader;

}

P2P_h getJoinResponseHeader(uint32_t msg_id) {
	P2P_h p2pHeader;
	p2pHeader.version = 1;
	p2pHeader.ttl = 1;
	p2pHeader.reserved = 0;
	p2pHeader.org_port = htons(MY_PORT);
	p2pHeader.org_ip = htonl(3232235797);
	p2pHeader.msg_type = MSG_JOIN;
	p2pHeader.msg_id = htonl(msg_id);
	p2pHeader.length = htons(JOINLEN);
	return p2pHeader;
}
P2P_h getPongBHeader(uint32_t msg_id) {
	P2P_h p2pHeader;
	p2pHeader.version = 1;
	p2pHeader.ttl = 1;
	p2pHeader.reserved = 0;
	p2pHeader.org_port = htons(MY_PORT);
	p2pHeader.org_ip = htonl(3232235797);
	p2pHeader.msg_type = MSG_PONG;
	p2pHeader.msg_id = msg_id;
	p2pHeader.length = htons(1);
	return p2pHeader;

}