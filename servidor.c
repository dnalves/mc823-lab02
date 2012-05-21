#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/time.h>
#include <time.h>

#define PORT "3490"			// Porta que sera usada para fazer enviar/receber mensagens
#define MAXDATASIZE 3000	// Numero de maximo de bytes que podem ser transmitidos
#define CODE_SIZE 5			// Tamanho do codigo da disciplina
#define USER_MAXSIZE 20		// Tamanho maximo do login do usuario
#define PASS_MAXSIZE 20		// Tamanho maximo do password do usuario
#define NEXT_CLASS_SIZE 1000// Tamanho maximo do comentario para a prox aula
#define PROGRAM_SIZE 1000	// Tamanho maximo do programa da aula
#define TITLE_SIZE 50		// Tamanho maximo do titulo da disciplina
#define ROOM_SIZE 10		// Tamanho maximo do nome da sala
#define TIME_SIZE 30		// Tamanho maximo do campo horario da aula
#define REMOVED_MARKER_SIZE 2// Tamanho do marcador de entrada removida no BD
//Tamanho maximo de uma entrada do bd
#define MAX_ENTRY_SIZE (CODE_SIZE+NEXT_CLASS_SIZE+PROGRAM_SIZE+TITLE_SIZE+ROOM_SIZE+TIME_SIZE+REMOVED_MARKER_SIZE+100)
#define PROFESOR 1			// Codigo para identificar professor
#define STUDENT 2			// Codigo para identificar aluno
#define MICRO_PER_SECOND	1000000

// Estrutura para definir uma disciplina
typedef struct {
	// Atributos
	char title[TITLE_SIZE+1];
	char program[PROGRAM_SIZE+1];
	char room[ROOM_SIZE+1];
	char time[TIME_SIZE+1];
	char cod[CODE_SIZE+1];
	char next_class[NEXT_CLASS_SIZE+1];
} discipline;


static const discipline empty_discipline;
static const char removed_entry = '-';
static const char valid_entry = '*';
static int numbytes; // Variavel para guardar quantos bytes foram transmitidos 
static int totalReceivs = 0;	// Numero de Mensagens recebidas pelo socket
static int totalSends = 0;		// Numero de Mensagens enviadas pelo socket
// O contador de mensagens enviadas é incrementado ao final de cada mensagem enviada pelo socket.
// O mesmo vale para o contador de mensagens recebidas, porem para cada mensagem recebida.

// Estruturas para as medicoes de tempo
struct timeval start_time;
struct timeval stop_time;
float resul = 0;

// Mapeia uma string com todas as informacoes da disciplina em uma estrutura organizada
int map_entry_to_struct(discipline * p, char entry[MAX_ENTRY_SIZE]){
	char *pch, *curpos;

	// Pulando o marcador de entrada removida (ja testado)
	pch = &entry[1];
	curpos = ++pch;

	// Pegando o Codigo da Disciplina
	pch = strchr(pch+1, '|');

	memcpy(p->cod, curpos, pch-curpos);
	curpos = ++pch;

	// Pegando o Titulo
	pch = strchr(pch+1, '|');
	memcpy(p->title, curpos, pch-curpos);
	curpos = ++pch;

	// Pegando a Sala
	pch = strchr(pch+1, '|');
	memcpy(p->room, curpos, pch-curpos);
	curpos = ++pch;	

	// Pegando o Programa
	pch = strchr(pch+1, '|');
	memcpy(p->program, curpos, pch-curpos);
	curpos = ++pch;

	// Pegando o Horario
	pch = strchr(pch+1, '|');
	memcpy(p->time, curpos, pch-curpos);
	curpos = ++pch;

	// Pegando a Proxima Aula
	pch = strchr(pch+1, '\0');
	memcpy(p->next_class, curpos, pch-curpos);	

	return 0;

}

// pega o sockaddr, IPv4 ou IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


// Funcao para facilitar debug
void print(char *nome, char *var){
	fprintf(stderr, "\n%s: %s\n", nome, var);
}

