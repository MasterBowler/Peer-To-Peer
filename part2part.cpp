#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <libgen.h>
#define MAXPEERS 10
#define BUFFSIZE 4096

extern int errno;
typedef struct thData
{
  int idThread; //id-ul thread-ului tinut in evidenta de acest program
  int cl;       //descriptorul intors de accept
} thData;

bool serverON;
int serverPORT;
char serverIP[15];

std::vector<std::string> sharedFiles;
char searchfile[100];
struct peer
{
  char IP[15];
  int port;
} peers[MAXPEERS], searchpeers[MAXPEERS];
bool peerOnline[MAXPEERS];

int currentPeersNo;
int searchpeersNo;

void *server(void *da);
void *client(void *da);

void upload(int sockfd, FILE *fd);
void download(int sockfd, FILE *fp);
void checkOnlinePeers();


void logPeerList(const char place[20]);
void logSharedFiles()
{
  for (int i = 0; i < sharedFiles.size(); ++i)
    printf("File %d: %s\n", i, sharedFiles[i].c_str());
}
int fileExists(char* location)
{
    FILE *fd;
    if ((fd = fopen(location, "r")))
    {
        fclose(fd);
        return 1;
    }
    return 0;
}
bool uniquePeer(char IP[15], int Port);
void getIP();
static void *treat(void *);
void raspunde(void *);
pthread_t serv, cli;
int main(int argc, char *argv[])
{
  //client - se ocupa si de UI
  //server - lanseaza mai multe threaduri

  pthread_create(&serv, NULL, server, NULL);
  pthread_create(&cli, NULL, client, NULL);
  pthread_exit(NULL);
};
void *server(void *da)
{
  struct sockaddr_in server;
  struct sockaddr_in from;
  int sd;
  pthread_t th[100];
  int i = 0;
  sd = socket(AF_INET, SOCK_STREAM, 0);
  int on = 1;
  setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
  bzero(&server, sizeof(server));
  bzero(&from, sizeof(from));
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = htonl(INADDR_ANY);
  server.sin_port = htons(0);
  bind(sd, (struct sockaddr *)&server, sizeof(struct sockaddr));
  listen(sd, 2);
  struct sockaddr_in sin;
  socklen_t len = sizeof(sin);
  getsockname(sd, (struct sockaddr *)&sin, &len);
  printf("Port number %d\n", ntohs(sin.sin_port));
  serverPORT = ntohs(sin.sin_port);
  getIP();
  serverON = true;
  while (serverON)
  {
    int client;
    thData *td;
    unsigned int length = sizeof(from);
    //printf("[server]Asteptam.\n");
    fflush(stdout);
    if ((client = accept(sd, (struct sockaddr *)&from, &length)) < 0)
    {
      perror("[server]Eroare la accept().\n");
      continue;
    }
    if (serverON)
    {
      td = (struct thData *)malloc(sizeof(struct thData));
      td->idThread = i++;
      td->cl = client;
      pthread_create(&th[i], NULL, &treat, td);
    }
  }
  pthread_exit(NULL);
}

static void *treat(void *arg)
{
  struct thData tdL;
  tdL = *((struct thData *)arg);
  //printf("[thread]- %d - Asteptam mesajul...\n", tdL.idThread);
  fflush(stdout);
  pthread_detach(pthread_self());
  raspunde((struct thData *)arg);
  close((intptr_t)arg);
  return (NULL);
};

