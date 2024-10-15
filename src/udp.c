#include "include/server.h"

void *udp_server(void *arg)
{
  server_state_t* server_state = (server_state_t*)arg;

  // Setup dependencies for win
  // __________________________________________________________
  #if defined(_WIN32)
    WSADATA d;
    if (WSAStartup(MAKEWORD(2, 2), &d))
    {
      fprintf(stderr, "Failed to initialize.\n");
      pthread_exit(NULL);
    }
  #endif

  // Program
  // __________________________________________________________
  printf("Configuring local address...\n");

  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));

  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags = AI_PASSIVE;

  struct addrinfo *bind_address;
  if (getaddrinfo(0, "8200", &hints, &bind_address) != 0)
  {
    perror("Error on getaddrinfo");
    pthread_exit(NULL);
  }

  printf("Creating socket...\n");
  SOCKET socket_listen = socket(bind_address->ai_family, bind_address->ai_socktype, bind_address->ai_protocol);

  if (!ISVALIDSOCKET(socket_listen))
  {
    fprintf(stderr, "socket() failed. (%d)\n", GETSOCKETERRNO());
    pthread_exit(NULL);
  }

  printf("Binding socket to local address...\n");

  if (bind(socket_listen, bind_address->ai_addr, bind_address->ai_addrlen))
  {
    fprintf(stderr, "bind() failed. (%d)\n", GETSOCKETERRNO());
    pthread_exit(NULL);
  }
  freeaddrinfo(bind_address);

  fd_set master;
  FD_ZERO(&master);
  FD_SET(socket_listen, &master);
  SOCKET max_socket = socket_listen;

  printf("Waiting for connections...\n");

  while (1)
  {
    fd_set reads;
    reads = master;

    if (select(max_socket + 1, &reads, 0, 0, 0) < 0)
    {
      fprintf(stderr, "select() failed. (%d)\n", GETSOCKETERRNO());
      pthread_exit(NULL);
    }

    if (FD_ISSET(socket_listen, &reads))
    {
      struct sockaddr_storage client_address;
      socklen_t client_len = sizeof(client_address);

      udp_request_package_t request = {0};
      size_t size = sizeof(udp_request_package_t);
      char buffer[size];

      int bytes_received = recvfrom(socket_listen, buffer, sizeof(buffer), 0, (struct sockaddr *)&client_address, &client_len);
      if (bytes_received < 1)
      {
        fprintf(stderr, "connection closed. (%d)\n", GETSOCKETERRNO());
        pthread_exit(NULL);
      }

      memcpy(&request, buffer, size);
      memcpy(&server_state->clients[request.id].position, &request.player.position, sizeof(vec_2));
      
      char buffer2[sizeof(udp_response_package_t)] = {0};
      udp_response_package_t response = {0};
      memcpy(&response.clients, &server_state->clients, sizeof(response.clients));
      memcpy(&buffer2, &response, sizeof(udp_response_package_t));

      sendto(socket_listen, buffer2, sizeof(buffer2), 0, (struct sockaddr *)&client_address, client_len);
    }
  }

  printf("Closing listening socket...\n");
  CLOSESOCKET(socket_listen);

  // Cleanup dependencies for win
  // __________________________________________________________
  #if defined(_WIN32)
    WSACleanup();
  #endif

  printf("Finished.\n");
  pthread_exit(NULL);
}
