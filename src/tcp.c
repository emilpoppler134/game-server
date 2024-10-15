#include "include/server.h"

tcp_response_package_t init_response(tcp_response_type_e type, data_u data)
{
  tcp_response_package_t package = {0};
  package.type = type;
  package.data = data;
  return package;
}

void* disconnect_timer_thread(void* arg) {
  client_t* client = (client_t*)arg;

  sleep(30);

  if (client->connected == 0)
  {
    memset(client, 0, sizeof(client_t));
  }

  pthread_exit(NULL);
}

void *tcp_server(void *arg)
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

  // SETUP TCP SERVER
  // __________________________________________________________
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));

  // Configuration
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  // Setup bind address
  struct addrinfo *bind_address;
  if (getaddrinfo(0, "8100", &hints, &bind_address) != 0)
  {
    perror("Error on getaddrinfo");
    pthread_exit(NULL);
  }

  // Setup socket
  SOCKET socket_listen;
  socket_listen = socket(bind_address->ai_family, bind_address->ai_socktype, bind_address->ai_protocol);
  if (!ISVALIDSOCKET(socket_listen))
  {
    perror("Socket invalid");
    pthread_exit(NULL);
  }

  // Setting the socket options
  int option_value = 1;
  if (setsockopt(socket_listen, SOL_SOCKET, SO_REUSEADDR, &option_value, sizeof(int)) == -1)
  {
      perror("Error on setsockopt");
      pthread_exit(NULL);
  }

  // Binding the socket to the address
  if (bind(socket_listen, bind_address->ai_addr, bind_address->ai_addrlen))
  {
    perror("Error on bind");
    pthread_exit(NULL);
  }

  // Cleaning up
  freeaddrinfo(bind_address);

  // Listen to the socket
  if (listen(socket_listen, 10) < 0)
  {
    perror("Error on listen");
    pthread_exit(NULL);
  }

  // Ready message
  printf("TCP Server ready.\n");

  // Stores all active sockets
  fd_set master;
  FD_ZERO(&master);
  FD_SET(socket_listen, &master);
  SOCKET max_socket = socket_listen;

  // MAIN TCP SERVER LOOP
  // __________________________________________________________
  while (1)
  {
    fd_set reads;
    reads = master;

    // Select all sockets
    if (select(max_socket + 1, &reads, 0, 0, 0) < 0)
    {
      perror("Error on select");
      pthread_exit(NULL);
    }

    // Loop through the sockets
    for (SOCKET i = 1; i <= max_socket; i++)
    {
      if (FD_ISSET(i, &reads))
      {
        if (i == socket_listen)
        {
          // NEW CONNECTION
          // __________________________________________________________
          struct sockaddr_storage client_address;
          socklen_t client_len = sizeof(client_address);

          // Accepts a connection
          SOCKET client_socket = accept(i, (struct sockaddr*)&client_address, &client_len);

          // Error handling
          if (!ISVALIDSOCKET(client_socket))
          {
            perror("Error on accept");
            pthread_exit(NULL);
          }

          // Saves the new connection in master
          FD_SET(client_socket, &master);

          // Maintains max sockets
          if (client_socket > max_socket)
          {
            max_socket = client_socket;
          }

          // Gets the client inet address
          char inet_address[INET_ADDRSTRLEN];
          getnameinfo((struct sockaddr*)&client_address, client_len, inet_address, sizeof(inet_address), 0, 0, NI_NUMERICHOST);

          printf("-> TCP Client connected on socket %d from %s\n", client_socket, inet_address);
        }
        else
        {
          // ALREADY ESTABLISHED CONNECTION
          // __________________________________________________________
          SOCKET client_socket = i;

          tcp_request_package_t request = {0};
          char buffer[1024];
          int bytes_received = recv(client_socket, buffer, 1024, 0);

          // If the connection is closed
          if (bytes_received < 1)
          {
            for (int j = 0; j < MAX_CLIENTS; j++)
            {
              if (server_state->clients[j].tcp_socket == client_socket)
              {
                server_state->clients[j].tcp_socket = 0;
                server_state->clients[j].connected = 0;

                // Create a timer thread to disconnect the client
                // (if it doesn't identify itself in 30 seconds)
                pthread_t tid;
                if (pthread_create(&tid, NULL, disconnect_timer_thread, &server_state->clients[j]) != 0) {
                  perror("Error creating thread");
                }
                break;
              }
            }

            // Closes the connection and clears the file descriptor
            FD_CLR(client_socket, &master);
            CLOSESOCKET(client_socket);
            printf("-> TCP Client %d diconnected\n", client_socket);
            continue;
          }

          // Copies the buffer to the request struct
          memcpy(&request, buffer, sizeof(tcp_request_package_t));

          // Handles the request based on the type
          switch(request.type)
          {
            case PACKAGE_IDENTIFY:
            {
              int mac_address_in_use = 0;
              int id = -1;

              // Check if a client with the same MAC address is already connected
              for (int j = 0; j < MAX_CLIENTS; j++)
              {
                if (strcmp(server_state->clients[j].mac_address, request.payload.mac_address) == 0)
                {
                  if (server_state->clients[j].connected == 0)
                  {
                    server_state->clients[j].tcp_socket = client_socket;
                    server_state->clients[j].connected = 1;
                    id = j;
                    break;
                  }
                  else
                  {
                    mac_address_in_use = 1;
                  }
                }
              }

              // If no client with the same MAC address is found, find an available slot
              if (id == -1)
              {
                for (int j = 0; j < MAX_CLIENTS; j++)
                {
                  if (server_state->clients[j].occupied == 0)
                  {
                    server_state->clients[j].tcp_socket = client_socket;
                    server_state->clients[j].connected = 1;
                    server_state->clients[j].occupied = 1;
                    strcpy(server_state->clients[j].mac_address, request.payload.mac_address);
                    id = j;
                    break;
                  }
                }
              }

              // If no available slot is found, send an error response
              if (id == -1 || mac_address_in_use == 1)
              {
                tcp_response_package_t response = init_response(ERROR, (data_u){0});
                ssize_t bytes_sent = send(client_socket, &response, sizeof(tcp_response_package_t), 0);
                if (bytes_sent < 0)
                {
                  perror("Error in send");
                  close(client_socket);
                }
                continue;
              }

              // Send a success response with the id
              tcp_response_package_t response = init_response(OK, (data_u){.id = id});
              ssize_t bytes_sent = send(client_socket, &response, sizeof(tcp_response_package_t), 0);
              if (bytes_sent < 0)
              {
                perror("Error in send");
                close(client_socket);
              }
            }
            break;

            // case 2:
            // {
            //   time();
            //   // Väder klocka
            //   // varje 5e minut
              
            // }
            // break;
            
            // case 3:
            // {
              
            // }
            // break;

            default:
              break;
          }
        }
      }
    }
  }

  // Closes socket
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
