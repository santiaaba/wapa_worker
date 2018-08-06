#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "parce.h"

#define PORT 3550 /* El puerto que será abierto */
#define BACKLOG 1 /* El número de conexiones permitidas */
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
	char site_id[5];	//El 5to es el '\0'
	char site_ver[3];	//El 3ro es el '\0'
	char site_name[100];
	char default_domain[100];
	char alias[200];
	FILE *fd;
	char site_path[100];

	int pos = 5;
	parce_data(buffer_rx,&pos,&user_id[0]);
	parce_data(buffer_rx,&pos,&sus_id[0]);
	parce_data(buffer_rx,&pos,&site_name[0]);
	parce_data(buffer_rx,&pos,&site_id[0]);
	parce_data(buffer_rx,&pos,&site_ver[0]);
	parce_data(buffer_rx,&pos,&default_domain[0]);
	parce_data(buffer_rx,&pos,&alias[0]);

	printf("Datos del sitio a ser agregado:\n");
	printf("	site_id:	%lu\n",site_id);
	printf("	site_ver	%u\n",site_ver);
	printf("	user_id:	%lu\n",user_id);
	printf("	sus_id:		%lu\n",sus_id);
	printf("	site_name:	%s\n",site_name);
	printf("	default_domain:	%s\n",default_domain);
	printf("	alias:		%s\n",alias);

	strcpy(site_path,VHOSTDIR);
	strcat(site_path,site_name);
	strcat(site_path,".conf");

	fd = fopen(site_path,"w");
	fprintf(fd,"#ID %lu\n",site_id);
	fprintf(fd,"#VER %u\n",site_ver);
	fprintf(fd,"<VirtualHost *:80>\n");
	fprintf(fd,"	DocumentRoot /websites/%lu/%lu/%s/wwwroot\n",
	user_id,sus_id,site_name);
	fprintf(fd,"	ServerName %s.%s\n",site_name,default_domain);
	fprintf(fd,"	ServerAlias %s\n",alias);
	fprintf(fd,"	CustomLog /websites/%lu/%lu/%s/logs/access.log combined\n",
	user_id,sus_id,site_name);
	fprintf(fd,"	ErrorLog /websites/%lu/%lu/%s/logs/error.log\n",
	user_id,sus_id,site_name);
	fprintf(fd,"");
	fprintf(fd,"</VirtualHost>\n");
	fclose(fd);

	return 1;
}

int get_sites(int fd_client){
	/* Retorna un listado de los id de los sitios que posee
	   cargados. en la variable last_site_id retorna el
	   ultimo site_id cargado en el buffer si es que se
	   supera el tamano del mismo. Si no quedan mas entocnes
	   se retorna 0 */
	FILE *fp;
	char buffer_tx[BUFFER_SIZE];
	char buffer[50];
	char site_id_char[5]; //el 5to es el '\0'
	unsigned long site_id;

	printf("Solicitan listado de sitios\n");

	fp = popen("cat /etc/httpd/sites.d/*.conf 2>/dev/null | grep ID | awk '{print $2}'", "r");
	if (fp == NULL) {
		printf("Failed to run command\n" );
		return 0;
	}
	// Suponemos que vamos a necesitar una sola transmicion
	strcpy(buffer_tx,"1|0|");
	while (fgets(buffer, sizeof(buffer)-1, fp) != NULL){
		printf("Dato a armar %s\n",buffer);
		/* Si last_site_id es distinto de 0 entonces debemos
 		 * avanzar hasta encontrar donde nos quedamos */
		if(strlen(buffer_tx)+strlen(buffer)+2 >= BUFFER_SIZE){
			/* Enviamos lo que tenemos de momento. Como quedan datos
 			 * atualizamos el char al inicio */
			buffer_tx[2] = '1';
			printf("Estamos en while enviando -%s-\n",buffer_tx);
			send(fd_client,buffer_tx,BUFFER_SIZE,0);
			/* Quedamos essperando que el controller acepte mas datos */
			recv(fd_client,buffer_tx,BUFFER_SIZE,0);
			if(buffer_tx[0] == 1){
				/* El controller acepta mas datos */
				strcpy(buffer_tx,"1|0|");
			} else {
				/* Por algun motivo el controller no acepta mas datos */
				return 0;
			}
		} else {
			/* Seguimos metiendo datos en buffer_tx */
			strcat(buffer_tx,buffer);strcat(buffer_tx,"|");
		}
	}
	/* Quedaron datos remanentes. Cambiamos el primer char a 0.
 	 * para indicar que no hay mas datos */
	strcpy(buffer_tx,"1|0|");
	printf("Estamos fin enviando -%s-\n",buffer_tx);
	send(fd_client,buffer_tx,BUFFER_SIZE,0);
	printf("cerramos el pipe\n");
	pclose(fp);
	return 1;
}

void del_site(){
	/* Elimina la configuracion de un sitio */
}

