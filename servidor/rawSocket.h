/*
 * rawSocket.h
 *
 *  Created on: May 3, 2014
 *      Author: paulo
 */

#ifndef RAWSOCKET_H_
#define RAWSOCKET_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <net/ethernet.h>
#include <netdb.h>
#include <linux/if_arp.h>
#include <unistd.h>
#include <sys/time.h>
#include <poll.h>
#include <dirent.h>
#include <sys/statvfs.h>

#define MAX_DADOS 15
#define MAX_MSG 21
#define MAX_COMANDO 50
#define MAX_ARG 15
#define MAX_TENTATIVAS 6
#define MAX_SEQ 15
#define MAX_ARQ 53300823 //máximo real pode ser 11529215044606846975

#define CDR 0 //cd remoto
#define LSR 1 //ls remoto
#define PUT 2
#define GET 3
#define CAT 4
#define CDL 5 //cd local
#define LSL 6 //ls local
#define DADOS 7
#define TAM_ARQ 8
#define EXIBE 9
#define ACK 10
#define FIM_ARQ 11
#define INVALIDO 12 //comando não existe
#define SUCESSO 13
#define ERRO 14
#define NACK 15

//VER ESSA PARADA DOS TIPOS, TEORICAMENTE SÓ PODERIAM TER 16 TIPOS (0 ~ 15)

#define byteInicio 0x7E;

#define ERRO_DIR 0 //diretorio inexistente
#define ERRO_PER 1 //permissao negada
#define ERRO_ESP 2 //espaco insuficiente
#define ERRO_ARQ 3 //arquivo inexistente
#define ERRO_MAX 4 //arquivo possui tamanho maior que o máximo possível

typedef unsigned char byte;

typedef struct sockaddr_ll endereco;

typedef enum {
	mensagemRecebida, timeout, error, fim, lixo
} tipoEvento;

typedef struct {
	byte inicio;
	byte tamanho : 4;
	byte sequencia : 4;
	byte tipo : 4;
	byte paridade : 4;
	byte dados[MAX_DADOS];

} mensagem;

typedef struct {
	int tipo;
	char comando[MAX_COMANDO];
	char argumento[MAX_ARG];
} linhaComando;

int tentativas;
int fd; //tem que ver pra que serve isso
int rede_habilitada;
int result_enviar, result_receber;
struct pollfd p; //tem que ver pra que serve isso
struct stat buf; //tem que ver pra que serve isso
endereco enderecoSocket;

byte buffer[MAX_MSG];

char * call_getcwd(void);

void habilitar_camada_rede(void);
void desabilitar_camada_rede(void);

void espera_resposta(tipoEvento *evento, mensagem *m, mensagem *r);
void espera_evento(tipoEvento *evento, mensagem *r);
void setar_endereco(void);

void incrementa_sequencia(byte *seq);

byte calcula_paridade(mensagem *m);

void exibe_erro(mensagem *m);

void envia_mensagem(mensagem *m);

void cria_mensagem(mensagem *m, byte sequencia, byte tipo, char *dados);

long calcula_tamanho_arquivo(char *nome);

char * le_arquivo(char *nome);

void envia_arquivo(char *nome, byte tipo, byte prox_enviar, byte prox_receber);
void recebe_arquivo(mensagem *m, char *nome, byte tipo, byte prox_enviar, byte prox_receber);

void imprime_mensagem(mensagem *m);

/* ir implementando
 void constroiPacoteNACK(frame *s,seq_nr n);
 void constroiPacoteERRO(frame *s,seq_nr next,char kindErro);
 void constroiPacoteTAM_ARQ(frame *s,seq_nr next, unsigned int tam_arq);
 void constroiPacoteMaxData(frame *f,FILE* fp);
 void constroiPacoteData(frame *f, int tam, FILE *fp);
 */

#endif /* RAWSOCKET_H_ */
