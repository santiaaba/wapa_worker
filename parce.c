#include "parce.h"

void parce_data(char *buffer, char sep, int *i, char *value){
	/* Dado un comando recibido desde el servidor con formato
	   de campos separados por sep, retorna el valor a partir
	   de la posición dada hasta el proximo sep o fin del string.
	   Retorna ademas la posición fin en i. los datos comienzan
	   en la posicion 3*/

	int j = 0;
	int largo;

	largo = strlen(buffer);
	while(*i < largo && buffer[*i] != sep && buffer[*i] != '\0'){
		value[j] = buffer[*i];
		j++; (*i)++;
	}
	value[j]='\0';
	/* Adelantamos la i una posicion ya que se
 	   encuentra parada en el caracter sep posiblemente */
	(*i)++;
}
