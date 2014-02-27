#include <sys/types.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>


/**
*	Estructuras y tipos auxiliares
*/
struct trama{
	unsigned short tipo;
	unsigned short x;
	unsigned short y;
	unsigned short z;
	unsigned short origen;
	double tiempo;
};

/**
*	Prototipos de funciones
*/
int crearSocket(int tipo);
int enlazaServer(char *ipServer, int puerto, struct sockaddr_in *servidor);
int conectaServer(int socket_cliente, struct sockaddr_in *servidor);
int cerrarSocket(int socket_cliente);
int leerTrama(int socket_cliente, struct trama *mensaje);
void generarTramaPunto(struct trama *mensaje);
int enviarPosicion(int socket_cliente, struct trama mensaje);
int inicializaConjunto(fd_set *conjunto, int *sockClients, int num);


/**
*	Nombre: Main (cliente proyecto)
*	Uso: Este software se conectará a la IP indicada por el puerto indicado y mostrará las posiciones de sus vecinos
*	
*	Argumentos: argv[1] --> IP del servidor al cual nos conectaremos
*				argv[2] --> Puerto a través del cual el software realizará las conexiones
*
*	Return: 0 si todo el ciclo siguió bien, -1 si ocurrió algún error.
*/

int main(int argc, char *argv[])
{
	if(argc < 1)
	{
		printf("Número de argumentos inválido\n");
		return 1;
	}
	else
	{
		char *ipServer = argv[1];  		//Obtenemos la ip del equipo a preguntar
		int puerto = atoi(argv[2]);		//Puerto que pasamos como argumento el cual casteamos de char* a int
		int socket_cliente;          	//Socket con el que nos conectaremos
		struct sockaddr_in servidor; 	//Dirección del servidor
		struct trama mensaje;			//Variable tipo trama que usaremos para el intercambio de datos
		unsigned short nclientes, nvecinos, nciclos;	//Numero de vecinos totales, vecinos por grupo y de ciclos para la simulación
		int contCiclos = 0, i;
		int contPuntos = 0;
		int contACK = 0;
		struct timeval tiempoInicial, tiempoCiclo, tiempoRespuesta;
		struct timespec timeInicial, timeCiclo, timeRespuesta;
		double acumuladorCiclo = 0, acumuladorRespuesta = 0;
		double acumuladorCiclo2 = 0, acumuladorRespuesta2 = 0;

		srand(time(NULL));

		/**
		*	FLUJO: Creamos el socket de escucha
		*/
		socket_cliente = crearSocket(0);
		if(socket_cliente == -1)
		{
			return -1;
		}
		printf("\n Socket creado en el puerto %d \n", socket_cliente);

		/**
		*	FLUJO: Enlazamos nuestro socket de escucha a la IP del servidor
		*/
		if(enlazaServer(ipServer, puerto, &servidor) == -1)
		{
			return -1;
		}
		printf("\n Direccion server correctamente enlazada \n");

		/**
		*	FLUJO: Realizamos una conexión mediante el socket al servidor previamente enlazado
		*/
		if(conectaServer(socket_cliente, &servidor) == -1)
		{
			return -1;
		}
		printf("\nConectado al server\n");

		/**
		*	FLUJO: Comprobamos que nos llegue la trama con señal de START, sino abortamos la simulación
		*/
		if(leerTrama(socket_cliente, &mensaje) == -1)
		{
			printf("Error al leer la trama\n");
		}

		printf("\nSTART recibido, tipo: %d, n: %d, v: %d, c: %d, origen: %d\n", mensaje.tipo, mensaje.x, mensaje.y, mensaje.z, mensaje.origen);

		if(mensaje.tipo != 0)
		{
			printf("El primer mensaje recibido no es START, simulación abortada.\n");
			return -1;
		}

		/**
		*	FLUJO: Obtenemos de la señal de START los parámetros para la simulación
		*/
		nclientes = mensaje.x;
		nvecinos = mensaje.y;
		nciclos = mensaje.z;
		while(contCiclos < nciclos)
		{
			/**
			*	FLUJO: Una vez llegada la señal de START generamos el primer punto a enviar
			*/
			generarTramaPunto(&mensaje);
			contPuntos = 0;
			contACK = 0;
			gettimeofday(&tiempoInicial, NULL);
			clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &timeInicial);
			/**
			*	FLUJO: Enviamos una posición al servidor. HITO 3
			*/
			if(enviarPosicion(socket_cliente, mensaje) == -1)
			{
				printf("Simulación Abortada\n");
				return -1;
			}
			int n_aux = 1;
			while(contPuntos < ((nvecinos - 1) * 2))
			{
				struct trama mensajeaux;
				n_aux = recv(socket_cliente, &mensajeaux, sizeof(struct trama), 0);
				if(n_aux < 0)
				{
					printf("\nError al leer del buffer\n");
					return -1;
				}
				printf("Mensaje recibido, tipo: %d, n: %d, v: %d, c: %d, origen: %d\n", mensajeaux.tipo, mensajeaux.x, mensajeaux.y, mensajeaux.z, mensajeaux.origen);
				//Construimos el ACK poniendo el tipo a 2, y lo enviamos al server
				switch(mensajeaux.tipo)
				{
					//Si nos llega un punto
					case 1:
						mensajeaux.tipo = 2;
						mensajeaux.x = 1000;
						mensajeaux.y = 1000;
						mensajeaux.z = 1000;
						printf("Enviamos ACK\n");
						if(enviarPosicion(socket_cliente, mensajeaux) == -1)
						{
							printf("Simulación Abortada\n");
							return -1;
						}

						contPuntos++;
						break;

					//Si nos llega un ACK
					case 2:
						printf("ACK Recibido\n");
						contPuntos++;
						contACK++;
						if(contACK == (nvecinos - 1)){
							if(gettimeofday(&tiempoRespuesta, NULL) < 0)
							{
								printf("Error al hacer el gettimeofday de respuesta\n");
							}
							acumuladorRespuesta += (tiempoRespuesta.tv_usec - tiempoInicial.tv_usec);
							clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &timeRespuesta);
							acumuladorRespuesta2 += ((timeRespuesta.tv_sec - timeInicial.tv_sec) + (timeRespuesta.tv_nsec * 1.0e-9 - timeInicial.tv_nsec * 1.0e-9));
						}
						break;

				}
				

				
			}
			if(gettimeofday(&tiempoCiclo, NULL) < 0)
			{
				printf("Error al hacer el gettimeofday de ciclo\n");
			}
			
			acumuladorCiclo += (tiempoCiclo.tv_usec - tiempoInicial.tv_usec);

			clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &timeCiclo);
			acumuladorCiclo2 += ((timeCiclo.tv_sec - timeInicial.tv_sec) + (timeCiclo.tv_nsec * 1.0e-9 - timeInicial.tv_nsec * 1.0e-9));
			contCiclos++;

		}

		printf("\nTerminamos ciclos\n");
		struct trama fin;
		fin.tipo = 3;
		fin.x = 1000;
		fin.y = 1000;
		fin.z = 1000;
		fin.origen = 65535;
		fin.tiempo = acumuladorCiclo2/nciclos;

		enviarPosicion(socket_cliente, fin);
		int aux;
		aux = recv(socket_cliente, &fin, sizeof(struct trama), 0);
		if(aux < 0)
		{
			printf("\nError al leer del buffer\n");
			return -1;
		}
		printf("Mensaje recibido, tipo: %d, n: %d, v: %d, c: %d, origen: %d\n", fin.tipo, fin.x, fin.y, fin.z, fin.origen);
		printf("Tiempo medio de respuesta: %f \nTiempo medio de ciclo: %f\n", acumuladorRespuesta, acumuladorCiclo);
		printf("Tiempo medio de respuesta2: %f \nTiempo medio de ciclo2: %f\n", acumuladorRespuesta2/nciclos, acumuladorCiclo2/nciclos);

		/*printf("El cliente se desconecta\n");

		if(cerrarSocket(socket_cliente) == -1)
		{
			return -1;
		}*/
	}

	return 0;
}