// Procura uma disciplina pelo codigo
// Parametros eh um ponteiro para uma estrutura discipline
// e o codigo da disciplina
int search_discipline_by_code(char cod[5], discipline * p){
	// abrir descritor de arquivo
	FILE *f;
	char actual_code[CODE_SIZE+1];
	char entry[MAX_ENTRY_SIZE];
	char valid[3];
	long pos = 0;

	// Limpa a disciplina
	discipline d = empty_discipline;
	// Limpa o buffer que vai guardar a entrada da disciplina lida do banco
	memset(entry, 0, sizeof entry);

	// Abre o arquivo do banco de dados
	if((f = fopen("./db.txt", "r")) == NULL){
		fprintf(stderr, "Can't open file.\n\n\n");
		exit(1);
	};


	while(!feof(f)){

		pos = ftell(f);
		// Testa se a entrada é válida
		if(fgets(valid, 3, f) != NULL && valid[0] == valid_entry){
			// Caso seja, pega o codigo da disciplina da entrada
			if(fgets(actual_code, CODE_SIZE+1, f) != NULL){

				// Compara com o codigo da disciplina desejada
				if(strcmp(actual_code, cod) == 0){

					// Achou a disciplina (volta ao começo da entrada)
					if(fseek(f, pos, SEEK_SET) == 0){

						// Pega a toda a entrada e coloca em "entry"
						if(fgets(entry, MAX_ENTRY_SIZE, f) != NULL){

							// Mapeia a disciplina na estrutura
							map_entry_to_struct(&d, entry);
							*p = d;
							fclose(f);
							return 0;
						}
					} else{

						perror( "Erro no Seek" );
						printf( "Erro no Seek: %s\n", strerror( errno ) );
						exit(1);
					}
				} else {
					// Nao eh o codigo da disciplina (le ateh o fim da linha e tenta a proxima)
					if(fgets(entry, MAX_ENTRY_SIZE, f) != NULL){
						memset(entry, 0, sizeof entry);
						continue;
					}
				}		

			}
		} else {
			// Caso esteja removida, le ateh o fim da linha e tenta a proxima entrada
			if(fgets(entry, MAX_ENTRY_SIZE, f) != NULL){
				memset(entry, 0, sizeof entry);
				continue;
			}
		}

	}
	fprintf(stdout, "\nA Disciplina não foi encontrada\n");
	fclose(f);
	return -1;
}


// Pega o codigo de CODE_SIZE digitos na string. que chegou do socket
char *getCodeFromString(char buf[MAXDATASIZE], char *code){

	// Pula a opcao do cliente
	char *pch;
	pch = strchr(buf, '|');
	pch++;

	// Pega o codigo
	memcpy(code, pch, CODE_SIZE);
	code[CODE_SIZE] = '\0';
	return pch;

}


void getNextClassFromString(char *string, char *next_class){
	char *pch;
	string++;
	pch = strchr(string, '\0');
	memcpy(next_class, string, pch - string);
}

// Pega o usuario e Senha a partir da String que chegou do socket
char *getUserAndPassFromString(char *string, char *user, char *pass){
	char *pch, *curpos;

	// Jumping the code
	pch = strchr(string, '|');
	curpos = ++pch;

	// Getting the user
	pch = strchr(pch, '|');
	memcpy(user, curpos, pch - curpos);
	curpos = ++pch;

	// Getting the password
	pch = strchr(pch, '|');
	memcpy(pass, curpos, pch - curpos);

	return pch;

}

