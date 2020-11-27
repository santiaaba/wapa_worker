#include "send_receive.h"

void int_to_4bytes(uint32_t *i, char *_4bytes){
	memcpy(_4bytes,i,4);
}

void _4bytes_to_int(char *_4bytes, uint32_t *i){
	memcpy(i,_4bytes,4);
}

int recv_all_message(int fd_client, char **rcv_message, uint32_t *rcv_message_size){
	/* Coordina la recepcion de mensajes grandes del cliente */
	/* El encabezado es de 1 char */
	char buffer[ROLE_BUFFER_SIZE];
	char printB[ROLE_BUFFER_SIZE+1];
	int first_message=1;
	uint32_t parce_size;
	int c=0;	// Cantidad de bytes recibidos

	// Al menos una vez vamos a ingresar
	*rcv_message_size=0;
	 do{
		printf("Esperamos nuevo mensaje\n");
		if(recv(fd_client,buffer,ROLE_BUFFER_SIZE,0)<=0){
			return 0;
		}
		if(first_message){
			first_message=0;
			_4bytes_to_int(buffer,rcv_message_size);
			*rcv_message=(char *)realloc(*rcv_message,*rcv_message_size);
		}
		_4bytes_to_int(&(buffer[4]),&parce_size);
		memcpy(*rcv_message+c,&(buffer[ROLE_HEADER_SIZE]),parce_size);
		c += parce_size;
	} while(c < *rcv_message_size);
	printf("RECV Mesanje: %s\n",*rcv_message);
	return 1;
}

int send_all_message(int fd_client, char *send_message, uint32_t send_message_size){
	/* Coordina el envio de los datos aun cuando se necesita
	 * mas de una transmision */

	char buffer[ROLE_BUFFER_SIZE];
	char printB[ROLE_BUFFER_SIZE+1];
	int c=0;	//Cantidad byes enviados
	uint32_t parce_size;

	 printf("Enviaremos al CORE: %i,%s\n",send_message_size, send_message);

	if( send_message[send_message_size-1] != '\0'){
		printf("cloud_send_receive: ERROR. send_message no termina en \\0\n");
		return 0;
	}

	/* Los 4 primeros bytes del header es el tamano total del mensaje */
	int_to_4bytes(&send_message_size,buffer);

	while(c < send_message_size){
		if(send_message_size - c + ROLE_HEADER_SIZE < ROLE_BUFFER_SIZE){
			/* Entra en un solo buffer */
			parce_size = send_message_size - c ;
		} else {
			/* No entra todo en el buffer */
			parce_size = ROLE_BUFFER_SIZE - ROLE_HEADER_SIZE;
		}
		int_to_4bytes(&parce_size,&(buffer[4]));
		memcpy(buffer + ROLE_HEADER_SIZE,send_message + c,parce_size);
		c += parce_size;
		if(send(fd_client,buffer,ROLE_BUFFER_SIZE,0)<0){
			printf("ERROR a manejar\n");
			return 0;
		}
	}
}
