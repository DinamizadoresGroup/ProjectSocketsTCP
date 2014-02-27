#include <sys/types.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/wait.h>



/**
*	Nombre: Main (cliente proyecto)
*	Uso: Este software se conectará a la IP indicada por el puerto indicado y mostrará las posiciones de sus vecinos
*	
*	Argumentos: argv[1] --> IP del servidor al cual nos conectaremos
*				argv[2] --> Puerto a través del cual el software realizará las conexiones
*				argv[3] --> Número de clientes a crear
*	Return: 0 si todo el ciclo siguió bien, -1 si ocurrió algún error.
*/

int main(int argc, char *argv[])
{
	int pid;
	int status;
	char *arguments[3] = {argv[1], argv[2],NULL};
	int i;
	if(argc != 4)
	{
		printf("Número de argumentos inválido1\n");
		return 1;
	}
	else
	{
		char *ipServer = argv[1];  		//Obtenemos la ip del equipo a preguntar
		int puerto = atoi(argv[2]);
		for ( i = 0 ; i < atoi(argv[3]) ; i++ )
		{
			pid = fork();

			switch ( pid )
			{
				case -1:
				perror("Error en el fork1\n");
				exit(-1);
				break;

				case 0:
				//execvp("./cliente0.3-alpha", arguments);
				execl("./cliente0.4-alpha", argv[1], argv[1],argv[2]);
				perror("Error en el fork2\n");
				exit(-1);
				break;


				

			}
		}
		wait(&status);
				if(status == 0)
				{
					printf( "El hijo ejecuta\n" );
				}

	}
	return 0;
}