// Imprime os Codigos e Titulos de todas as disciplinas
void print_code_title_all_disciplines(int sockfd, struct sockaddr_storage p){

	// Abrir descritor de arquivo
	FILE *f;
	discipline d;
	char entry[MAX_ENTRY_SIZE];
	char to_send[MAX_ENTRY_SIZE];

	if((f = fopen("./db.txt", "r")) == NULL){
		fprintf(stderr, "Can't open file.\n\n\n");
		exit(1);
	};

	while(!feof(f)){
		d = empty_discipline;
		memset(entry, 0, sizeof entry);
		// Pega uma entrada do banco de dados
		if(fgets(entry, MAX_ENTRY_SIZE, f) != NULL){
			// Se a entrada nao estiver removida
			if(entry[0] != removed_entry){
				// Mapeia para a estrutura
				map_entry_to_struct(&d, entry);
				// Prepara a string para ser enviada
				memset(to_send, 0, sizeof to_send);
				sprintf(to_send, "%s -- %s\n", d.cod, d.title);

				//Para a contagem do tempo
				gettimeofday(&stop_time, NULL);
				resul += (float)(stop_time.tv_sec - start_time.tv_sec);
				fprintf(stderr, "\nTempo até o momento: %f\n", resul);
				// Agora envia o pacote com as informacoes
				if ((numbytes = sendto(sockfd, to_send, strlen(to_send), 0,
				                       (struct sockaddr *)&p, sizeof p)) == -1) {
										   perror("talker: sendto");
										   exit(1);
									   }
				//Reinicia a contagem do tempo
				gettimeofday(&start_time, NULL);

				totalSends++;


			}

		}
	}
	fclose(f);

	// Envia "[Finished]" para indicar que enviou a mensagem toda
	if ((numbytes = sendto(sockfd, "[Finished]", strlen("[Finished]"), 0,
	                       (struct sockaddr *)&p, sizeof p)) == -1) {
							   perror("talker: sendto");
							   exit(1);
						   }
	totalSends++;
}

// Imprime o programa da disciplina a partir do codigo
void print_program_by_code(char cod[CODE_SIZE], int sockfd, struct sockaddr_storage p){
	discipline d;
	char to_send[MAX_ENTRY_SIZE];

	// Procura a disciplina pelo codigo e coloca na estrutura "d"
	if(search_discipline_by_code(cod, &d) == -1){
		// Caso nao tenha achado
		sprintf(to_send, "\nDisciplina nao encontrada\n");

		//Para a contagem do tempo
		gettimeofday(&stop_time, NULL);
		resul += (float)(stop_time.tv_sec - start_time.tv_sec);

		// Envia o texto que nao encontrou
		if ((numbytes = sendto(sockfd, to_send, strlen(to_send), 0,
		                       (struct sockaddr *)&p, sizeof p)) == -1) {
								   perror("talker: sendto");
								   exit(1);
							   }
		totalSends++;
		// Envia "[Finished]" para indicar que enviou a mensagem toda 
		if ((numbytes = sendto(sockfd, "[Finished]", strlen("[Finished]"), 0,
		                       (struct sockaddr *)&p, sizeof p)) == -1) {
								   perror("talker: sendto");
								   exit(1);
							   }

		totalSends++;
		return;
	}
	//Caso tenha achado
	sprintf(to_send, "\nPrograma da Disciplina %s\n\n%s", cod, d.program);

	//Para a contagem do tempo
	gettimeofday(&stop_time, NULL);
	resul += (float)(stop_time.tv_sec - start_time.tv_sec);

	// Agora envia o pacote com as informacoes
	if ((numbytes = sendto(sockfd, to_send, strlen(to_send), 0,
	                       (struct sockaddr *)&p, sizeof p)) == -1) {
							   perror("talker: sendto");
							   exit(1);
						   }
	totalSends++;
	// Envia "[Finished]" para indicar que enviou a mensagem toda 
	if ((numbytes = sendto(sockfd, "[Finished]", strlen("[Finished]"), 0,
	                       (struct sockaddr *)&p, sizeof p)) == -1) {
							   perror("talker: sendto");
							   exit(1);
						   }
	totalSends++;
}

