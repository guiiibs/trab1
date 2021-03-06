/*
 * rawSocketCliente.c
 *
 *  Created on: May 3, 2014
 *      Author: paulo
 */

#include "rawSocket.h"

void comando_cd_remoto(char *argumento) {
	mensagem m, r;
	byte prox_receber = 0;
	int fim = 0;
	tipoEvento evento;


	cria_mensagem(&m, 0, CDR, argumento); //cria solicitação do cd
	envia_mensagem(&m); //envia a mensagem

	do {
		espera_resposta(&evento, &m, &r);
		if (evento == mensagemRecebida) {
			if (r.sequencia == prox_receber) {
				if (r.tipo == SUCESSO || r.tipo == ERRO) {
					fim = 1;
					cria_mensagem(&m, 1, ACK, "");
					envia_mensagem(&m);
				} else if (r.tipo == NACK) {
					incrementa_sequencia(&prox_receber);
					envia_mensagem(&m);
				}
			} else if (r.sequencia > prox_receber) {
				fim = 1; //sai do while
				printf("Mensagem com sequência maior do que a esperada, comando não executado...\n");
			}
		} else if (evento == timeout) { //se o evento for um timeout
			fim = 1; //sai do while
			printf("Timeout, comando não executado...\n");
		}
	} while (!fim);

	if (r.tipo == ERRO) {
		printf("Erro ao executar comando: ");
		exibe_erro(&r);
		//fwrite(r.dados, r.tamanho, 1, stdout); //escreve os dados na tela
		//printf("\n");
	} else if (r.tipo == SUCESSO) {
		printf("Comando executado com sucesso.\n");
	}
}

void comando_cd_local(char *argumento) {
	chdir(argumento);
}

void comando_ls_remoto(char *argumento) {
	mensagem m;

	cria_mensagem(&m, 0, LSR, argumento); //cria ls
	envia_mensagem(&m); //envia solicitação do ls

	recebe_arquivo(&m, "", EXIBE, 1, 0);
	printf("\n");
}

void comando_ls_local(char *argumento) {
	if (argumento != NULL && !strcmp(argumento, "")) {
		system("ls");
	} else if (!strcmp(argumento, "-l")) {
		system("ls -l");
	} else if (!strcmp(argumento, "-la")) {
		system("ls -la");
	} else if (!strcmp(argumento, "-a")) {
		system("ls -a");
	}
}

void comando_put(char *argumento) {
	int fim, temPermissao, deuErro = 1, enviouTam = 0;
	unsigned int tam;
	char *tamHexa = malloc(MAX_DADOS + 1);
	mensagem m, r;
	byte prox_enviar = 0, prox_receber = 0;
	FILE *fp;
	struct stat fileStat;
	tipoEvento evento;

	printf("executou: put %s\n", argumento);

	fp = fopen(argumento, "r");
	if (fp != NULL) {
		fstat(fileno(fp), &fileStat);
		temPermissao = (fileStat.st_mode & S_IROTH); //verifica a permissão
		tam = fileStat.st_size; //recebe tamanho do arquivo

		if (tam > MAX_ARQ) { //se tamanho for maior que o máximo possível
			printf("tamanho arquivo maior que o máximo.\n");
			return;
		} else if (temPermissao && fp) { //se tiver permissão
			deuErro = 0; //tem que esperar resposta
		} else if (!temPermissao) { //se não tiver permissão
			printf("usuário sem permissão\n");
			return;
		} else if (errno == ENOENT) { //se arquivo não existe
			printf("arquivo inexistente\n");
			return;
		}
	} else { //se arquivo não existe (acho que o else if de cima só funciona para diretórios)
		printf("arquivo inexistente\n");
		return;
	}

	if (fp != NULL) {
		fclose(fp);
	}

	cria_mensagem(&m, prox_enviar, PUT, argumento);
	envia_mensagem(&m);
	incrementa_sequencia(&prox_enviar);

	if (!deuErro) {
		fim = 0;
		do {
			espera_resposta(&evento, &m, &r);
			if (evento == mensagemRecebida) {
				if (r.sequencia == prox_receber) {
					if (r.tipo == ACK) {
						if (!enviouTam) {
							sprintf(tamHexa, "%X", tam); //'tamHexa' recebe 'tam' em hexa
							cria_mensagem(&m, prox_enviar, TAM_ARQ, tamHexa);
							envia_mensagem(&m);
							incrementa_sequencia(&prox_receber);
							incrementa_sequencia(&prox_enviar);
							enviouTam = 1;
						} else {
							fim = 1; //se deu erro é só pra esperar o ack e encerrar
							incrementa_sequencia(&prox_receber);
							envia_arquivo(argumento, DADOS, prox_enviar, prox_receber); //se não deu erro, envia o arquivo
						}
					} else if (r.tipo == NACK) {
						incrementa_sequencia(&prox_receber);
						envia_mensagem(&m);
					} else if (r.tipo == ERRO) {
						fim = 1;
						exibe_erro(&m);
						//printf("Cliente não possui espaço para receber o arquivo.\n"); //único erro que pode receber
					}
				} else if (r.sequencia > prox_receber) {
					fim = 1; //sai do while
					printf("Mensagem com sequência maior do que a esperada, comando não executado...\n");
				}
			} else if (evento == timeout) { //se o evento for um timeout
				fim = 1; //sai do while
				printf("Timeout, comando não executado...\n");
			} else if (evento == error) {
				cria_mensagem(&m, prox_enviar, NACK, "");
				envia_mensagem(&m);
				incrementa_sequencia(&prox_enviar); //incrementa prox_enviar e prox_receber
				incrementa_sequencia(&prox_receber);
			}
		} while (!fim);
	} else {
		printf("deu um erro inesperado, não era pra ter acontecido isso\n");
	}
}

