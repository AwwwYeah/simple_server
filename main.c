#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>

/* Server parameters */
#define SERVER_PORT 80
#define ROOT_PATH "./root/index.html"
#define PATH_404 "404.html"

/* Start strings */
#define ST_STR_OK "HTTP/1.1 200 OK\r\n"
#define ST_STR_NF "HTTP/1.1 404 Not Found\r\n"

/* Headers */
#define SERVER_HDR "Server: TestServer\r\n\r\n"

/* Other program defines */
#define ERR "[*] ERROR"
#define MESSG_SIZE 640000

int open_sock(unsigned short port, struct sockaddr_in *sin, char *errbuf); 
int connections_handling(int serv_sock, char *errbuf);
void finish_handling(int client_sock, void *client_mem);
char *get_http_header(char *messg);
int send_file(char *standart_str, int client_sock, struct sockaddr_in *client_sin, char *errbuf);


int main(int argc, char **argv)
{
	int server_sock;
	struct sockaddr_in serv_sin;
	char errbuf[512];
	FILE *fd;

	/* Open the tcp socket */
	server_sock = open_sock(SERVER_PORT, &serv_sin, errbuf);

	if(server_sock == -1)
	{
		printf("%s\n", errbuf);

		exit(-1);
	}

	printf("[*] Starting the server...\n");
	printf("[*] The socket was bound to %hu port\n", SERVER_PORT);

	/* Connections handling */
	while(1)
	{
		if(connections_handling(server_sock, errbuf) == -1)
			printf("%s\n", errbuf);

	}

	close(server_sock);

	exit(0);
}

int open_sock(unsigned short port, struct sockaddr_in *sin, char *errbuf)
{
	int sock;
	int setsockopt_arg = 1;

	memset(errbuf, 0, 512);

	sin->sin_family = AF_INET;
	sin->sin_port   = htons(port);
	sin->sin_addr.s_addr = INADDR_ANY;
	memset(sin->sin_zero, 0, 8);

	sock = socket(PF_INET, SOCK_STREAM, 0);

	if(sock == -1)
	{
		snprintf(errbuf, 512, "%s open_sock(): socket(): %s\n", ERR, strerror(errno));

		return -1;
	}

	if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &setsockopt_arg, sizeof(int)) == -1)
	{
		snprintf(errbuf, 512, "%s open_sock(): setsockopt(): %s\n", ERR, strerror(errno));

		return -1;
	}	

	if(bind(sock, (struct sockaddr *)sin, sizeof(struct sockaddr_in)) == -1)
	{
		snprintf(errbuf, 512, "%s open_sock(): bind(): %s\n", ERR, strerror(errno));

		return -1;
	}

	if(listen(sock, 10) == -1)
	{
		snprintf(errbuf, 512, "%s open_sock(): lisen(): %s\n", ERR, strerror(errno));

		return -1;
	}

	return sock;
}

int connections_handling(int serv_sock, char *errbuf)
{
	int client_sock;
	struct sockaddr_in client_sin;
	int sin_size = sizeof(struct sockaddr_in);
	char *client_messg = (char *)malloc(MESSG_SIZE);
	int res, stand_str_len, resource_len;
	char *standart_str;

	memset(errbuf, 0, 255);
	memset(client_messg, 0, MESSG_SIZE);

	client_sock = accept(serv_sock, (struct sockaddr *)&client_sin, &sin_size);

	if(client_sock == -1)
	{
		snprintf(errbuf, 512, "%s connections_handling(): accept(): %s\n", ERR, strerror(errno));

		finish_handling(client_sock, client_messg);

		return -1;
	}

	printf("[*] Accepted connection from %s\n", inet_ntoa(client_sin.sin_addr));

	res = recvfrom(client_sock, client_messg, MESSG_SIZE, 0, (struct sockaddr *)&client_sin, &sin_size);

	if(res <= 0)
	{
		snprintf(errbuf, 512, "%s connections_handling(): accept(): %s\n", ERR, strerror(errno));

		finish_handling(client_sock, client_messg);

		return -1;
	}

	standart_str = get_http_header(client_messg);

	printf("[*] %s (%s)\n", inet_ntoa(client_sin.sin_addr), standart_str);

	/* If the request is GET, then send requested file */
	if(strncmp("GET", standart_str, 3) == 0)
	{
		if(send_file(standart_str, client_sock, &client_sin, errbuf) == -1)
			printf("%s\n", errbuf);

	}

	else
	{
		snprintf(errbuf, 512, "%s connections_handling(): unknown request \"%s\"\n", ERR, standart_str);

		finish_handling(client_sock, client_messg);

		return -1;
	}

	finish_handling(client_sock, client_messg);

	return 0;
}