//Imprime tudo sobre todas as disciplinas
void print_all_disciplines(int sockfd, struct sockaddr_storage p){
	// abrir descritor de arquivo
	FILE *f;
	discipline d;
	char entry[MAX_ENTRY_SIZE];
	char to_send[MAX_ENTRY_SIZE];


	if((f = fopen("./db.txt", "r")) == NULL){
		fprintf(stderr, "Can't open file.\n\n\n");
		exit(1);
	};

	while(!feof(f)){
		d = empty_discipline;
		memset(entry, 0, sizeof entry);

		// Pega uma entrada da disciplina
		if(fgets(entry, MAX_ENTRY_SIZE, f) != NULL){
			// Testa se a entrada esta removida
			if(entry[0] != removed_entry){
				// Mapeia a entrada para uma estrutura
				map_entry_to_struct(&d, entry);
				// Prepara o texto a ser enviado
				sprintf(to_send, "Cod: %s\nTitulo: %s\nPrograma: %s\nSala de Aula: %s\nHorário: %s\nProx. Aula: %s\n\n--------------------------------------------------------------\n", d.cod, d.title, d.program, d.room, d.time, d.next_class);

				//Para a contagem do tempo
				gettimeofday(&stop_time, NULL);
				resul += (float)(stop_time.tv_sec - start_time.tv_sec);


				//Envia!
				if ((numbytes = sendto(sockfd, to_send, strlen(to_send), 0,
				                       (struct sockaddr *)&p, sizeof p)) == -1) {
										   perror("talker: sendto");
										   exit(1);
									   }
				//Reinicia a contagem do tempo
				gettimeofday(&start_time, NULL);
				totalSends++;
			}
		}
	}

	// Envia "[Finished]" para avisar ao cliente que a mensagem chegou ao fim
	if ((numbytes = sendto(sockfd, "[Finished]", strlen("[Finished]"), 0,
	                       (struct sockaddr *)&p, sizeof p)) == -1) {
							   perror("talker: sendto");
							   exit(1);
						   }
	totalSends++;
	fclose(f);
}

// Imprime todas as infos da disciplina a partir do codigo
void print_discipline_by_code(char cod[CODE_SIZE], int sockfd, struct sockaddr_storage p){
	discipline d;
	char to_send[MAX_ENTRY_SIZE];

	// Procura a disciplina pelo codigo
	if(search_discipline_by_code(cod, &d) == -1){
		// Agora envia o pacote com as informacoes
		sprintf(to_send, "\nDisciplina nao encontrada\n");

		//Para a contagem do tempo
		gettimeofday(&stop_time, NULL);
		resul += (float)(stop_time.tv_sec - start_time.tv_sec);

		if ((numbytes = sendto(sockfd, to_send, strlen(to_send), 0,
		                       (struct sockaddr *)&p, sizeof p)) == -1) {
								   perror("talker: sendto");
								   exit(1);
							   }
		totalSends++;
		// Envia "[Finished]" para avisar ao cliente que a mensagem chegou ao fim
		if ((numbytes = sendto(sockfd, "[Finished]", strlen("[Finished]"), 0,
		                       (struct sockaddr *)&p, sizeof p)) == -1) {
								   perror("talker: sendto");
								   exit(1);
							   }
		totalSends++;
		return;
	}

	sprintf(to_send, "\nCod: %s\nTitulo: %s\nPrograma: %s\nSala de Aula: %s\nHorário: %s\nProx. Aula: %s\n\n--------------------------------------------------------------\n", d.cod, d.title, d.program, d.room, d.time, d.next_class);

	//Para a contagem do tempo
	gettimeofday(&stop_time, NULL);
	resul += (float)(stop_time.tv_sec - start_time.tv_sec);


	if ((numbytes = sendto(sockfd, to_send, strlen(to_send), 0,
	                       (struct sockaddr *)&p, sizeof p)) == -1) {
							   perror("talker: sendto");
							   exit(1);
						   }
	totalSends++;
	// Envia "[Finished]" para avisar ao cliente que a mensagem chegou ao fim
	if ((numbytes = sendto(sockfd, "[Finished]", strlen("[Finished]"), 0,
	                       (struct sockaddr *)&p, sizeof p)) == -1) {
							   perror("talker: sendto");
							   exit(1);
						   }
	totalSends++;

}