long long espaco_livre_diretorio_corrente() {
	struct statvfs fiData;
	char *diretorio = call_getcwd();

	if((statvfs(diretorio, &fiData)) < 0 ) {
		return -1;
	} else {
		return fiData.f_bsize * fiData.f_bfree;
	}
}

void comando_get(char *argumento) {
	mensagem m, r;
	byte prox_enviar = 0, prox_receber = 0;
	tipoEvento evento;
	char erro[2];
	int fim, i;
	long long tamArquivo, espacoLivre;
	char *hexa = (char *) malloc(MAX_DADOS);

	cria_mensagem(&m, prox_enviar, GET, argumento);
	envia_mensagem(&m);
	incrementa_sequencia(&prox_enviar);

	fim = 0;
	do {
		espera_resposta(&evento, &m, &r);
		if (evento == mensagemRecebida) {
			if (r.sequencia == prox_receber) {
				if (r.tipo == TAM_ARQ) {
					for (i = 0; i < r.tamanho; i++) {
						hexa[i] = r.dados[i];
					}
					tamArquivo = strtol(hexa, (char**) NULL, 16);
					espacoLivre = espaco_livre_diretorio_corrente();

					if (tamArquivo < espacoLivre && espacoLivre > 0) {
						cria_mensagem(&m, prox_enviar, ACK, "");
						envia_mensagem(&m);
						fim = 1;
						recebe_arquivo(&m, argumento, DADOS, prox_enviar, prox_receber);
					} else { //não tem espaço para receber
						erro[0] = ERRO_ESP; //cria mensagem de erro e envia
						erro[1] = '\0';
						cria_mensagem(&m, prox_enviar, ERRO, erro);
						envia_mensagem(&m);
						fim = 1;
					}
					incrementa_sequencia(&prox_enviar);
					incrementa_sequencia(&prox_receber);
				} else if (r.tipo == NACK) {
					incrementa_sequencia(&prox_receber);
					envia_mensagem(&m);
				} else if (r.tipo == ERRO) {
					cria_mensagem(&m, prox_enviar, ACK, "");
					envia_mensagem(&m);
					fim = 1;
					exibe_erro(&r);
					//printf("Cliente não possui espaço para receber o arquivo.\n"); //único erro que pode receber
				}
			} else if (r.sequencia > prox_receber) {
				fim = 1; //sai do while
				printf("Mensagem com sequência maior do que a esperada, comando não executado...\n");
			}
		} else if (evento == timeout) { //se o evento for um timeout
			fim = 1; //sai do while
			printf("Timeout, comando não executado...\n");
		} else if (evento == error) {
			cria_mensagem(&m, prox_enviar, NACK, "");
			envia_mensagem(&m);
			incrementa_sequencia(&prox_enviar); //incrementa prox_enviar e prox_receber
			incrementa_sequencia(&prox_receber);
		}
	} while (!fim);
}

void comando_cat(char *argumento) {
	mensagem m;

	cria_mensagem(&m, 0, CAT, argumento);
	envia_mensagem(&m);
	recebe_arquivo(&m, "", EXIBE, 1, 0);
	printf("\n");
}

linhaComando leitura_comando(void) {
	linhaComando lComando;
	char *buf;
	char *comandoAux, *argumentoAux, *lComandoAux;

	comandoAux = (char *) malloc(100);
	lComandoAux = (char *) malloc(100);
	argumentoAux = (char *) malloc(100);

	lComando.argumento[0] = '\0'; //só pra limpar

	buf = call_getcwd();
	strcat(buf, "$ ");
	printf("%s", buf);

	fflush(stdin);

	lComando.tipo = INVALIDO;

	fgets(lComandoAux, 100, stdin);

	sscanf(lComandoAux, "%s %s", comandoAux, argumentoAux);

	if (strlen(comandoAux) > 0) {
		if (strlen(comandoAux) > MAX_ARG || strlen(argumentoAux) > MAX_ARG) {
			printf(
					"Comando e argumento devem ter no máximo %d caracteres cada.\n",
					MAX_ARG);
		} else {
			strcpy(lComando.comando, comandoAux);
			strcpy(lComando.argumento, argumentoAux);

			if (!strcmp(lComando.comando, "cdr")) {
				lComando.tipo = CDR;
			} else if (!strcmp(lComando.comando, "cdl")) {
				lComando.tipo = CDL;
			} else if (!strcmp(lComando.comando, "lsr")) {
				lComando.tipo = LSR;
			} else if (!strcmp(lComando.comando, "lsl")) {
				lComando.tipo = LSL;
			} else if (!strcmp(lComando.comando, "put")) {
				lComando.tipo = PUT;
			} else if (!strcmp(lComando.comando, "get")) {
				lComando.tipo = GET;
			} else if (!strcmp(lComando.comando, "cat")) {
				lComando.tipo = CAT;
			} else {
				printf("%s: Comando inválido\n", comandoAux);
			}
		}
	}

	return lComando;
}

void programa_cliente(void) {
	int fim = 0;
	linhaComando comando;

	do {
		comando = leitura_comando();

		switch (comando.tipo) {
		case CDR:
			comando_cd_remoto(comando.argumento);
			break;
		case CDL:
			comando_cd_local(comando.argumento);
			break;
		case LSR:
			comando_ls_remoto(comando.argumento);
			break;
		case LSL:
			comando_ls_local(comando.argumento);
			break;
		case PUT:
			comando_put(comando.argumento);
			break;
		case GET:
			comando_get(comando.argumento);
			break;
		case CAT:
			comando_cat(comando.argumento);
			break;
		}

	} while (!fim);
}