void finish_handling(int client_sock, void *client_mem)
{
	close(client_sock);

	free(client_mem);

	return;
}

char *get_http_header(char *messg)
{
	static char http_head[128];

	memset(http_head, 0, 128);
	
	for(int i = 0; messg[i] != '\r'; i++)
		http_head[i] = messg[i];

	return http_head;
}

int send_file(char *standart_str, int client_sock, struct sockaddr_in *client_sin, char *errbuf)
{
	char requested_file[128];
	FILE *fd;
	int descriptor;
	int stand_str_len;
	int resource_len;
	char *serv_messg = (char *)malloc(MESSG_SIZE);
	struct stat st;
	char found_file[256];

	memset(serv_messg, 0, MESSG_SIZE);
	memset(requested_file, 0, 56);
	memset(found_file, 0, 256);

	for(int i = 4, f = 0; standart_str[i] != ' '; i++, f++)
		requested_file[f] = standart_str[i];

	/* If this is a root file */
	if(!strcmp("/", requested_file))
	{
		fd = fopen(ROOT_PATH, "r");
		descriptor = open(ROOT_PATH, O_RDONLY, 0);

		/* If the file isn't found, send 404 code */
		if(fd == NULL)
		{
			printf("[*] %s (HTTP/1.1 404 Not Found\n", inet_ntoa(client_sin->sin_addr));

			strcat(serv_messg, ST_STR_NF);
			strcat(serv_messg, SERVER_HDR);
			stand_str_len = strlen(serv_messg);

			if((fd = fopen(PATH_404, "r")) != NULL)
			{
				descriptor = open(PATH_404, O_RDONLY, 0);

				fstat(descriptor, &st);
				resource_len = fread(&serv_messg[stand_str_len], 1, st.st_size, fd);

				sendto(client_sock, serv_messg, stand_str_len + resource_len, 0, (struct sockaddr *)&client_sin, sizeof(struct sockaddr_in));

				fclose(fd);
				close(descriptor);
				free(serv_messg);

				return 0;
			}

			free(serv_messg);

			return -1;
		}

		// 1. Compose HTTP header
		strcat(serv_messg, ST_STR_OK);
		strcat(serv_messg, SERVER_HDR);
		strcat(serv_messg, "\r\n");
		stand_str_len = strlen(serv_messg);

		// 2. Read requested resource
		fstat(descriptor, &st);
		resource_len = fread(&serv_messg[stand_str_len], 1, st.st_size, fd);

		// 3. Send the resource
		resource_len = sendto(client_sock, serv_messg, stand_str_len + resource_len, 0, (struct sockaddr *)&client_sin, sizeof(struct sockaddr_in));

		printf("[*] %s (HTTP/1.1 200 OK)\n", inet_ntoa(client_sin->sin_addr));

		fclose(fd);
		close(descriptor);
	}

	/* Other file */
	else
	{
		fd = fopen(&requested_file[1], "r");

		/* If the file isn't found, send 404 code */
		if(fd == NULL)
		{
			printf("[*] %s (HTTP/1.1 404 Not Found)\n", inet_ntoa(client_sin->sin_addr));

			strcat(serv_messg, ST_STR_NF);
			strcat(serv_messg, SERVER_HDR);
			stand_str_len = strlen(serv_messg);

			if((fd = fopen(PATH_404, "r")) != NULL)
			{
				descriptor = open(PATH_404, O_RDONLY, 0);

				fstat(descriptor, &st);
				resource_len = fread(&serv_messg[stand_str_len], 1, st.st_size, fd);

				sendto(client_sock, serv_messg, stand_str_len + resource_len, 0, (struct sockaddr *)&client_sin, sizeof(struct sockaddr_in));

				fclose(fd);
				close(descriptor);
				free(serv_messg);

				return 0;
			}

			free(serv_messg);

			return -1;
		}

		descriptor = open(&requested_file[1], O_RDONLY, 0);

		// 1. Compose HTTP header
		strcat(serv_messg, ST_STR_OK);
		strcat(serv_messg, SERVER_HDR);
		stand_str_len = strlen(serv_messg);

		// 2. Read requested resource
		fstat(descriptor, &st);
		resource_len = fread(&serv_messg[stand_str_len], 1, st.st_size, fd);

		// 3. Send the resource
		resource_len = sendto(client_sock, serv_messg, stand_str_len + resource_len, 0, (struct sockaddr *)&client_sin, sizeof(struct sockaddr_in));

		printf("[*] %s (HTTP/1.1 200 OK)\n", inet_ntoa(client_sin->sin_addr));

		fclose(fd);
		close(descriptor);

	}

	free(serv_messg);

	return 0;
}