// Imprime os comentarios da proxima aula a partir do codigp
void print_next_class_by_code(char cod[CODE_SIZE], int sockfd, struct sockaddr_storage p){
	discipline d;
	char to_send[MAX_ENTRY_SIZE];

	if(search_discipline_by_code(cod, &d) == -1){
		// Caso a disciplina nao seja encontrada
		// Agora envia o pacote com as informacoes
		sprintf(to_send, "\nDisciplina nao encontrada\n");

		//Para a contagem do tempo
		gettimeofday(&stop_time, NULL);
		resul += (float)(stop_time.tv_sec - start_time.tv_sec);

		if ((numbytes = sendto(sockfd, to_send, strlen(to_send), 0,
		                       (struct sockaddr *)&p, sizeof p)) == -1) {
								   perror("talker: sendto");
								   exit(1);
							   }
		totalSends++;
		// Envia "[Finished]" para avisar ao cliente que a mensagem chegou ao fim
		if ((numbytes = sendto(sockfd, "[Finished]", strlen("[Finished]"), 0,
		                       (struct sockaddr *)&p, sizeof p)) == -1) {
								   perror("talker: sendto");
								   exit(1);
							   }
		totalSends++;
		return;
	}
	sprintf(to_send, "\nProxima Aula da Disciplina: %s\n%s", cod, d.next_class);

	//Para a contagem do tempo
	gettimeofday(&stop_time, NULL);
	resul += (float)(stop_time.tv_sec - start_time.tv_sec);

	if ((numbytes = sendto(sockfd, to_send, strlen(to_send), 0,
	                       (struct sockaddr *)&p, sizeof p)) == -1) {
							   perror("talker: sendto");
							   exit(1);
						   }
	totalSends++;
	// Envia "[Finished]" para avisar ao cliente que a mensagem chegou ao fim
	if ((numbytes = sendto(sockfd, "[Finished]", strlen("[Finished]"), 0,
	                       (struct sockaddr *)&p, sizeof p)) == -1) {
							   perror("talker: sendto");
							   exit(1);
						   }
	totalSends++;
}

// Remove a disciplina que possui o codigo passado como parametro
// Na verdade o que acontece eh que eh atualizado um caractere no banco de dados
// Para indicar que a disciplina foi removida
void remove_discipline_by_code(char cod[CODE_SIZE]){
	// abrir descritor de arquivo
	FILE *f;
	char actual_code[CODE_SIZE+1];
	char entry[MAX_ENTRY_SIZE];
	char valid[3];
	long pos = 0;
	memset(entry, 0, sizeof entry);

	if((f = fopen("./db.txt", "r+")) == NULL){
		fprintf(stderr, "Can't open file.\n\n\n");
		exit(1);
	};
	while(!feof(f)){

		pos = ftell(f);
		if(fgets(valid, 3, f) != NULL && valid[0] == valid_entry){

			if(fgets(actual_code, CODE_SIZE+1, f) != NULL){


				if(strcmp(actual_code, cod) == 0){
					// Achou a disciplina

					if(fseek(f, pos, SEEK_SET) == 0){
						// Voltou ao comeco da disciplina correta
						// Agora remove a disciplina
						fputc((int)removed_entry, f);
					} else{

						perror( "Erro no Seek" );
						printf( "Erro no Seek: %s\n", strerror( errno ) );
						exit(1);
					}
				} else {
					// Nao eh o codigo da disciplina

					if(fgets(entry, MAX_ENTRY_SIZE, f) != NULL){
						memset(entry, 0, sizeof entry);
						continue;
					}
				}		

			}
		} else {
			if(fgets(entry, MAX_ENTRY_SIZE, f) != NULL){
				memset(entry, 0, sizeof entry);
				continue;
			}
		}

	}
	fclose(f);
}

// Salva a disciplina passada como parametro
void save_discipline(discipline d){
	// abrir descritor de arquivo
	FILE *f;	

	if((f = fopen("./db.txt", "a")) == NULL){
		fprintf(stderr, "Can't open file.\n\n\n");
		exit(1);
	};
	fprintf(f, "*|%s|%s|%s|%s|%s|%s", d.cod, d.title, d.room, d.program, d.time, d.next_class);
	fclose(f);
}

