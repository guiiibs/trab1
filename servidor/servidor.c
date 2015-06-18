/*
 * servidor.c
 *
 *  Created on: May 6, 2014
 *      Author: paulo
 */

#include "rawSocketServidor.h"

int main() {
	habilitar_camada_rede();
	programa_servidor();
	desabilitar_camada_rede();
	return 0;
}