void raspunde(void *arg)
{
  int nr, i = 0;
  int clientPORT;
  char clientIP[15];
  struct thData tdL;
  tdL = *((struct thData *)arg);
  read(tdL.cl, &nr, sizeof(int)); //message type
  //printf("[Thread %d]Mesajul a fost receptionat...%d\n", tdL.idThread, nr);
  read(tdL.cl, &clientIP, sizeof(clientIP));     // peer IP
  read(tdL.cl, &clientPORT, sizeof(clientPORT)); // peer PORT
  if (nr == 1)
  {
    //printf("[Thread %d]Trimitem mesajul inapoi peer list.\n", tdL.idThread);
    write(tdL.cl, &currentPeersNo, sizeof(int));
    write(tdL.cl, &peers, sizeof(peers));
  }
  else if (nr == 2)
  {
    char filename[BUFFSIZE];
    bool onDevice = false;
    int sd;
    struct sockaddr_in server;
    int ttl;
    read(tdL.cl, &filename, sizeof(filename));
    read(tdL.cl, &ttl, sizeof(ttl));
    --ttl;
    for (int i = 0; i < sharedFiles.size(); ++i)
    {
      char *writable = new char[sharedFiles[i].size() + 1];
      std::copy(sharedFiles[i].begin(), sharedFiles[i].end(), writable);
      writable[sharedFiles[i].size()] = '\0';
      //printf("search parse: %s\n", writable);
      if (strcmp(basename(writable), filename) == 0)
      {
        sd = socket(AF_INET, SOCK_STREAM, 0);
        server.sin_family = AF_INET;
        server.sin_addr.s_addr = inet_addr(clientIP);
        server.sin_port = htons(clientPORT);
        connect(sd, (struct sockaddr *)&server, sizeof(struct sockaddr));
        nr = 3;
        onDevice = true;
        write(sd, &nr, sizeof(nr)); // message type = file location

        write(sd, &serverIP, sizeof(serverIP));     // send location IP
        write(sd, &serverPORT, sizeof(serverPORT)); //send location PORT

        write(sd, &filename, sizeof(filename)); //send filename
        i = sharedFiles.size();
      }
      delete[] writable;
    }
    if (!onDevice)
    {
      //forward message to all peers
      for (i = 0; i < currentPeersNo; ++i)
      {
        if (strcmp(clientIP, peers[i].IP) != 0 || clientPORT != peers[i].port)
        {
          sd = socket(AF_INET, SOCK_STREAM, 0);
          server.sin_family = AF_INET;
          server.sin_addr.s_addr = inet_addr(peers[i].IP);
          server.sin_port = htons(peers[i].port);
          connect(sd, (struct sockaddr *)&server, sizeof(struct sockaddr));
          nr = 2;
          write(sd, &nr, sizeof(int)); //message type = search filename

          write(sd, &serverIP, sizeof(clientIP));     //send node IP
          write(sd, &serverPORT, sizeof(clientPORT)); //send node PORT

          write(sd, filename, sizeof(filename)); //send filename

          write(sd, &ttl, sizeof(ttl)); //send ttl
          close(sd);
        }
      }
    }
  }
  else if (nr == 3)
  {
    char filename[BUFFSIZE];
    read(tdL.cl, &filename, sizeof(filename));
    //filelocation is clientIP::clientPORT
    if (strcmp(filename, searchfile) == 0 && searchpeersNo < MAXPEERS)
    {
      strcpy(searchpeers[searchpeersNo].IP, clientIP);
      searchpeers[searchpeersNo++].port = clientPORT;
    }
  }
  else if (nr == 4) //send file
  {
    char filename[BUFFSIZE];
    int sd;
    struct sockaddr_in server;
    read(tdL.cl, &filename, sizeof(filename));
    bool onDevice = false;
    for (int i = 0; i < sharedFiles.size(); ++i)
    {
      char *writable = new char[sharedFiles[i].size() + 1];
      std::copy(sharedFiles[i].begin(), sharedFiles[i].end(), writable);
      writable[sharedFiles[i].size()] = '\0';
      if (strcmp(basename(writable), filename) == 0)
      {

        sd = socket(AF_INET, SOCK_STREAM, 0);
        server.sin_family = AF_INET;
        server.sin_addr.s_addr = inet_addr(clientIP);
        server.sin_port = htons(clientPORT);
        connect(sd, (struct sockaddr *)&server, sizeof(struct sockaddr));
        nr = 5;
        onDevice = true;
        write(sd, &nr, sizeof(nr)); // message type = file location

        write(sd, &serverIP, sizeof(serverIP));     // send location IP
        write(sd, &serverPORT, sizeof(serverPORT)); //send location PORT

        write(sd, &filename, sizeof(filename)); //send filename

        FILE *fd = fopen(sharedFiles[i].c_str(), "rb");
        if (fd == NULL)
        {
          perror("Can't open file");
          exit(1);
        }
        upload(sd, fd);
        printf("Send Success\n");
        fclose(fd);
        close(sd);
        i = sharedFiles.size();
      }
      delete[] writable;
    }
    if (!onDevice)
    {
      sd = socket(AF_INET, SOCK_STREAM, 0);
      server.sin_family = AF_INET;
      server.sin_addr.s_addr = inet_addr(clientIP);
      server.sin_port = htons(clientPORT);
      connect(sd, (struct sockaddr *)&server, sizeof(struct sockaddr));
      nr = 6;
      write(sd, &nr, sizeof(nr)); // message type = file not found

      write(sd, &serverIP, sizeof(serverIP));     // send location IP
      write(sd, &serverPORT, sizeof(serverPORT)); //send location PORT
      close(sd);
    }
  }
  else if (nr == 5) //receive file
  {
    char filename[BUFFSIZE];
    read(tdL.cl, &filename, sizeof(filename));
    FILE *fd = fopen(filename, "wb");
    download(tdL.cl, fd);
    fclose(fd);
    printf("Receive Success\n");
  } else if ( nr == 6)
  {
    printf("Could not get file.\n");
  } else if ( nr == 7)
  {
    //send back 8
    int sd;
    struct sockaddr_in server;
    sd = socket(AF_INET, SOCK_STREAM, 0);
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(clientIP);
    server.sin_port = htons(clientPORT);
    connect(sd, (struct sockaddr *)&server, sizeof(struct sockaddr));
    nr = 8;
    write(sd, &nr, sizeof(nr)); // message type = i'm online

    write(sd, &serverIP, sizeof(serverIP));     // send location IP
    write(sd, &serverPORT, sizeof(serverPORT)); //send location PORT
    close(sd);
  } else if ( nr == 8 )
  {
    //peer online
    int i;
    for ( i=0; i<currentPeersNo; ++i)
    {
        if(strcmp(peers[i].IP,clientIP)==0&&peers[i].port==clientPORT)
          peerOnline[i]=true;
    }
  }
  if (uniquePeer(clientIP, clientPORT) && currentPeersNo < MAXPEERS)
  {
    strcpy(peers[currentPeersNo].IP, clientIP);
    peers[currentPeersNo++].port = clientPORT;
  }
  //logPeerList("Thread");
}
bool uniquePeer(char IP[15], int Port)
{
  int i;
  for (i = 0; i < currentPeersNo; ++i)
  {
    if (strcmp(peers[i].IP, IP) == 0 && peers[i].port == Port)
      return false;
  }
  return true;
}
void logPeerList(const char place[20])
{
  int i;
  for (i = 0; i < currentPeersNo; ++i)
  {
    printf("[%s] Peer %d : %s :: %d\n", place, i, peers[i].IP, peers[i].port);
  }
}
void *client(void *da)
{

  int sd;
  struct sockaddr_in server;
  int nr = 0;
  char buf[BUFFSIZE];
  char commands[10][BUFFSIZE];
  char IP[15];
  char *p;
  int port, i, checktime;
  bool running = true;
  bool uniqueFile = true;
  while (!serverON)
    ; //wait

  printf("How to join peer: connect <IP> <PORT>\n");
  checktime = 0;
  while (running == true)
  {
    fflush(stdout);
    read(0, buf, sizeof(buf));
    p = strtok(buf, " \n");
    i = 0;
    while (p != NULL)
    {
      strcpy(commands[i], p);
      //printf("parsed: %s %d\n",commands[i],i);
      ++i;
      if (strcmp(commands[0], "add") == 0 || strcmp(commands[0], "remove") == 0)
        p = strtok(NULL, "\n");
      else
        p = strtok(NULL, " \n");
    }
    if (strcmp(commands[0], "connect") == 0)
    {
      strcpy(IP, commands[1]);
      port = atoi(commands[2]);

      //peers.emplace_back(IP,port); //add initial node

      sd = socket(AF_INET, SOCK_STREAM, 0);
      server.sin_family = AF_INET;
      server.sin_addr.s_addr = inet_addr(IP);
      server.sin_port = htons(port);
      connect(sd, (struct sockaddr *)&server, sizeof(struct sockaddr));
      //get peer list
      nr = 1;
      write(sd, &nr, sizeof(int)); //message join get list

      write(sd, &serverIP, sizeof(serverIP));     //send node IP
      write(sd, &serverPORT, sizeof(serverPORT)); //send server PORT

      read(sd, &currentPeersNo, sizeof(int)); //get list size
      read(sd, &peers, sizeof(peers));        //get list

      checkOnlinePeers();

      if (currentPeersNo < MAXPEERS) // add bootstraping node to the peer list
      {
        strcpy(peers[currentPeersNo].IP, IP);
        peers[currentPeersNo].port = port;
        ++currentPeersNo;
      }

      printf("[client]Received peer list\n");
      //logPeerList("client");
      close(sd);
    }
    else if (strcmp(commands[0], "stop") == 0)
    {
      //printf("stop: %s\n",commands[0]);
      running = false;
      serverON = false;
      /*
        sd = socket (AF_INET, SOCK_STREAM, 0);
        server.sin_family = AF_INET;
        server.sin_addr.s_addr = inet_addr(serverIP);
        server.sin_port = serverPORT;
        connect (sd, (struct sockaddr *) &server,sizeof (struct sockaddr));
        write (sd,&nr,sizeof(int));
        close (sd);
        */
      pthread_cancel(serv);
      continue;
    }
    else if (strcmp(commands[0], "add") == 0)
    {
      uniqueFile = true;
      if(fileExists(commands[1]))
      {
        for (int i = 0; i < sharedFiles.size(); ++i)
          if (sharedFiles[i] == commands[1])
            uniqueFile = false, i = sharedFiles.size();

        if (uniqueFile)
          sharedFiles.push_back(commands[1]);
      }
      logSharedFiles();
    }
    else if (strcmp(commands[0], "remove") == 0)
    {
      for (int i = 0; i < sharedFiles.size(); ++i)
      {
        if (sharedFiles[i] == commands[1])
        {
          sharedFiles.erase(sharedFiles.begin() + i);
          i = sharedFiles.size();
        }
      }
      logSharedFiles();
    }
    else if (strcmp(commands[0], "search") == 0)
    {
      for (i = 0; i < currentPeersNo; ++i)
      {
        strcpy(searchfile, commands[1]);
        sd = socket(AF_INET, SOCK_STREAM, 0);
        server.sin_family = AF_INET;
        server.sin_addr.s_addr = inet_addr(peers[i].IP);
        server.sin_port = htons(peers[i].port);
        connect(sd, (struct sockaddr *)&server, sizeof(struct sockaddr));
        nr = 2;
        write(sd, &nr, sizeof(int)); //message type = search filename

        write(sd, &serverIP, sizeof(serverIP));     //send node IP
        write(sd, &serverPORT, sizeof(serverPORT)); //send node PORT

        write(sd, &commands[1], sizeof(commands[1])); //send filename
        nr = 10;                                      //time to live
        write(sd, &nr, sizeof(nr));                   //send ttl
        close(sd);
      }
      //wait a few seconds
      sleep(2);
      //show search results
      for (i = 0; i < searchpeersNo; ++i)
      {
        printf("%d. %s %s::%d\n", i, searchfile, searchpeers[i].IP, searchpeers[i].port);
      }
    }
    else if (strcmp(commands[0], "download") == 0)
    {
      //download filename IP PORT
      char filename[BUFFSIZE];
      strcpy(filename, commands[1]);
      sd = socket(AF_INET, SOCK_STREAM, 0);
      server.sin_family = AF_INET;
      server.sin_addr.s_addr = inet_addr(commands[2]);
      server.sin_port = htons(atoi(commands[3]));
      connect(sd, (struct sockaddr *)&server, sizeof(struct sockaddr));
      nr = 4;
      write(sd, &nr, sizeof(int)); //message type = download filename

      write(sd, &serverIP, sizeof(serverIP));     //send node IP
      write(sd, &serverPORT, sizeof(serverPORT)); //send node PORT

      write(sd, &filename, sizeof(filename)); //send filename
    }
    else
    {
      printf("Unknown command.\n");
    }
    ++checktime;
    if(checktime == 5000)
    {
      checkOnlinePeers();
      checktime = 0;
    }
  }
  pthread_exit(NULL);
}
void getIP()
{
  struct ifaddrs *ifAddrStruct = NULL;
  struct ifaddrs *ifa = NULL;
  void *tmpAddrPtr = NULL;

  getifaddrs(&ifAddrStruct);

  for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next)
  {
    if (!ifa->ifa_addr)
    {
      continue;
    }
    if (ifa->ifa_addr->sa_family == AF_INET)
    {
      tmpAddrPtr = &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
      char addressBuffer[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
      if (strcmp(ifa->ifa_name, "enp0s3") == 0)
      {
        strcpy(serverIP, addressBuffer);
        printf("%s IP Address %s\n", ifa->ifa_name, addressBuffer);
      }
    }
  }
  if (ifAddrStruct != NULL)
    freeifaddrs(ifAddrStruct);
}