// Checa o arquivo "users.txt" se o usuario e senha passados como parametro
// estao corretos. Retorna 1 caso o usuario seja uma professor e 2 caso contrario.
int authenticate(char user[USER_MAXSIZE], char password[PASS_MAXSIZE]){
	FILE *f;
	char temp[30];
	char line[USER_MAXSIZE+PASS_MAXSIZE+5];
	char *pch, *curpos;

	if((f = fopen("./users.txt", "r")) == NULL){
		fprintf(stderr, "Can't open file.\n\n\n");
		exit(1);
	}

	while(!feof(f)){
		// Pega a linha e tenta pegar o user

		if(fgets(line, 99, f) != NULL){


			curpos = line;
			pch = strchr(line, '|');
			memset(temp, 0, sizeof temp); 
			memcpy(temp, line, pch-line);

			if(strcmp(temp, user) == 0){
				//Achou o usuario
				memset(temp, 0, sizeof temp);
				curpos = ++pch;
				pch = strchr(pch, '|');
				memcpy(temp, curpos, pch-curpos);

				if(strcmp(temp, password) == 0){
					// Senha correta
					curpos = ++pch;
					pch = strchr(pch, '\0');
					memset(temp, 0, sizeof temp);
					memcpy(temp, curpos, pch-curpos);

					if(temp[0]  == '1'){	
						return 1;
					} else {
						return 2;
					}
				}
			}
		}
	}

	return 0;

}

// Salva a nota da proxima aula. Primeiro autentica o usuario e depois verifica se este tem permissao para realizar a operacao
void save_next_class_note(char cod[CODE_SIZE], char next_class[NEXT_CLASS_SIZE], int sockfd, struct sockaddr_storage p, char *user, char *pass){
	discipline d;
	char to_send[MAX_ENTRY_SIZE];
	memset(to_send, 0, sizeof to_send);

	if(authenticate(user, pass) != PROFESOR){
		// Envia mensagem de erro caso o usuario nao seja um professor

		sprintf(to_send, "\nAcao permitida somente a professores\n");

		//Para a contagem do tempo
		gettimeofday(&stop_time, NULL);
		resul += (float)(stop_time.tv_sec - start_time.tv_sec);

		if ((numbytes = sendto(sockfd, to_send, strlen(to_send), 0,
		                       (struct sockaddr *)&p, sizeof p)) == -1) {
								   perror("talker: sendto");
								   exit(1);
							   }
		totalSends++;
		// Envia "[Finished]" para avisar ao cliente que a mensagem chegou ao fim
		if ((numbytes = sendto(sockfd, "[Finished]", strlen("[Finished]"), 0,
		                       (struct sockaddr *)&p, sizeof p)) == -1) {
								   perror("talker: sendto");
								   exit(1);
							   }
		totalSends++;

		return;
	}

	if(search_discipline_by_code(cod, &d) == -1){
		// Caso nao tenha achado a disciplina
		sprintf(to_send, "\nDisciplina nao encontrada\n");

		//Para a contagem do tempo
		gettimeofday(&stop_time, NULL);
		resul += (float)(stop_time.tv_sec - start_time.tv_sec);

		if ((numbytes = sendto(sockfd, to_send, strlen(to_send), 0,
		                       (struct sockaddr *)&p, sizeof p)) == -1) {
								   perror("talker: sendto");
								   exit(1);
							   }
		totalSends++;
		// Envia "[Finished]" para avisar ao cliente que a mensagem chegou ao fim
		if ((numbytes = sendto(sockfd, "[Finished]", strlen("[Finished]"), 0,
		                       (struct sockaddr *)&p, sizeof p)) == -1) {
								   perror("talker: sendto");
								   exit(1);
							   }
		totalSends++;
		return;
	}
	memset(d.next_class, 0, sizeof d.next_class);
	strcpy(d.next_class, next_class);

	// Caso a disciplina seja encontrada, remove a entrada atual dela e adiciona uma nova
	// com o novo texto para proxima aula
	remove_discipline_by_code(cod);
	save_discipline(d);

	// Envia mensagem de sucesso
	sprintf(to_send, "\nOperacao realizada com sucesso.\n");

	//Para a contagem do tempo
	gettimeofday(&stop_time, NULL);
	resul += (float)(stop_time.tv_sec - start_time.tv_sec);

	if ((numbytes = sendto(sockfd, to_send, strlen(to_send), 0,
	                       (struct sockaddr *)&p, sizeof p)) == -1) {
							   perror("talker: sendto");
							   exit(1);
						   }
	totalSends++;
	// Envia "[Finished]" para avisar ao cliente que a mensagem chegou ao fim
	if ((numbytes = sendto(sockfd, "[Finished]", strlen("[Finished]"), 0,
	                       (struct sockaddr *)&p, sizeof p)) == -1) {
							   perror("talker: sendto");
							   exit(1);
						   }
	totalSends++;

}

