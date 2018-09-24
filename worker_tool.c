#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include "send_receive.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include "parce.h"

#define PORT 3550 /* El puerto que será abierto */
#define BACKLOG 1 /* El número de conexiones permitidas */
#define VHOSTDIR "/etc/httpd/sites.d/"
#define ROLE_BUFFER_SIZE 25
#define ROLE_HEADER_SIZE 8

/********************************
 *	Estructura		*
 ********************************/
struct config{
	char default_domain[200];
};


/********************************
 *	FUNCIONES		*
 ********************************/

httpd_systemctl(int action, char **send_message, uint32_t *send_message_size){	//0 stop, 1 start, 2 reload
	FILE *fp;
	char status[2];

	*send_message_size=2;
	*send_message=(char *)realloc(*send_message,*send_message_size);
	switch(action){
		case 0:
			fp = popen("systemctl stop httpd 2>/dev/null; echo $?", "r");
			break;
		case 1:
			fp = popen("systemctl start httpd 2>/dev/null; echo $?", "r");
			break;
		case 2:
			fp = popen("systemctl reload httpd 2>/dev/null; echo $?", "r");
			break;
		default:
			strcpy(*send_message,"0");
	}
	fgets(status, sizeof(status)-1, fp);
	pclose(fp);
	strcpy(*send_message,"1");
}

void delete_site(char *rcv_message, char **send_message, uint32_t *send_message_size){
	char command[200];
	char site_name[100];
	char status[2];
	FILE *fp;
	int pos=2;

	parce_data(rcv_message,'|',&pos,site_name);

	sprintf(command,"rm /etc/httpd/sites.d/%s.conf 2>/dev/null; echo $?",site_name);
	printf("Ejecutando comando: %s\n",command);
	fp = popen(command, "r");
	fgets(status, sizeof(status)-1, fp);
	pclose(fp);

	*send_message_size = 2;
	*send_message = (char *)realloc(*send_message,*send_message_size);
	
	sprintf(*send_message,"1");
}

//int add_site(char *buffer_rx, int fd_client){
void add_site(char *rcv_message, char **send_message, uint32_t *send_message_size,
	      struct config *c){
	/* Agrega el archivo de configuracion de un sitio
	   al directorio /etc/httpd/sites.d */

	char site_id[20];	//El 5to es el '\0'
	char site_ver[20];	//El 3ro es el '\0'
	char site_name[100];
	char dir[10];
	char aux[200];
	char *aux_list = NULL;
	int aux_list_size;
	int pos = 1;
	int posaux;
	FILE *fd;
	char site_path[100];

	printf("paso 1:%s\n",rcv_message);
	parce_data(rcv_message,'|',&pos,site_id);
	printf("paso 2\n");
	parce_data(rcv_message,'|',&pos,site_name);
	printf("paso 3\n");
	parce_data(rcv_message,'|',&pos,dir);
	printf("paso 4\n");
	parce_data(rcv_message,'|',&pos,site_ver);
	printf("paso 5\n");

	printf("Datos del sitio a ser agregado:\n");
	printf("	site_id:	%s\n",site_id);
	printf("	site_ver	%s\n",site_ver);
	printf("	site_name:	%s\n",site_name);

	// Comenzamos a armar el archivo del vhost
	strcpy(site_path,VHOSTDIR);
	strcat(site_path,site_name);
	strcat(site_path,".conf");

	*send_message_size = 2;
	*send_message = (char *)realloc(*send_message,*send_message_size);

	if(fd = fopen(site_path,"w")){
		fprintf(fd,"#ID %s\n",site_id);
		fprintf(fd,"#VER %s\n",site_ver);
		fprintf(fd,"<Directory /websites/%s/%s/wwwroot>\n",dir,site_name);
		fprintf(fd,"	AllowOverride All\n");
		fprintf(fd,"	Require all granted\n");
		fprintf(fd,"</Directory>\n");
		fprintf(fd,"<VirtualHost *:80>\n");
		fprintf(fd,"	DocumentRoot /websites/%s/%s/wwwroot\n",
		dir,site_name);
		fprintf(fd,"	ServerName %s.%s\n",site_name,c->default_domain);
	
		printf("Preparado para los alias\n");
		aux_list = (char *)malloc(strlen(rcv_message) - pos + 1);
		parce_data(rcv_message,'|',&pos,aux_list);
		aux_list_size = strlen(aux_list);
		printf("Tenemos alias para agregar: %s\n",aux_list);
		posaux=0;
		while(posaux < aux_list_size){
			parce_data(aux_list,',',&posaux,aux);
			printf("tenemos alias -%s-\n",aux);
			fprintf(fd,"	ServerAlias %s\n",aux);
		}
	
		printf("Preparamos los indices\n");
		aux_list = (char *)malloc(strlen(rcv_message) - pos + 1);
		parce_data(rcv_message,'|',&pos,aux_list);
		aux_list_size = strlen(aux_list);
		posaux=0;
		while(posaux<aux_list_size){
			parce_data(aux_list,',',&posaux,aux);
			printf("tenemos index -%s-\n",aux);
			fprintf(fd,"	DirectoryIndex %s\n",aux);
		}
		
		fprintf(fd,"	CustomLog /websites/%s/%s/logs/access.log combined\n",
		dir,site_name);
		fprintf(fd,"	ErrorLog /websites/%s/%s/logs/error.log\n",
		dir,site_name);
		fprintf(fd,"");
		fprintf(fd,"</VirtualHost>\n");
		fclose(fd);
		
		sprintf(*send_message,"1");
	} else {
		sprintf(*send_message,"0");
	}
}

