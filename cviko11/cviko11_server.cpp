/* serv.cpp  -  Minimal ssleay server for Unix
   30.9.1996, Sampo Kellomaki <sampo@iki.fi> */


/* mangled to work with SSLeay-0.9.0b and OpenSSL 0.9.2b
   Simplified to be even more minimal
   12/98 - 4/99 Wade Scholine <wades@mail.cybg.com> */

#include <cstdio>
#include <unistd.h>
#include <cstdlib>
#include <memory.h>
#include <cerrno>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <vector>
#include <string>

#include <openssl/rsa.h>       /* SSLeay stuff */
#include <openssl/crypto.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/err.h>


/* define HOME to be dir for key and cert files... */
#define HOME "./"
/* Make these what you want for cert & key files */
#define CERTF  HOME "myserver.crt"
#define KEYF  HOME  "mypriv.pem"


#define CHK_NULL(x) if ((x)==NULL) exit (1)
#define CHK_ERR(err,s) if ((err)==-1) { perror(s); exit(1); }
#define CHK_SSL(err) if ((err)==-1) { ERR_print_errors_fp(stderr); exit(2); }

struct Client
{
    pollfd poll{};
    SSL* ssl = nullptr;
};

void get_peer_addr(int fd, char* host, unsigned short& port)
{
  sockaddr_in address{};
  uint address_size = sizeof(address);

  int result = getpeername(fd, (sockaddr*) &address, &address_size);
  strcpy(host, inet_ntoa(address.sin_addr));
  port = ntohs(address.sin_port);
}

std::string clientName(int client)
{
  char clientHost[256];
  unsigned short clientPort;
  get_peer_addr(client, clientHost, clientPort);

  return std::string(clientHost) + ":" + std::to_string(clientPort);
}

int chat_send_all(int clientIndex, const std::vector<Client>& clients)
{
  char buffer[256];
  ssize_t len = SSL_read(clients[clientIndex].ssl, buffer, sizeof(buffer) - 1);
  if (len <= 0) return -1;
  CHK_SSL(len);
  buffer[len] = '\0';

  auto name = clientName(clients[clientIndex].poll.fd);
  printf("%s sends %s", name.c_str(), buffer);

  bool close = std::string(buffer).find("close") != std::string::npos;

  std::string msg = name;
  msg += " says: ";
  msg += buffer;

  for (int i = 1; i < clients.size(); i++)
  {
    auto remote = clients[i];
    if (close || clients[clientIndex].poll.fd != remote.poll.fd)
    {
      CHK_SSL(SSL_write(remote.ssl, msg.c_str(), msg.size()));
    }
  }

  return 0;
}

void server_body_poll(sockaddr_in addr, int fd, SSL_CTX* ctx)
{
  std::vector<Client> clients;
  clients.emplace_back();
  clients.back().poll.fd = fd;
  clients.back().poll.events = POLLIN;

  socklen_t len;
  while (true)
  {
    std::vector<pollfd> pollfds;
    for (auto& c: clients)
    {
      pollfds.push_back(c.poll);
    }

    int ret = poll(pollfds.data(), clients.size(), 1000);
    if (ret < 0)
    {
      printf("Server error\n");
      return;
    }
    else if (ret == 0) continue;

    if ((pollfds[0].revents & POLLIN) != 0)
    {
      int client = accept(fd, (sockaddr*) &addr, &len);
      if (client < 0)
      {
        perror("accept");
        return;
      }

      SSL* ssl = SSL_new(ctx);
      CHK_NULL(ssl);
      CHK_SSL(SSL_set_fd(ssl, client));
      CHK_SSL(SSL_accept(ssl));

      /* Get the cipher - opt */
      printf("SSL connection using %s\n", SSL_get_cipher (ssl));
      printf("client connected\n");

      clients.emplace_back();
      clients.back().poll.fd = client;
      clients.back().poll.events = POLLIN | POLLHUP;
      clients.back().ssl = ssl;
      continue;
    }

    std::vector<Client> nextClients = { clients[0] };
    for (int i = 1; i < clients.size(); i++)
    {
      auto& client = clients[i];
      if ((pollfds[i].revents & POLLIN) != 0)
      {
        if (chat_send_all(i, clients) < 0)
        {
          printf("client disconnected\n");
          SSL_free(client.ssl);
          close(pollfds[i].fd);
        }
        else if ((pollfds[i].revents & POLLHUP) == 0)
        {
          nextClients.push_back(client);
        }
      }
      else if ((pollfds[i].revents & POLLHUP) == 0)
      {
        nextClients.push_back(client);
      }
      else
      {
        printf("Client disconnected\n");
        SSL_free(client.ssl);
        close(pollfds[i].fd);
      }
    }
    clients = std::move(nextClients);
  }
}

int main()
{
  int err;
  int listen_sd;
  int sd;
  struct sockaddr_in sa_serv;
  struct sockaddr_in sa_cli;
  socklen_t client_len;
  SSL_CTX* ctx;
  SSL*     ssl;
  X509*    client_cert;
  char*    str;
  char     buf [4096];
  const SSL_METHOD *meth;
  
  /* SSL preliminaries. We keep the certificate and key with the context. */

  SSL_load_error_strings();
  SSLeay_add_ssl_algorithms();
  meth = SSLv23_server_method();
  ctx = SSL_CTX_new (meth);
  if (!ctx) {
    ERR_print_errors_fp(stderr);
    exit(2);
  }
  
  if (SSL_CTX_use_certificate_file(ctx, CERTF, SSL_FILETYPE_PEM) <= 0) {
    ERR_print_errors_fp(stderr);
    exit(3);
  }
  if (SSL_CTX_use_PrivateKey_file(ctx, KEYF, SSL_FILETYPE_PEM) <= 0) {
    ERR_print_errors_fp(stderr);
    exit(4);
  }

  if (!SSL_CTX_check_private_key(ctx)) {
    fprintf(stderr,"Private key does not match the certificate public key\n");
    exit(5);
  }

  /* ----------------------------------------------- */
  /* Prepare TCP socket for receiving connections */

  listen_sd = socket (AF_INET, SOCK_STREAM, 0);   CHK_ERR(listen_sd, "socket");
  
  memset (&sa_serv, '\0', sizeof(sa_serv));
  sa_serv.sin_family      = AF_INET;
  sa_serv.sin_addr.s_addr = INADDR_ANY;
  sa_serv.sin_port        = htons (1111);          /* Server Port number */
  
  err = bind(listen_sd, (struct sockaddr*) &sa_serv,
	     sizeof (sa_serv));                   CHK_ERR(err, "bind");
	     
  /* Receive a TCP connection. */
	     
  err = listen (listen_sd, 5);                    CHK_ERR(err, "listen");
  
  server_body_poll(sa_serv, listen_sd, ctx);
  SSL_CTX_free(ctx);
}
/* EOF - serv.cpp */