int main(void)
{
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	int rv;
	int numbytes;
	struct sockaddr_storage their_addr;
	char buf[MAXDATASIZE];
	socklen_t addr_len;
	char s[INET6_ADDRSTRLEN];

	char code[6];
	char next_class[NEXT_CLASS_SIZE+1];
	char pass[PASS_MAXSIZE+1];
	char user[USER_MAXSIZE+1];
	char *pch;
	int choice;
	int new_fd;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC; 
	hints.ai_socktype = SOCK_DGRAM; // Mostra que sera utilizado datagramas
	hints.ai_flags = AI_PASSIVE; // Pega o meu IP

	// Passamos para a funcao getaddrinfo o hostname (que eh nulo pois eh o servidor)
	// a porta que iremos utilizar, a estrutura com as especificacoes e um ponteiro para uma lista
	if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// percorremos a lista e tentamos "bindar" com o primeiro que conseguirmos
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
		                     p->ai_protocol)) == -1) {
								 perror("listener: socket");
								 continue;
							 }

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("listener: bind");
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "listener: failed to bind socket\n");
		return 2;
	}

	freeaddrinfo(servinfo);

	// Loop principal
	while(1) {  
		addr_len = sizeof their_addr;

		memset(buf, 0, sizeof buf);

		// Esperamos alguma mensagem chegar pelo socket, quando chegar colocamos o seu conteudo
		// em "buf" e guardamos as informacoes de quem enviou (na estrutura their_addr)
		if ((numbytes = recvfrom(sockfd, buf, MAXDATASIZE-1 , 0,
		                         (struct sockaddr *)&their_addr, &addr_len)) == -1) {
									 perror("recvfrom");
									 exit(1);
								 }
		//Inicia a contagem do tempo
		gettimeofday(&start_time, NULL);

		// Incrementamos a contagem de mensagens recebidas
		totalReceivs++;

		buf[numbytes] = '\0';


		// Pega a opcao recebida
		sscanf(buf, "%d", &choice);

		if(choice < 3 && choice > 0){
			// Nao precisa de codigo da disciplina

		}else if(choice > 2 && choice < 7){

			//  Pega o codigo da disciplina na string que recebeu do socket
			pch = getCodeFromString(buf, &code);

			if(choice == 6){
				// Pegar usuario e senha
				memset(user, 0, sizeof user);
				memset(pass, 0, sizeof pass);
				memset(next_class, 0, sizeof next_class);
				pch = getUserAndPassFromString(pch, &user, &pass);
				getNextClassFromString(pch, &next_class);
			}

		}

		// A partir da opcao do usuario manda para o caso correto
		switch(choice){

			case 1:
				print_code_title_all_disciplines(sockfd, their_addr);
				break;
			case 2:
				print_all_disciplines(sockfd, their_addr);
				break;
			case 3:
				print_program_by_code(code, sockfd, their_addr);
				break;
			case 4:
				print_discipline_by_code(code, sockfd, their_addr);
				break;
			case 5:
				print_next_class_by_code(code, sockfd, their_addr);
				break;
			case 6:
				save_next_class_note(code, next_class, sockfd, their_addr, user, pass);
				break;
			default:
				break;


		}

		//Para a contagem do tempo
		gettimeofday(&stop_time, NULL);
		
		resul += (stop_time.tv_usec - start_time.tv_usec)/(float)MICRO_PER_SECOND;

		// Escreve no arquivo
		FILE *f;
		if((f = fopen("./servidor.txt", "a")) == NULL){
			fprintf(stderr, "Can't open file.\n\n\n");
			exit(1);
		};
		
		fprintf(f, "%f\n", resul);
		fclose (f);
		resul = 0;

	}

	// Fecha o socket antes de terminar o programa
	close(sockfd);


	// Imprime a quantidade de mensagens recebidas e enviadas
	fprintf(stderr, "\nTotal de mensagens recebidas: %d\n", totalReceivs);
	fprintf(stderr, "\nTotal de mensagens enviadas: %d\n", totalSends);
	return 0;
}