/**
*	Definición de funciones
*/
/**
*	Nombre: crearSocket
*	Uso: Crea un socket
*
*	Argumentos: int tipo --> Si 0 el tipo será TCP, si !=0 será UDP
*	Return: El descriptor del socket creado, -1 si ocurre error.
*/
int crearSocket(int tipo)
{
	int socket_cliente = -1;

	if(tipo == 0)
	{
		socket_cliente = socket(PF_INET, SOCK_STREAM, 0);
		
		if(socket_cliente < 0)
		{
			printf("Error al crear el socket: TCP Version\n");
			return -1;
		}

		return socket_cliente;
	}
	else
	{
		socket_cliente = socket(PF_INET, SOCK_DGRAM, 0);
		
		if(socket_cliente < 0)
		{
			printf("Error al crear el socket: UDP Version\n");
			return -1;
		}

		return socket_cliente;
	}
}

/**
*	Nombre: enlazaServer
*	Uso: Enlazamos con el cliente el puerto local y la dirección del server
*	Argumentos:	char *ipServer --> La dirección del server al cual nos queremos conectar
*				int puerto     --> El puerto local por el cual nos queremos conectar al servidor
*				struct sockaddr_in *servidor --> La estructura donde almacenamos los datos del servidor (por referencia)
*	Return: El valor de si se ha realizado correctamene el enlace puerto-ip, -1 si ocurre algún error.
*/
int enlazaServer(char *ipServer, int puerto, struct sockaddr_in *servidor)
{

	servidor->sin_family = AF_INET;
	servidor->sin_port = htons(puerto);
	
	if(inet_aton(ipServer, &servidor->sin_addr) == 0)
	{
		printf("\nError al enlazar el puerto con la IP del server \n");
		return -1;
	}

	return 0;
}

