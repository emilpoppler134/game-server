#ifndef __SERVER_H__
#define __SERVER_H__

// Dependencies
// __________________________________________________________
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <pthread.h>


// Networking
// __________________________________________________________
#if defined(_WIN32)
  // For win based system
  #ifndef _WIN32_WINNT
  #define _WIN32_WINNT 0x0600
  #endif
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "ws2_32.lib")
#else
  // For unix based system
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <netdb.h>
  #include <unistd.h>
  #include <errno.h>
#endif

// Socket in windows returns a unsingde int and in onix system it returns a int
#if !defined(_WIN32)
  #define SOCKET int
#endif

// Socket returns INVALID_SOCKET if it fails on windos and negativ number on Onix. But SOCKET is an unsignid int on windows
#if defined(_WIN32)
  #define ISVALIDSOCKET(s) ((s) != INVALID_SOCKET)
#else
  #define ISVALIDSOCKET(s) ((s) >= 0)
#endif

// Windows need a special funtion to close sockets
#if defined(_WIN32)
  #define CLOSESOCKET(s) closesocket(s)
#else
  #define CLOSESOCKET(s) close(s)
#endif

// On windows you retrive errors with the WSAGetLastError fuktion and on Onix errno
#if defined(_WIN32)
  #define GETSOCKETERRNO() (WSAGetLastError())
#else
  #define GETSOCKETERRNO() (errno)
#endif

// This is needed on windos to indicate if a terminal input is wating
#if defined(_WIN32)
  #include <conio.h>
#endif

// SERVER STATE
// _____________________________________________________________________________
#define MAC_ADDRSTRLEN 18
#define MAX_CLIENTS 16

typedef struct vec_2
{
  float x;
  float y;
} vec_2;

typedef struct client_t
{
  char mac_address[MAC_ADDRSTRLEN];
  int tcp_socket;
  int connected;
  int occupied;
  vec_2 position;
} client_t;

typedef struct server_state_t
{
  client_t clients[MAX_CLIENTS];
} server_state_t;


// UDP PACKAGES
// _____________________________________________________________________________
typedef struct player_t
{
  vec_2 position;
} player_t;

typedef struct udp_request_package_t
{
  int id;
  player_t player;
} udp_request_package_t;

typedef struct udp_response_package_t
{
  client_t clients[MAX_CLIENTS];
} udp_response_package_t;


// TCP PACKAGES
// _____________________________________________________________________________
typedef enum tcp_request_type_e
{
  PACKAGE_IDENTIFY,
} tcp_request_type_e;

typedef enum tcp_response_type_e
{
  OK = 0,
  ERROR = -1,
} tcp_response_type_e;

typedef union payload_u
{
  char mac_address[MAC_ADDRSTRLEN];
} payload_u;

typedef union data_u
{
  int id;
} data_u;

typedef struct tcp_request_package_t
{
  tcp_request_type_e type;
  payload_u payload;
} tcp_request_package_t;

typedef struct tcp_response_package_t
{
  tcp_response_type_e type;
  data_u data;
} tcp_response_package_t;


// METHODS
// _____________________________________________________________________________
void *udp_server(void *arg);
void *tcp_server(void *arg);

#endif