int check(char *detalle){
	/* Retorna si un worker esta en condiciones de
	   estar online y tambien estadisticas del mismo */
	FILE *fp;
	char buffer[100];
	int status = 1;

	/* Verificar que este montado via NFS el filer */
	/* Verificar que el httpd este corriendo */
	fp = popen("systemctl status httpd > /dev/null; echo $?", "r");
	if (fp == NULL) {
		printf("Fallo al obtener el estado del apache\n" );
		strcpy(detalle,"No se pudo determinar si el proceso httpd esta corriendo|");
		status = 0;
	}
	fgets(buffer, sizeof(buffer)-1, fp);
	pclose(fp);
	if(buffer[0] != '0'){
		printf("Proceso apache caido\n");
		strcpy(detalle,"httpd caido");
		status = 0;
	}
}

void statistics(char *aux){
	/* Obtiene estadisticas del worker */
	FILE *fp;
	char buffer[100];

	/* Para la CPU */
	fp = popen("vmstat | tail -1 | awk '{print \"|\"$14\"|\"$13\"|\"$15\"|\"$16\"|\"}'", "r");
	fgets(buffer, sizeof(buffer)-1, fp);
	strcpy(aux,buffer);
	aux[strlen(aux) - 1] = '\0';
	pclose(fp);

	/* Para la memoria */
	fp = popen("free | tail -2 | head -1 | awk '{print $2\"|\"$3\"|\"}'", "r");
	fgets(buffer, sizeof(buffer)-1, fp);
	strcat(aux,buffer);
	aux[strlen(aux) - 1] = '\0';
	pclose(fp);

	/* Para la swap */
	fp = popen("free | tail -1 | awk '{print $2\"|\"$3}'", "r");
	fgets(buffer, sizeof(buffer)-1, fp);
	strcat(aux,buffer);
	aux[strlen(aux) - 1] = '\0';
	pclose(fp);
}

int repare(){
	/* intenta reparar el worker */
	/* retorna 1 si tuvo exito. 0 en caso contrario */
	return 1;
}

/********************************
 * 	MAIN			*
 ********************************/

int main(int argc , char *argv[]){

	int fd_server, fd_client; /* los ficheros descriptores */
	int sin_size;
	int cant_bytes;
	struct sockaddr_in server;
	struct sockaddr_in client;
	char buffer_rx[BUFFER_SIZE];
	char buffer_tx[BUFFER_SIZE];
	unsigned int last_site_id;
	char aux[200];
	char action;
	int result;
	int pos;
	struct timeval tv;
	
	tv.tv_sec = 3;
	tv.tv_usec = 0;

	if ((fd_server=socket(AF_INET, SOCK_STREAM, 0)) == -1 ) {  
		printf("error en socket()\n");
		return 1;
	}
	server.sin_family = AF_INET;
	server.sin_port = htons(PORT);
	server.sin_addr.s_addr = INADDR_ANY;
	//bzero(&(server.sin_zero),8);
	//setsockopt(fd_server, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

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
		printf("Esperando conneccion desde el cliente()\n"); //Debemos mantener viva la conexion
		if ((fd_client = accept(fd_server,(struct sockaddr *)&client,&sin_size))<0) {
			printf("error en accept()\n");
			return 1;
		}

		// Aguardamos continuamente que el cliente envie un comando
		clear_buffer(buffer_rx);
		while(recv(fd_client,buffer_rx,BUFFER_SIZE,0)>0){
			printf("Recibimos -%s-\n",buffer_rx);
			/* Obtenemos la accion a realizar */
			action = buffer_rx[0];
			switch(action){
				case 'A':
					printf("Agregamos sitio\n");
					if(add_site(buffer_rx)){
						buffer_tx[0] = '1';
					} else {
						buffer_tx[0] = '0';
					}
					printf("Enviamos -%s-\n",buffer_tx);
					write(fd_client,buffer_tx, BUFFER_SIZE);
					break;
				case 'G':
					get_sites(fd_client);
					break;
				case 'D':
					printf("Implementar\n");
					break;
				case 'C':
					printf("Chequeamos el worker\n");
					if(check(aux) == 1){
						buffer_tx[0] = '1';
					} else {
						buffer_tx[0] = '0';
					}
					buffer_tx[1] = '|'; buffer_tx[2] = '\0';
					strcat(buffer_tx,aux);
					statistics(aux);
					strcat(buffer_tx,aux);
					
					printf("Esperando que el cliente reciba los datos\n");
					cant_bytes = send(fd_client,buffer_tx, BUFFER_SIZE,0);
					printf("Enviamos(%i) -%s-\n",cant_bytes,buffer_tx);
					break;
				default :
					printf("Error protocolo\n");
					send(fd_client,"0\0",BUFFER_SIZE,0);
			}
			printf("Volvemos a esperar un mesane\n");
		}
		close(fd_client);
	}
}