/**
*	Nombre: conectaServer
*	Uso: Realizamos conexión entre el puerto especificado con la estrutura del servidor dada. ¡Solo para TCP!
*
*	Argumentos:	int socket_cliente -->	El descriptor del puerto abierto para realizar la conexión
*				struct sockaddr_in *servidor --> La estructura que almacena los datos del servidor	(por referencia)
*	Return: La confirmación de si se ha realizado correctamente
*/
int conectaServer(int socket_cliente, struct sockaddr_in *servidor)
{
	
	if(connect(socket_cliente, (struct sockaddr *) servidor, sizeof(struct sockaddr_in)) < 0)
	{
		printf("\nError al conectar con el servidor\n");
		return -1;
	}

	return 0;
}

/**
*	Nombre: cerrarSocket
*	Uso: Cerramos el puerto asociado al descriptor que le pasamos
*
*	Argumentos: int socket_cliente --> El descriptor del puerto el cual cerraremos
*
*	Return: 0 si se cerró correctamente, -1 si se produjo algún error.
*/
int cerrarSocket(int socket_cliente)
{
	
	if(close(socket_cliente) != 0)
	{
		printf("\n Error al cerrar el socket\n");
		return -1;
	}

	return 0;
}

/**
*	Nombre: leerTrama
*	Uso: Leemos del buffer asociado a un socket una estructura tipo trama
*
*	Argumentos:	int socket_cliente --> El descriptor del puerto del cual queremos leer datos
*				struct trama *mensaje	--> El mensaje tipo trama donde se almacenará el contenido volcado del buffer
*
*	Return:	0 si se ha leido bien, -1 si ha ocurrido algún error.
*/
int leerTrama(int socket_cliente, struct trama *mensaje)
{
	int n_aux = recv(socket_cliente, mensaje, sizeof(struct trama), 0);
	if(n_aux < 0)
	{
		printf("\nError al leer del buffer\n");
		return -1;
	}

	return 0;
}

/**
*	Nombre: generarTramaPunto
*	Uso:	Montar una trama que envíe un punto de coordenadas
*	Argumentos:	struct trama *start 	--> La trama por referencia que montamos como paquete de coordenadas
*/
void generarTramaPunto(struct trama *mensaje)
{
    int posx, posy, posz;

    posx = ((rand() * 10000) % 250);
	if(posx < 0)
	{
		posx = posx * -1;
	}
	posy = ((rand() * 10000) % 250);
	if(posy < 0)
	{
		posy = posy * -1;
	}
	posz = ((rand() * 10000) % 250);
	if(posz < 0)
	{
		posz = posz * -1;
	}

	unsigned short x1 = (unsigned short) posx;
    unsigned short y1 = (unsigned short) posy;
    unsigned short z1 = (unsigned short) posz;

    mensaje->tipo = 1;		//Definimos esta trama como un envío de puntos
    mensaje->x = x1;
    mensaje->y = y1;
    mensaje->z = z1;
    mensaje->origen = 65535;
    
    printf("\nTrama punto generada, tipo: %d, n: %d, v: %d, c: %d, origen: %d\n", mensaje->tipo, mensaje->x, mensaje->y, mensaje->z, mensaje->origen);

    return;
}

/**
*	Nombre: enviarTrama
*	Uso:	Envia por el descriptor de socket indicado un mensaje tipo trama
*	Argumentos:		int socket_cliente 	--> El descriptor del puerto por el cual enviamos la trama
*					struct trama mensaje 	--> La estructura tipo punto la cual queremos enviar
*	Return: 0 si se ha producido correctamente; -1 si ha habido algún error
*/
int enviarPosicion(int socket_cliente, struct trama mensaje)
{
	if(send(socket_cliente, &mensaje, sizeof(struct trama), 0) < 0)
	{
		printf("Error al realizar el envío de la trama. simulación abortada.\n");
		return -1;
	}

	return 0;
}

/**
*	Nombre:	inicializaConjunto
*	Uso: Inicializa el conjunto de los descriptores
*	Argumentos:	fd_set *conjunto 	--> Puntero al conjunto con los descriptores
*				int *sockClients 	--> sockClients con las conexiones aceptadas
*				int num 	--> Numero de conexiones existentes
*	Return:	Valor máximo de los identificadores de las conexiones aceptadas
*/
int inicializaConjunto(fd_set *conjunto, int *sockClients, int num)
{
	register int maximo, i;

	FD_ZERO(conjunto);

	for(i = 0; i < num; i++)
	{
		FD_CLR(sockClients[i], conjunto);
	}

	maximo = -1;
	for(i = 0; i < num; i++)
	{
		if(sockClients[i] > maximo)
		{
			maximo = sockClients[i];
		}
		FD_SET(sockClients[i], conjunto);
	}

	return maximo;
}
