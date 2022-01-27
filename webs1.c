#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <sys/wait.h>

// http://localhost:8000

#define SERVER_PORT    "8000" //порт соединения,тк у нас будет IP
#define MAX_CONNECTION 1000  //макс число соединений


typedef enum 
{//перечисление //типы http запросов
 
  eHTTP_UNKNOWN = 0
 ,eHTTP_CONNECT
 ,eHTTP_DELETE
 ,eHTTP_GET
 ,eHTTP_HEAD
 ,eHTTP_OPTIONS
 ,eHTTP_PATCH
 ,eHTTP_POST
 ,eHTTP_PUT
 ,eHTTP_TRACE

}eHTTPMethod;

typedef struct 
{
  eHTTPMethod type;
  char        path[255];
}sHTTPHeader;

void *get_client_addr(struct sockaddr *);
int create_socket(const char *); //создает сокет,куда передаем порт

void http_request(int);//передадим идентифик сокета
void parse_http_request(const char*, sHTTPHeader *);//передаем строку кот мы прочли и второй арг будет заполняться
void send_message(int, const char*); //отправ смс обратно клиенту. Идентиф и сообщение
void send_404(int); 


int main()
{
  int sock;

  sock = create_socket(SERVER_PORT); //вызвали функцию создания сокета.передали порт
 
  if(sock < 0)
  {
    fprintf(stderr, "error create socket\n");
    return -1;
  }

  printf("server created!\n");

  struct sockaddr_storage client_addr;
  int client_d;
  //char client_ip
  while(1) 
  {
    socklen_t s_size = sizeof(client_addr);
    client_d = accept(sock, (struct sockaddr*)&client_addr, &s_size);

    if(client_d == -1)
    {
      fprintf(stderr, "error accept\n");
      return -1;
    }

    char ip[INET6_ADDRSTRLEN];//буфер выделяем
    inet_ntop(client_addr.ss_family, get_client_addr((struct sockaddr *)&client_addr), ip, sizeof ip);//для вывода айпи адр клиента кот подключился
    printf("server: got connection from %s\n", ip);

    // read
    http_request(client_d);

    close(client_d);
  }

  return 0;
}


void *get_client_addr(struct sockaddr *sa)
{
  if (sa->sa_family == AF_INET)
  {
    return &(((struct sockaddr_in*)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int create_socket(const char *apstrPort)
{
  struct addrinfo hints;  //хранится инф об адресе.Этот адрес нужен для сокета
  struct addrinfo *servinfo;
  struct addrinfo *p; //р - временная перем

  memset(&hints, 0, sizeof(hints)); //очищаем память,забьем все нулями

  // IPv4 or IPv6
  hints.ai_family   = AF_UNSPEC; //типа адреса.вторая часть это const,кот запис-ся в 1 часть
  hints.ai_socktype = SOCK_STREAM; //тип сокета.
  hints.ai_flags    = AI_PASSIVE; 

  int r = getaddrinfo(NULL, apstrPort, &hints, &servinfo); 
  if( r != 0)
  {
    fprintf(stderr, "error getaddrinfo()\n");
    return -1;
  }

  int sock;
  int yes;
  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  for(p = servinfo; p != NULL; p = p->ai_next)//перебираем адреса
  { //будет список в кот будет инфа
    sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if(sock == -1)
      continue;//продолдаем дальше,выбир из списка нужный прав сокет кот соответ всем этим параметрам

////////////////////////
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
    {
      fprintf(stderr, "error setsockopt\n");
      close(sock);
      freeaddrinfo(servinfo); // all done with this structure
      return -2;
    }
////////////////////////////////////////////////

    if(bind(sock, p->ai_addr, p->ai_addrlen) == -1)
    {
      close(sock);//значит отработан неправильно
      continue; //продолжим цикл фор на 144 строке
    }
    break; //завершаем цикл
  }

///////////////////////////////////////////////////////////////////////////////////////////////////////////////

  freeaddrinfo(servinfo); // all done with this structure //освобождаем структуру

  if(p == NULL) //мы прошлись по всему списку. т.е.не нашли нам нужного сокета
  {
    fprintf(stderr, "failed to find address\n"); //выдаем ошибку
    return -3;
  }

////////////////////////////////////////////////////////////
  if(listen(sock, MAX_CONNECTION) == -1)
  {
    fprintf(stderr, "error listen\n");
    return -4;
  }

  return sock; //иначе возвращ наш дексриптор
}
//////////////////////////////////////////////////////////////////////

void http_request(int aSock) //передаем идентиф нашего сокета
{ //разбирает http 
  const int request_buffer_size = 65536; //макс прочесть за 1 раз 64кб
  char      request[request_buffer_size];//буфер куда я читаю

  int bytes_recvd = recv(aSock, request, request_buffer_size - 1, 0);
/////////////////////////////////////////////////////
  if (bytes_recvd < 0) //прочли
  {
    fprintf(stderr, "error recv\n");
    return;
  }

  request[bytes_recvd] = '\0';

  printf("request:\n%s\n",request);

  sHTTPHeader req;
  parse_http_request(request, &req);

  if(req.type == eHTTP_GET)
  {
    send_message(aSock, "sensor 1: 10<br> sensor 2: 20<br><a href=\"https://www.msu.kz/news/detail.php?ELEMENT_ID=7296\">external</a><br><a href=\"internal\">internal</a>");
  }
  else
  {
    send_404(aSock);
  }
}

void parse_http_request(const char *apstrRequest, sHTTPHeader *apHeader)
{//определяю это запрос get или нет
  int  type_length = 0;
  char type[255]   = {0};
  int  index = 0;

  apHeader->type = eHTTP_UNKNOWN;

  sscanf(&apstrRequest[index], "%s", type); //читаю строку до пробела
  type_length = strlen(type);

  if(type_length == 3)
  {
    if(type[0] == 'G' && type[1] == 'E' && type[2] == 'T') //если это get,то я отправляю смс с пом sen_mes
      apHeader->type = eHTTP_GET;

    index += type_length + 1; //индекс html
    sscanf(&apstrRequest[index], "%s", apHeader->path);
  }
}

void send_message(int aSock, const char *apstrMessage)
{
  char buffer[65536] = { 0 };

  strcat(buffer, "HTTP/1.1 200 OK\n\n"); //беру строку и формирую ответ.Первым делом чтобы браузер знал что ок. 200 - это ОК
  strcat(buffer, "<h1>");
  strcat(buffer, apstrMessage);
  strcat(buffer, "</h1>");

  int len = strlen(buffer);
  send(aSock, buffer, len, 0);
}

void send_404(int aSock)
{ 
  const char *buffer = "HTTP/1.1 404 \n\n";
  int len = strlen(buffer);
  send(aSock, buffer, len, 0);
}




// server: got connection from 127.0.0.1
// request:
// GET /index.html HTTP/1.1
// Host: localhost:3490
// Connection: keep-alive
// Upgrade-Insecure-Requests: 1
// User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Ubuntu Chromium/68.0.3440.75 Chrome/68.0.3440.75 Safari/537.36
// Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,image/apng,*/*;q=0.8
// Accept-Encoding: gzip, deflate, br
// Accept-Language: en-US,en;q=0.9,ru;q=0.8