void purge(char **send_message, int *send_message_size){
	/* Elimina todos los archivos de virtual host de los sitios */
	
	FILE *fp;
	char aux[5];

	*send_message_size=2;
	*send_message=(char *)realloc(*send_message,*send_message_size);
	fp = popen("rm -f /etc/httpd/sites.d/*.conf 2>/dev/null; echo $?", "r");
	if (fp == NULL) {
		printf("Failed to run command\n" );
		sprintf(*send_message,"0");
	}
	fgets(aux, sizeof(aux)-1, fp);
	pclose(fp);
	sprintf(*send_message,"1");

}

void get_sites(char **send_message, uint32_t *send_message_size){
	/* Retorna un listado de los id de los sitios que posee
	   cargados. El primer dato es un indicador de si a continuacion
	   se enviaran mas datos. 1 por Si, 0 por no. */

	FILE *fp;
	char aux[50];
	uint32_t total_size;
	char site_id_char[5]; //el 5to es el '\0'
	unsigned long site_id;

	printf("Solicitan listado de sitios\n");

	fp = popen("cat /etc/httpd/sites.d/*.conf 2>/dev/null | grep ID | awk '{print $2}'", "r");
	if (fp == NULL) {
		printf("Failed to run command\n" );
		*send_message_size = 2;
		*send_message=(char *)realloc(*send_message,*send_message_size);
		sprintf(*send_message,"0");
	} else {
		*send_message_size = 100;
		*send_message=(char *)realloc(*send_message,*send_message_size);
		strcpy(*send_message,"1");
		total_size = 1;
		while (fgets(aux, sizeof(aux)-1, fp) != NULL){
			if(*send_message_size < strlen(aux) + total_size + 1){
				*send_message_size += 100;
				*send_message=(char *)realloc(*send_message,*send_message_size);
			}
			aux[strlen(aux)-1] = '\0';	//Quita el enter al final de la linea
			strcat(*send_message,aux);
			strcat(*send_message,"|");
			total_size += strlen(aux) + 1;	//El +1 es por el '|'
		}
		pclose(fp);
		*send_message_size = total_size + 1;
		printf("*send_message = %s\n",*send_message);
		printf("*send_message_size = %i\n",*send_message_size);

		*send_message=(char *)realloc(*send_message,*send_message_size);
	}
}

