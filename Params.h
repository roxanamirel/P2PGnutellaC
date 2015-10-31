#pragma once
#include <stdint.h>


#define MY_PORT    6161
const uint32_t MY_IP = 3232235797;
char query_key[11] = "rm1testkey";
bool shouldSendQuery = false;
char *bootstrap_ip = "130.233.195.32";
char *port = "6346";

const int OUR_SEARCH_CRITERIA_LENGTH = 11;
const int MAX_HIT_ENTRY = 5;
const int TOTAL_POSSIBLE_NEIGHBOURS = 30;
const int PONGB_MAX_ENTRY = 5;