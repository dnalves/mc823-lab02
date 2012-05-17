#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <time.h>


#define PORT "3490" // Porta que sera usada para fazer enviar/receber mensagens

#define MAXDATASIZE 3000 // Numero de maximo de bytes que podem ser transmitidos
#define CODE_SIZE 5 // Tamanho do codigo da disciplina
#define NEXT_CLASS_SIZE 1000 // Tamanho maximo do comentario para a prox aula
#define USER_MAXSIZE 20 // Tamanho maximo do login do usuario
#define PASS_MAXSIZE 20 // Tamanho maximo do password do usuario
#define MICRO_PER_SECOND	1000000

static const char removed_entry = '-';
static const char valid_entry = '*';

static int totalSends = 0;  //Numero de Mensagens enviadas pelo socket
static int totalRecvs = 0;	// Numero de Mensagens recebidas pelo socket

// Estruturas para as medicoes de tempo
struct timeval start_time;
struct timeval stop_time;	

// Funcao para facilitar debug
void print(char *nome, char *var){
	fprintf(stderr, "\n%s: %s\n", nome, var);
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


/*
 * Instancia o socket e as informacoes do endereco do mesmo para o ip passado
 * e faz chamada ao sendto com o buffer recebido por parametro, no final a instancia
 * do socket e guarda no apontador passado
 */
int sendDataTo(char *buf, char *ip, int *sock, struct addrinfo *p)
{
	int sockfd;
	struct addrinfo hints, *servinfo;
	int rv;
	int numbytes;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;

	if ((rv = getaddrinfo(ip, PORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and make a socket
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
		                     p->ai_protocol)) == -1) {
								 perror("talker: socket");
								 continue;
							 }

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "talker: failed to bind socket\n");
		return 2;
	}

	//Inicia a contagem do tempo
	gettimeofday(&start_time, NULL);

	if ((numbytes = sendto(sockfd, buf, strlen(buf), 0,
	                       p->ai_addr, p->ai_addrlen)) == -1) {
							   perror("talker: sendto");
							   exit(1);
						   }
	totalSends++;
	freeaddrinfo(servinfo);

	*sock = sockfd; //Guarda a instancia do socket no apontador

	return 0;
}

/*
 * Metodo para receber a mensagem vinda do servidor, com o mesmo socket usado
 * pelo sendDataTo
 */
int receiveData(int *sock, struct addrinfo *p){
	int sockfd = *sock;
	int numbytes;
	struct sockaddr_storage their_addr;
	char buf[MAXDATASIZE];
	socklen_t addr_len;
	char s[INET6_ADDRSTRLEN];

	struct timeval tv;
	fd_set readfds;

	tv.tv_sec = 3; //Tempo limite para o recebimento de um pacote do servidor
	tv.tv_usec = 500000;

	FD_ZERO(&readfds);
	FD_SET(sockfd, &readfds);

	addr_len = sizeof their_addr;
	//Itera ate receber o pacote final, caso a espera por pacote exceda o tempo
	//limite, o loop e quebrado e uma mensagem é mostrado ao usario
	while(1){
		int t = select(sockfd + 1, &readfds, NULL, NULL, &tv);

		if (t != 0) {
			if (FD_ISSET(sockfd, &readfds)) { //Caso esteja disponivel para leitura
				memset(buf, 0, sizeof buf);
				if ((numbytes = recvfrom(sockfd, buf, MAXDATASIZE-1 , 0,
				                         (struct sockaddr *)&their_addr, &addr_len)) == -1) {
											 perror("recvfrom");
											 exit(1);
										 }
				totalRecvs++;

				buf[numbytes] = '\0';

				if (strcmp(buf, "[Finished]") == 0)
					break;

				fprintf(stdout, "%s", buf);    
			}
			else{
				printf("Mensagem completa não recebida, provavelmente algum pacote foi perdido.\n");
				break;

			}

		}
		else { //Se tempo limite excedido, sai do loop de leitura
			printf("Mensagem completa não recebida, provavelmente algum pacote foi perdido.\n");
			break;
		}


	}

	close(sockfd);

	// Pega o tempo de termino de uma operacao
	gettimeofday(&stop_time, NULL);

	// Calcula o tempo gasto para a operacao
	float resul;
	resul = (float)(stop_time.tv_sec - start_time.tv_sec);
	resul += (stop_time.tv_usec - start_time.tv_usec)/(float)MICRO_PER_SECOND;

	// Escreve no arquivo
	FILE *f;
	if((f = fopen("./cliente.txt", "a")) == NULL){
		fprintf(stderr, "Can't open file.\n\n\n");
		exit(1);
	};

	fprintf(f, "%f\n", resul);
	fclose (f);

	gettimeofday(&stop_time, NULL);
	gettimeofday(&start_time, NULL);

	return 0;
}

