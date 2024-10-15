#include "include/common.h"
#include "include/server.h"

#include "udp.c"
#include "tcp.c"

// Setup dependencies for win
#if defined(_WIN32)
  WSADATA d;
  if (WSAStartup(MAKEWORD(2, 2), &d))
  {
    fprintf(stderr, "Failed to initialize.\n");
    return 1;
  }
#endif

// Signal handler
static volatile sig_atomic_t server_running = 1;
static void sig_handler(int _)
{
  (void)_;
  server_running = 0;
}

int main()
{
  signal(SIGINT, sig_handler);

  // For controling the frame rate
  const int frames_per_second = 1;
  const int frame_time_us = 1000000 / frames_per_second;

  // Initialize server state
  server_state_t server_state = {0};

  // Createing threads
  pthread_t udp_server_thread;
  pthread_t tcp_server_thread;
  pthread_create(&udp_server_thread, NULL, &udp_server, &server_state);
  pthread_create(&tcp_server_thread, NULL, &tcp_server, &server_state);

  // Main loop
  while (server_running)
  {
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
      printf("Client %d: %.2f, %.2f\n", i, server_state.clients[i].position.x, server_state.clients[i].position.y);
    }
    
    usleep(frame_time_us);
  }

  // Close server
  printf("\nClosing server...\n");
  return EXIT_SUCCESS;
}