void check(char **send_message, uint32_t *send_message_size){
	/* Retorna si un worker esta en condiciones de
	   estar online y tambien estadisticas del mismo */
	FILE *fp;
	char status = '1';
	char detalle[300];
	char buffer[200];
	char aux[200];

	/* Verificar que este montado via NFS el filer */
	/* Verificar que el httpd este corriendo */
	strcpy(detalle,"todo OK"); // De entrada esta todo bien
	fp = popen("systemctl status httpd > /dev/null; echo $?", "r");
	if (fp == NULL) {
		printf("Fallo al obtener el estado del apache\n" );
		strcpy(detalle,"No se pudo determinar si el proceso httpd esta corriendo|");
		status = '0';
	}
	fgets(buffer, sizeof(buffer)-1, fp);
	pclose(fp);

	if(buffer[0] != '0'){
		strcpy(detalle,"httpd caido");
		status = '0';
	}
	*send_message_size = strlen(detalle) + 3; //el 3 incluye el estado , el "|" y el \0
	*send_message=(char *)realloc(*send_message,*send_message_size);
	sprintf(*send_message,"%c%s|",status,detalle);

	/* Para la CPU */
	fp = popen("uptime | awk '{print $10\"|\"$11\"|\"$12\"|\"}' | sed 's\\,\\\\g'", "r");
	fgets(buffer, sizeof(buffer)-1, fp);
	strcpy(aux,buffer);
	aux[strlen(aux) - 1] = '\0';
	pclose(fp);

	/* Para la CPU */
	fp = popen("vmstat | tail -1 | awk '{print $14\"|\"$13\"|\"$15\"|\"$16\"|\"}'", "r");
	fgets(buffer, sizeof(buffer)-1, fp);
	strcat(aux,buffer);
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

	*send_message_size += strlen(aux);
	*send_message=(char *)realloc(*send_message,*send_message_size);
	strcat(*send_message,aux);
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
	char *rcv_message=NULL;
	int rcv_message_size;
	char *send_message=NULL;
	int send_message_size;
	char action;
	struct config c;
	
	/* Par ael futuro debe o bien leerlo de un archivo o
 	 * bien recibir la configuracion del controller*/
	strcpy(c.default_domain,"fibercorp.com.ar");

	if ((fd_server=socket(AF_INET, SOCK_STREAM, 0)) == -1 ) {  
		printf("error en socket()\n");
		return 1;
	}
	server.sin_family = AF_INET;
	server.sin_port = htons(PORT);
	server.sin_addr.s_addr = INADDR_ANY;

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

		while(recv_all_message(fd_client,&rcv_message,&rcv_message_size)){
			printf("Recibimos -%s-\n",rcv_message);
			/* Obtenemos la accion a realizar */
			action = rcv_message[0];
			switch(action){
				case 'A':
					printf("Agregamos sitio\n");
					add_site(rcv_message,&send_message,&send_message_size,&c);
					break;
				case 'd':
					printf("Eliminamos sitio\n");
					delete_site(rcv_message,&send_message,&send_message_size);
					break;
				case 'G':
					get_sites(&send_message,&send_message_size);
					break;
				case 'D':
					printf("Implementar\n");
					break;
				case 'P':
					printf("Eliminamos configuracion del apache\n");
					purge(&send_message,&send_message_size);
					break;
				case 'C':
					printf("Chequeamos el worker\n");
					check(&send_message,&send_message_size);
					break;
				case 'S':
					/* Start apache */
					httpd_systemctl(0,&send_message,&send_message_size);
					break;
				case 'K':
					/* Stop apache */
					httpd_systemctl(1,&send_message,&send_message_size);
					break;
				case 'R':
					/* Reload apache */
					httpd_systemctl(2,&send_message,&send_message_size);
					break;
				default :
					printf("Error protocolo\n");
					send_message_size=2;
					send_message = (char *)realloc(send_message,send_message_size);
					sprintf(send_message,"0");
			}
			printf("ENVIANDO bytes:%i mensaje:%s\n",send_message_size,send_message);
			send_all_message(fd_client,send_message,send_message_size);
		}
		close(fd_client);
	}
}