/*
 * Envia a requisição desejada para o servidor e recebe sua resposta
 */
int sendRequest(char packet[NEXT_CLASS_SIZE+1+100])
{

	// MUDA O IP!!!
	char ip[100] = "143.106.16.28"; //Ip do servidor

	int sockfd;
	struct addrinfo *p;

	sendDataTo(packet, ip, &sockfd, p);

	receiveData(&sockfd, p);

	return 1;
}

int main(){

	int choice = 999;
	char cod[CODE_SIZE+1];
	char temp[30];
	char next_class[NEXT_CLASS_SIZE+1];
	char user[USER_MAXSIZE+1], password[PASS_MAXSIZE+1];
	char packet[NEXT_CLASS_SIZE+1+100];


	while(choice != 0){
		printf("\n\n#############################################\n");
		printf("# Escolha o que deseja fazer:\n");
		printf("# 1. Listar todas as disciplinas (codigo e titulo)\n");
		printf("# 2. Listar todas as disciplinas\n");
		printf("# 3. Mostrar programa da disciplina pelo codigo\n");
		printf("# 4. Mostrar disciplina pelo codigo\n");
		printf("# 5. Mostrar notas da proxima aula pelo codigo\n");
		printf("# 6. Alterar nota da proxima aula de uma disciplina\n");
		printf("# 0. Sair\n");
		fgets(temp, sizeof(temp), stdin);
		sscanf(temp, "%d", &choice);
		memset(packet, 0, sizeof packet);
		sscanf(temp, "%s", temp);
		strcat(packet, temp);
		// fprintf(stderr, "\nPacket: %s\n", packet);
		fprintf(stdout, "\n");
		if(choice < 3 && choice > 0){
			// Nao precisa de codigo da disciplina
			sendRequest(packet);
		}else if(choice > 2 && choice < 7){
			//  Pegar codigo da disciplina e mandar
			printf("Digite o codigo da disciplina no formato XX123\n");
			fgets(temp, sizeof(temp), stdin);
			sscanf(temp, "%s", cod);
			strcat(packet, "|");
			strcat(packet, cod);
			memset(temp, 0, sizeof temp);
			if(choice == 6){
				// Pegar usuario e senha para mandar
				printf("\nUsuario:");
				fgets(temp, sizeof(temp), stdin);
				sscanf(temp, "%s", user);
				strcat(packet, "|");
				strcat(packet, user);
				memset(temp, 0, sizeof temp);
				printf("\nSenha:");
				fgets(temp, sizeof temp, stdin);
				sscanf(temp, "%s", password);
				memset(temp, 0, sizeof temp);
				strcat(packet, "|");
				strcat(packet, password);

				// Pega o comentario da proxima aula
				memset(temp, 0, sizeof temp);
				printf("\nComentario:");
				fgets(next_class, sizeof(next_class), stdin);
				strcat(packet, "|");
				strcat(packet, next_class);
				print("packet", packet);

			}
			sendRequest(packet);


		}
		memset(temp, 0, sizeof temp);


	}

	printf("Total de pacotes enviados: %d\n", totalSends);
	printf("Total de pacotes recebidos: %d\n", totalRecvs);

	return 0;

}
