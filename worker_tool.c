#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "parce.h"

#define PORT 3550 /* El puerto que será abierto */
#define BACKLOG 2 /* El número de conexiones permitidas */
#define VHOSTDIR "/etc/httpd/sites.d/"
#define BUFFER_SIZE 1024

/********************************
 *	FUNCIONES		*
 ********************************/

void clear_buffer(char *buffer_rx){
	int i;
	for (i=0;i<BUFFER_SIZE;i++){
		buffer_rx[i] = '\0';
	}
}

int add_site(char *buffer_rx){
	/* Agrega el archivo de configuracion de un sitio
	   al directorio /etc/httpd/sites.d */

	char user_id[100];
	char sus_id[100];
	char site_name[100];
	char default_domain[100];
	char alias[200];
	FILE *fd;
	char site_path[100];

	int pos = 3;
	parce_data(buffer_rx,&pos,&user_id[0]);
	parce_data(buffer_rx,&pos,&sus_id[0]);
	parce_data(buffer_rx,&pos,&site_name[0]);
	parce_data(buffer_rx,&pos,&default_domain[0]);
	parce_data(buffer_rx,&pos,&alias[0]);

	printf("Datos del sitio a ser agregado:\n");
	printf("	user_id:	%s\n",&user_id);
	printf("	sus_id:		%s\n",&sus_id);
	printf("	site_name:	%s\n",&site_name);
	printf("	default_domain:	%s\n",&default_domain);
	printf("	alias:		%s\n",&alias);

	strcpy(site_path,VHOSTDIR);
	strcat(site_path,site_name);
	strcat(site_path,".conf");

	fd = fopen(site_path,"w");
	fprintf(fd,"<VirtualHost *:80>\n");
	fprintf(fd,"	DocumentRoot /websites/%s/%s/%s/wwwroot\n",
	user_id,sus_id,site_name);
	fprintf(fd,"	ServerName %s.%s\n",site_name,default_domain);
	fprintf(fd,"	ServerAlias %s\n",alias);
	fprintf(fd,"	CustomLog /websites/%s/%s/%s/logs/access.log combined\n",
	user_id,sus_id,site_name);
	fprintf(fd,"	ErrorLog /websites/%s/%s/%s/logs/error.log\n",
	user_id,sus_id,site_name);
	fprintf(fd,"");
	fprintf(fd,"</VirtualHost>\n");
	fclose(fd);

	return 1;
}

void del_site(){
	/* Elimina la configuracion de un sitio */
}

int worker_check(){
	/* Retorna si un worker esta en condiciones de
	   estar online */
}

/********************************
 * 	MAIN			*
 ********************************/

int main(int argc , char *argv[]){

	int fd_server, fd_client; /* los ficheros descriptores */
	int sin_size;
	struct sockaddr_in server;
	struct sockaddr_in client;
	char buffer_rx[BUFFER_SIZE];
	char buffer_tx[BUFFER_SIZE];
	char taskid[2];
	char action;
	int result;
	int pos;

	if ((fd_server=socket(AF_INET, SOCK_STREAM, 0)) == -1 ) {  
		printf("error en socket()\n");
		return 1;
	}
	server.sin_family = AF_INET;
	server.sin_port = htons(PORT);
	server.sin_addr.s_addr = INADDR_ANY;
	//bzero(&(server.sin_zero),8);

	if(bind(fd_server,(struct sockaddr*)&server, sizeof(struct sockaddr))<0) {
		printf("error en bind() \n");
		return 1;
	}

	if(listen(fd_server,BACKLOG) == -1) {  /* llamada a listen() */
		printf("error en listen()\n");
		return 1;
	}
	sin_size=sizeof(struct sockaddr_in);

	while(1){
		printf("Esperando mensaje desde el cliente()\n");
		if ((fd_client = accept(fd_server,(struct sockaddr *)&client,&sin_size))<0) {
			printf("error en accept()\n");
			return 1;
		}
		printf("PASO\n");
		//printf("Se obtuvo una conexión desde %s\n", inet_ntoa(client.sin_addr) );
		clear_buffer(buffer_rx);
		printf("PASO2\n");
		recv(fd_client,buffer_rx,BUFFER_SIZE,0);
		printf("Recibimos -%s-\n",buffer_rx);
		/* Obtenemos el id de tarea. Las tareas en los workers son sincronas */
		parce_get_taskid(&buffer_rx[0], &taskid[0]);
		printf("taskid: %lu\n",taskid);
		strcpy(buffer_tx,taskid);
		/* Obtenemos la accion a realizar */
		action = parce_get_action(&buffer_rx[0]);
		printf("action: %c\n",action);

		switch(action){
			case 'A':
				printf("Agregamos sitio\n");
				if(add_site(&buffer_rx[0])){
					strcat(buffer_tx,"1");
				} else {
					strcat(buffer_tx,"0");
				}
				send(fd_client,buffer_tx, BUFFER_SIZE,0);
				break;
			case 'D':
				printf("Implementar\n");
				break;
			case 'C':
				printf("Implementar\n");
				break;
		}

		close(fd_client);
	}
}