void upload(int sd, FILE *fd)
{
  int n;
  char buff[BUFFSIZE];
  memset(buff, 0, BUFFSIZE);
  while ((n = fread(buff, sizeof(char), BUFFSIZE, fd)) > 0)
  {
    if (send(sd, buff, n, 0) == -1 || n != BUFFSIZE )
    {
      perror("Failed to upload file.\n");
    }
    memset(buff, 0, BUFFSIZE);
  }
}

void download(int sd, FILE *fd)
{
  ssize_t n;
  char buff[BUFFSIZE];
  memset(buff, 0, BUFFSIZE);
  while ((n = recv(sd, buff, BUFFSIZE, 0)) > 0)
  {
    if ( fwrite(buff, sizeof(char), n, fd) != n || n == -1)
    {
      printf("Failed to download.\n");
      return;
    }
    memset(buff, 0, BUFFSIZE);
  }
}

void checkOnlinePeers()
{
  int sd,i,j,nr;
  struct sockaddr_in server;
  for(i=0;i<MAXPEERS;++i)
    peerOnline[i]=false;
  for (i = 0; i < currentPeersNo; ++i)
      {
        sd = socket(AF_INET, SOCK_STREAM, 0);
        server.sin_family = AF_INET;
        server.sin_addr.s_addr = inet_addr(peers[i].IP);
        server.sin_port = htons(peers[i].port);
        connect(sd, (struct sockaddr *)&server, sizeof(struct sockaddr));
        nr = 7;
        write(sd, &nr, sizeof(int)); //message type = u online?
        write(sd, &serverIP, sizeof(serverIP));     //send node IP
        write(sd, &serverPORT, sizeof(serverPORT)); //send node PORT
        close(sd);
      }
  //sleep
  sleep(2);
  //delete inactive
  for(i=0;i<currentPeersNo;++i)
  {
    if(peerOnline[i]==false)
    {
      for(j=i;j<currentPeersNo-1;++j)
        {
          strcpy(peers[j].IP,peers[j+1].IP);
          peers[j].port=peers[j+1].port;
          peerOnline[j]=peerOnline[j+1];
        }
      --currentPeersNo;
      --i;
    }
  }
}