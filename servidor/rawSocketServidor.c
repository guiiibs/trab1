#include "rawSocket.h"

void comando_cd(mensagem *m) {
	struct stat st;
	int i;
	char *argumento = (char *) malloc(MAX_DADOS + 1);
	char erro[2];
	DIR *d;
	int permissao, fim;
	mensagem e, r;
	tipoEvento evento;
	byte prox_receber = 1;

	for(i = 0; i < m->tamanho; i++) {
		argumento[i] = m->dados[i];
	}
	argumento[i] = '\0';

	d = opendir(argumento);
	stat(argumento, &st);

	permissao = (st.st_mode & S_IROTH);

	if (permissao && d) {
		cria_mensagem(&e, 0, SUCESSO, "");
		chdir(argumento);
		//printf("cd %s\n", argumento);
	} else if (!permissao) {
		erro[0] = ERRO_PER;
		erro[1] = '\0';
		cria_mensagem(&e, 0, ERRO, erro);
		//printf("Sem permissão para abrir o diretório '%s'\n", argumento);
	} else if (errno == ENOENT) {
		erro[0] = ERRO_DIR;
		erro[1] = '\0';
		cria_mensagem(&e, 0, ERRO, erro);
		//printf("Diretório '%s' não existe\n", argumento);
	}

	envia_mensagem(&e);

	fim = 0;
	do {
		espera_resposta(&evento, &e, &r);
		if (evento == mensagemRecebida) {
			if (r.sequencia == prox_receber) {
				if (r.tipo == ACK) {
					fim = 1;
					printf("executou: cd %s\n", argumento);
				} else if (r.tipo == NACK) {
					incrementa_sequencia(&prox_receber);
					envia_mensagem(&e);
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

	free(argumento);
	free(d);
}

void comando_ls(mensagem *m) {
	char *argumento = (char *) malloc(MAX_DADOS);
	int i = 0;

	//copia para "argumento" a área de dados da mensagem recebida
	for (; i < m->tamanho; i++) {
		argumento[i] = m->dados[i];
	}
	argumento[i] = '\0';

	//verifica qual "tipo" de ls foi solicitado, executa e imprime o comando executado
	if (!strcmp(argumento, "-a")) {
		system("ls -a > .lsResult");
		printf("executou: ls -a\n");
	} else if (!strcmp(argumento, "-l")) {
		system("ls -l > .lsResult");
		printf("executou: ls -l\n");
	} else if (!strcmp(argumento, "-la")) {
		system("ls -la > .lsResult");
		printf("executou: ls -la\n");
	} else {
		system("ls > .lsResult");
		printf("executou: ls\n");
	}

	envia_arquivo(".lsResult", EXIBE, 0, 1); //envia o arquivo com a saída do ls
	system("rm .lsResult"); //remove o arquivo temporário
}

//comando get envia arquivo que cliente está solicitando
void comando_get(mensagem *m) {
	char *argumento = (char *) malloc(MAX_DADOS + 1);
	int i = 0, fim, temPermissao, deuErro = 1;
	unsigned int tam;
	char *erro, *tamHexa = malloc(MAX_DADOS + 1);
	mensagem r;
	byte prox_enviar = 0, prox_receber = 1;
	FILE *fp;
	struct stat fileStat;
	tipoEvento evento;

	erro = (char *) malloc(3);

	for (; i < m->tamanho; i++) {
		argumento[i] = m->dados[i];
	}
	argumento[i] = '\0';

	printf("executou: get %s\n", argumento);

	fp = fopen(argumento, "r");
	if (fp != NULL) {
		fstat(fileno(fp), &fileStat);
		temPermissao = (fileStat.st_mode & S_IROTH); //verifica a permissão
		tam = fileStat.st_size; //recebe tamanho do arquivo

		if (tam > MAX_ARQ) { //se tamanho for maior que o máximo possível
			//erro[0] = '4'; //cria mensagem de erro e envia
			//erro[1] = '\0';
			strcpy(erro, "4");
			cria_mensagem(m, prox_enviar, ERRO, erro);
			envia_mensagem(m);
			incrementa_sequencia(&prox_enviar);
			deuErro = 1;
			printf("tamanho arquivo maior que o máximo.\n");
		} else if (temPermissao && fp) { //se tiver permissão
			sprintf(tamHexa, "%X", tam); //'tamHexa' recebe 'tam' em hexa
			cria_mensagem(m, prox_enviar, TAM_ARQ, tamHexa); //cria mensagem como o tamanho do arquivo e envia
			envia_mensagem(m);
			incrementa_sequencia(&prox_enviar);
			deuErro = 0; //tem que esperar resposta
		} else if (!temPermissao) { //se não tiver permissão
			//erro[0] = ERRO_PER; //cria mensagem de erro e envia
			//erro[1] = '\0';
			strcpy(erro, "1");
			cria_mensagem(m, prox_enviar, ERRO, erro);
			envia_mensagem(m);
			incrementa_sequencia(&prox_enviar);
			deuErro = 1;
			printf("usuário sem permissão\n");
		} else if (errno == ENOENT) { //se arquivo não existe
			//erro[0] = ERRO_ARQ; //cria mensagem de erro e envia
			//erro[1] = '\0';
			strcpy(erro, "3");
			cria_mensagem(m, prox_enviar, ERRO, erro);
			envia_mensagem(m);
			incrementa_sequencia(&prox_enviar);
			deuErro = 1;
			printf("arquivo inexistente\n");
		}
	} else { //se arquivo não existe (acho que o else if de cima só funciona para diretórios)
		//erro[0] = ERRO_ARQ; //cria mensagem de erro e envia
		//erro[1] = '\0';
		strcpy(erro, "3");
		cria_mensagem(m, prox_enviar, ERRO, erro);
		envia_mensagem(m);
		incrementa_sequencia(&prox_enviar);
		deuErro = 1;
		printf("arquivo inexistente\n");
	}

	fim = 0;
	do {
		espera_resposta(&evento, m, &r);
		if (evento == mensagemRecebida) {
			if (r.sequencia == prox_receber) {
				if (r.tipo == ACK) {
					fim = 1; //se deu erro é só pra esperar o ack e encerrar
					if (!deuErro) { //se não deu erro, envia o arquivo
						incrementa_sequencia(&prox_receber);
						envia_arquivo(argumento, DADOS, 0, 1); //se não deu erro, envia o arquivo
					}
				} else if (r.tipo == NACK) {
					incrementa_sequencia(&prox_receber);
					envia_mensagem(m);
				} else if (r.tipo == ERRO) {
					fim = 1;
					exibe_erro(m);
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
			cria_mensagem(m, prox_enviar, NACK, "");
			envia_mensagem(m);
			incrementa_sequencia(&prox_enviar); //incrementa prox_enviar e prox_receber
			incrementa_sequencia(&prox_receber);
		}
	} while (!fim);

	if (fp != NULL) {
		fclose(fp);
	}
}

void comando_cat(mensagem *m) {
	char *argumento = (char *) malloc(m->tamanho);
	int i = 0, fim;
	char *comando = (char *) malloc(40);
	FILE *fp;
	mensagem r;
	tipoEvento evento;
	byte prox_enviar = 0, prox_receber = 1;

	//copia dados da mensagem para variável argumento
	for (; i < m->tamanho; i++) {
		argumento[i] = m->dados[i];
	}
	argumento[i] = '\0';

	printf("executou: cat %s\n", argumento); //imprime o comando que vai ser executado

	fp = fopen(argumento, "r");
	if (fp == NULL) { //arquivo não existe
		cria_mensagem(m, prox_enviar, ERRO, "3"); //erro arq não existe
		envia_mensagem(m);
		fim = 0;
		do {
			espera_resposta(&evento, m, &r);
			if (evento == mensagemRecebida) {
				if (r.sequencia == prox_receber) {
					if (r.tipo == ACK) {
						fim = 1; //se deu erro é só pra esperar o ack e encerrar
					} else if (r.tipo == NACK) {
						incrementa_sequencia(&prox_receber);
						envia_mensagem(m);
					}
				} else if (r.sequencia > prox_receber) {
					fim = 1; //sai do while
					printf("Mensagem com sequência maior do que a esperada, comando não executado...\n");
				}
			} else if (evento == timeout) { //se o evento for um timeout
				fim = 1; //sai do while
				printf("Timeout, comando não executado...\n");
			} else if (evento == error) {
				cria_mensagem(m, prox_enviar, NACK, "");
				envia_mensagem(m);
				incrementa_sequencia(&prox_enviar); //incrementa prox_enviar e prox_receber
				incrementa_sequencia(&prox_receber);
			}
		} while (!fim);
		printf("arquivo %s não existe\n", argumento);
	} else {
		//monta o comando a ser executado: cat "argumento" > .catResult
		strcpy(comando, "cat ");
		strcat(comando, argumento);
		strcat(comando, " > .catResult");

		system(comando); //executa o comando

		envia_arquivo(".catResult", EXIBE, prox_enviar, prox_receber); //envia o arquivo com a saida do cat

		system("rm .catResult");
		fclose(fp);
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

//comando put recebe arquivo que o cliente está enviando
void comando_put(mensagem *m) {
	mensagem r;
	byte prox_enviar = 0, prox_receber = 1;
	tipoEvento evento;
	char erro[2];
	int fim, i;
	long long tamArquivo, espacoLivre;
	char *hexa = (char *) malloc(MAX_DADOS), *argumento = (char *) malloc(MAX_DADOS + 1);

	//copia nome do arquivo para variável argumento
	for (i = 0; i < m->tamanho; i++) {
		argumento[i] = m->dados[i];
	}
	argumento[i] = '\0';

	printf("executou put %s\n", argumento);

	cria_mensagem(m, prox_enviar, ACK, "");
	envia_mensagem(m);
	incrementa_sequencia(&prox_enviar);

	fim = 0;
	do {
		espera_resposta(&evento, m, &r);
		if (evento == mensagemRecebida) {
			if (r.sequencia == prox_receber) {
				if (r.tipo == TAM_ARQ) {
					for (i = 0; i < r.tamanho; i++) {
						hexa[i] = r.dados[i];
					}
					tamArquivo = strtol(hexa, (char**) NULL, 16);
					espacoLivre = espaco_livre_diretorio_corrente();

					if (tamArquivo < espacoLivre && espacoLivre > 0) {
						cria_mensagem(m, prox_enviar, ACK, "");
						envia_mensagem(m);
						fim = 1;
						incrementa_sequencia(&prox_enviar);
						incrementa_sequencia(&prox_receber);
						recebe_arquivo(m, argumento, DADOS, prox_enviar, prox_receber);
					} else { //não tem espaço para receber
						erro[0] = ERRO_ESP; //cria mensagem de erro e envia
						erro[1] = '\0';
						cria_mensagem(m, prox_enviar, ERRO, erro);
						envia_mensagem(m);
						fim = 1;
						incrementa_sequencia(&prox_enviar);
						incrementa_sequencia(&prox_receber);
					}
				} else if (r.tipo == NACK) {
					incrementa_sequencia(&prox_receber);
					envia_mensagem(m);
				} else if (r.tipo == ERRO) {
					cria_mensagem(m, prox_enviar, ACK, "");
					envia_mensagem(m);
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
			cria_mensagem(m, prox_enviar, NACK, "");
			envia_mensagem(m);
			incrementa_sequencia(&prox_enviar); //incrementa prox_enviar e prox_receber
			incrementa_sequencia(&prox_receber);
		}
	} while (!fim);
}

void programa_servidor(void) {
	tipoEvento evento;
	mensagem m;

	do {
		espera_evento(&evento, &m);

		if (evento == mensagemRecebida) {
			if (m.sequencia == 0) {
				switch (m.tipo) {
					case LSR:
						comando_ls(&m);
						break;
					case CDR:
						comando_cd(&m);
						break;
					case PUT:
						comando_put(&m);
						break;
					case GET:
						comando_get(&m);
						break;
					case CAT:
						comando_cat(&m);
						break;
					default:
						break;
				}
			}
		}

	} while (1);
}
