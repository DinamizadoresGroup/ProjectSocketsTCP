#include <sys/types.h> //para los protocolos
#include <sys/socket.h> //libreria sockets
#include <netinet/in.h> //para el sockaddr
#include <netinet/ip.h> //para el sockaddr_storage
#include <unistd.h> // para el close, y el sleep
#include <arpa/inet.h> // para el inet_aton
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h> //boolean
#include <string.h>
#include <netdb.h> //addrinfo
#include <sys/time.h> //struct timeval
#include <time.h> // struct timeval
#include <sys/ioctl.h>
#include <sys/wait.h>
//#define FD_SETSIZE 65536

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
int crearSocket(int tipo, unsigned short puerto, int maxcon);
int AcceptTCPConnection(int servSock, int num_conex, int *sockClients);
int inicializaConjunto(fd_set *conjunto, int *sockClients, int num);
void construirStart( int numConexiones, int vecinos, int ciclos, struct trama *start );
int enviarTrama(int *sockClients, int indice, struct trama mensaje, int num);
int leerBuffer(int *sockClients, int num, struct trama *mensaje);
int getGrupo(int i, int v);
int limiteSuperior(int grupo, int v);
int limiteInferior(int grupo, int v);
int EnterPulsado(void);
void simulacion(int numcli, int *sockClients, int numexe, int numvec);


/**
*	CÓDIGO PRINCIPAL DEL SERVIDOR
*	Argumentos:	argv[1]	--> IP
*				argv[2] --> Puerto de socket
*				argv[3]	-->	Número de clientes a conectarse
*				argv[4]	-->	Número de vecinos por grupo
*				argv[5]	-->	Número de ciclos de ejecución
*/
int main(int argc, char *argv[]) 
{
	int puerto = atoi(argv[2]);		              //El puerto por donde queremos abrir la conexion
	int numcli = atoi(argv[3]);		              //El numero maximo de clientes que permitirá el servidor
	int numvec = atoi(argv[4]);		              //El numero maximo de vecinos por grupo que permitirá el servidor
	int numexe = atoi(argv[5]);		              //El número máximo de ciclos que harán los clientes
	int descriptor_socket;			              //El descriptor del socket por el cual abrimos conexion
	int sockClients[numcli];				      //El vector de descriptores de sockets de clientes
    
	/**
	*	FLUJO: Verificamos que el número de clientes máximos aceptados son múltiplo del número de vecinos por grupo
	*/
	if((numcli % numvec) != 0)
	{
		printf("El número de clientes no es múltiplo del número de vecinos por grupo. La simulación no se realizará. \n");
		return -1;
	}

	/**
	*	FLUJO: Creamos el socket y lo enlazamos
	*/
	descriptor_socket = crearSocket(0, puerto, numcli);

	/**
	*	FLUJO: Hacemos una espera activa hasta que tenemos todos nuestros clientes conectados
	*/
    if(AcceptTCPConnection(descriptor_socket, numcli, sockClients) < 0)
    {
        return -1;
    }


    int i;
    int j;
    int k;
    int grupo;
    int limInf;
    int limSup;
    int sockClientsons[numvec];
    pid_t child_pid, wpid;
    int status = 0;
    printf("parent_pid = %d\n", getpid());
    for ( i = 0 ; i < numcli/numvec ; i++ )
    {
        printf("grupo %d\n", i);
        grupo = i + 1;
        limInf = ((grupo -1) * numvec);
        limSup = ((grupo * numvec) - 1);
        k = 0;
        if ((child_pid = fork()) == 0 )
        {
            printf("child_pid = %d\n", getpid());
            for(j = limInf ; j <= limSup ; j++)
            {
                printf("cliente: %d\n", j);
                sockClientsons[k] = sockClients[j];
                k++;
            }
            simulacion(numvec, sockClientsons, numexe, numvec);
            //liberar memoria
            exit(0);
        }
    }
    

    while ((wpid = wait(&status)) > 0)
    {
        printf("Exit status of %d was %d (%s)\n", (int)wpid, status,
               (status > 0) ? "accept" : "reject");
    }
    return 0;



    printf("Clientes conectados :)\n");

    

	return 0;
}

void simulacion(int numcli, int *sockClients, int numexe, int numvec)
{
    double timeClients[numcli];                 //El vector de tiempos medios de ciclo de cada cliente
    struct trama mensaje;                         //La variable tipo trama que usaremos para el paso de mensajes
    int i, j;                                         //Variable auxilar para uso en los for
    int opcion;
    int pid = (int)getpid();

    char commandauxiliarCPU[] = " -b -n 1 | tail -n 2"; //| tail -n 2 | cut -d ' ' -f 22"
    char commandauxiliarMEM[] = " -b -n 1| tail -n 2";
    char pidString[10];
    snprintf(pidString, sizeof(pidString), "%d", pid);
    printf("Cadena: %s\n", pidString);
    char commandauxCPU[51] = "top -p ";  
    strncat(commandauxCPU, pidString, strlen(pidString));
    char commandauxMEM[51] = "top -p ";
    strncat(commandauxMEM, pidString, strlen(pidString));
    strncat(commandauxCPU, commandauxiliarCPU, strlen(commandauxiliarCPU));
    strncat(commandauxMEM, commandauxiliarMEM, strlen(commandauxiliarMEM));
    FILE* fileCPU = popen(commandauxCPU, "r");
    FILE* fileMEM = popen(commandauxMEM, "r");
    FILE* fileDatos;
    char bufferCPU[100];
    char bufferMEM[100];
    fgets(bufferCPU, 100, fileCPU);
    fgets(bufferMEM, 100, fileMEM);
    printf("\ncpu = %s", bufferCPU);
    printf("\nmem = %s", bufferMEM);
    fflush(fileCPU);
    fflush(fileMEM);
    /**
    *   FLUJO: Generamos una trama para inicializar los clientes del tipo 'START:N:V:C'
    */
    construirStart(numcli, numvec, numexe, &mensaje);
    
    printf("Mensaje Start construido\n");

    /**
    *   FLUJO: Enviamos la señal de 'START' a todos los clientes conectados
    */
    for(i = 0; i < numcli; i++)
    {
        enviarTrama(sockClients, i, mensaje, numcli);
    }



    /**
    *   FLUJO: Comprobamos que nos llegan mensajes de todos los clientes
    */
    int contFin = 0;
    bool esperar = false;
    while( !esperar )
    {
        int rc, auxread = 0;
        fd_set conjuntodescriptores;                  //Conjunto de descriptores de socket
        int maxconjunto;                              //Socket maximo del conjunto de descriptores
  
        maxconjunto = inicializaConjunto(&conjuntodescriptores, sockClients, numcli);

        rc = select(maxconjunto + 1, &conjuntodescriptores, NULL, NULL, NULL);  //El ultimo campo es NULL para que espere hasta que sea pueda leer de algún socket

        if(rc < 0)
        {
            printf("Error en el select a la hora de leer\n");
            //return -1;
        }
        else if(rc == 0)
        {
            printf("Tiempo excedido, no hay sockets que leer\n");
            //return -1;
        }
        else if(rc > 0)
        {
            struct trama *vectorTramas = (struct trama*) calloc(rc, sizeof(struct trama));
            int it = 0;
            for(i = 0; i < numcli; i++)
            {
                if(FD_ISSET(sockClients[i], &conjuntodescriptores))
                {
                    auxread = read(sockClients[i], &mensaje, sizeof(struct trama));
                    if(auxread < 0)
                    {   
                        printf("Fallo de lectura\n");
                        //return -1;
                    }
                    else if(auxread == 0)
                    {
                        //printf("No se ha producido lectura de datos en el buffer\n");
                        //return 0;
                    }
                    else
                    {
                        printf("Bytes leidos: %d\n", auxread);
                        mensaje.origen = (unsigned short) i;
                        printf("\nTrama recibida, tipo: %d, n: %d, v: %d, c: %d, origen: %d\n", mensaje.tipo, mensaje.x, mensaje.y, mensaje.z, mensaje.origen);
                        vectorTramas[it] = mensaje;
                        it++;
                    }
                }
            }
            for(i = 0; i < it; i++)
            {
                opcion = vectorTramas[i].tipo;
                switch(opcion)
                {
                    //Para cuando sea un punto lo que recibimos
                    case 1:
                        printf("Reenvimamos el punto recibido\n");
                        int grupo = ((vectorTramas[i].origen/numvec)+1);
                        int limInf = ((grupo - 1) * numvec);
                        int limSup = ((grupo * numvec) - 1);
                        /*printf( "\ngrupo = %d\n", grupo  );
                        printf( "\nlimInf = %d\n", limInf  );
                        printf( "\nlimSup = %d\n", limSup  );*/
                        for(j = limInf; j <= limSup; j++)
                        {
                            if( j != vectorTramas[i].origen )
                            enviarTrama(sockClients, j, vectorTramas[i], numcli);
                            //printf( "\nj = %d\n", j  );
                        }

                        break;

                    //Para cuando sea un ACK (reenviamos al origen)
                    case 2:
                        printf("Redirigimos ACK al cliente que toca.\n");
                        enviarTrama(sockClients, vectorTramas[i].origen, vectorTramas[i], numcli);
                        break;

                    //Para cuando sea una señal FIN
                    case 3:
                        printf("Llegó señal FIN, confirmamos\n");
                        vectorTramas[i].x = 65535;
                        vectorTramas[i].y = 65535;
                        vectorTramas[i].z = 65535;
                        timeClients[vectorTramas[i].origen] = vectorTramas[i].tiempo;
                        enviarTrama(sockClients, vectorTramas[i].origen, vectorTramas[i], numcli);
                        contFin++;
                        break;
                }
                
            }
            free(vectorTramas);
            if( contFin == numcli )
                esperar = !esperar;
        }       

    }
    char filePid[5] = ".txt";
    strncat(pidString, filePid, strlen(filePid));

    if( (fileDatos = fopen(pidString, "w+")) == NULL)
    {
        printf("\nError en apertura de fichero");
    } 
    double mostrar = 0.0;
    fputs("Tiempo medio d respuetsa del sistema\n", fileDatos);
    for(i = 0; i < numcli; i++)
    {
        printf("La posicion %d de timeClients vale: %f segundos\n", i, timeClients[i]);
        mostrar += timeClients[i];
    }
    mostrar /= numcli;
    printf("El tiempo de respuesta medio del sistema es de %f segundos\n", mostrar);
    char mostrarStr[10];
    snprintf(mostrarStr, sizeof(mostrarStr), "%f", mostrarStr);
    fputs(mostrarStr, fileDatos);
    //fwrite(&mostrar, sizeof(double), sizeof(mostrar), fileDatos);
    //fputs(mostrar, fileDatos);
    fputs("\n", fileDatos);

    int grupo = (numcli/numvec);
    int limInf;
    int limSup;
    int k;
    double aux1 = 0.0,aux2=0.0;
    double vecgroup[grupo];
    for( i = 1; i <= grupo ; i++ )
    {
        limInf = ((i - 1) * numvec);
        limSup = ((i * numvec) - 1);
        aux1 = 0;
        for( k = limInf ; k <= limSup ; k++ )
        {
            aux1 += timeClients[k];
        }
        vecgroup[i-1] = aux1 / numvec;

    }
    fputs("MEDIA DE GRUPOS\n", fileDatos);

    char vecGroupStr[10];
    //snprintf(vecGroupStr, sizeof(vecGroupStr), "%f", vecgroup[i]);
    //fputs(vecGroupStr, fileDatos);

    for( i = 0 ; i < grupo ; i++ )
    {
        printf("La media del grupo %d es: %f segundos\n", i, vecgroup[i]);
        //fflush(stdout);
        snprintf(vecGroupStr, sizeof(vecGroupStr), "%f", vecgroup[i]);
        fputs(vecGroupStr, fileDatos);
        //fflush(stdout);
        //fflush(fileDatos);
        fputs("\n", fileDatos);

    }
 
    fclose(fileDatos);
    printf( "\nNos dormimos un ratito" );

    printf("\nDesconectando clientes...\n");
    printf("Comando MEM -->%s\n", commandauxMEM);
    printf("Comando CPU -->%s\n", commandauxCPU);
}

/**
*	Funciones auxiliares
*/

/**
*	Nombre: crearSocket
*	Uso: Crea un socket
*
*	Argumentos: int tipo --> Si 0 el tipo será TCP, si !=0 será UDP
*				unsigned short puerto 	--> El puerto a través del cual queremos hacer un sockets
*               int maxcon  --> Maxima cantidad de conexiones que escuchará el listen
*	Return: El descriptor del socket creado, -1 si ocurre error.
*/
int crearSocket(int tipo, unsigned short puerto, int maxcon)
{
	int socket_cliente = -1;
	struct sockaddr_in server;
	int modo = 0;

	if(tipo == 0)
	{
		socket_cliente = socket(PF_INET, SOCK_STREAM, 0);
		
		if(socket_cliente < 0)
		{
			printf("Error al crear el socket: TCP Version\n");
			return -1;
		}

		/**
		*	PASAMOS LOS DATOS DE CONEXIÓN
		*/
		server.sin_family = AF_INET;         
   		server.sin_port = htons(puerto); 
   		server.sin_addr.s_addr = INADDR_ANY; 

   		/**
   		*	ASOCIAMOS EL SOCKET AL PUERTO
   		*/
   		if(bind(socket_cliente,(struct sockaddr*)&server,sizeof(struct sockaddr))==-1) 
   		{
	      	printf("error en bind() \n");
      		exit(-1);
   		}

   		/**
   		*	Ponemos el socket en modo bloqueante 
   		*/
   		if (ioctl(socket_cliente,FIONBIO,&modo)!=0) 
   		{
      		printf("error en ioctl()\n");
		    exit(-1);
   		}
  
  		/**
  		*	Ponemos el tamaño de la sockClients de conexion
  		*/
   		if (listen(socket_cliente, maxcon) != 0) 
   		{
      		printf("error en listen()\n");
      		exit(-1);
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
*	Nombre:	aceptarConexion
*	Uso:	Acepta conexiones pendientes de sockets.
*   Argumentos:		int socket_cliente -->	Socket pendiente de aceptación
*   				int *sockClients -->	sockClients de sockets de aceptacion.
*					int indice 	--> La posición en que se ha conectado el cliente
*
*   Return:	0 si se ha establecido una conexión correctamente, -1 si no ha ocurrido
*/
int AcceptTCPConnection( int servSock, int num_conex,  int *sockClients )
{
    //int *sockClients = ( int* )calloc( num_conex, sizeof( int ) ); // Descriptores de sockets de clientes
    bool esperar =  true; // Centinela según el número de conexiones
    int contClnt = 0; // Contador de clientes
    int clntSock; //Socket del cliente
    struct sockaddr_storage clntAddr; // Dirección del cliente
    socklen_t clntAddrLen = sizeof( clntAddr ); // Tamaño de la dirección del cliente

    // Ciclo que termina cuando tenemos todos los clientes
    while ( esperar )
    {
        memset( &clntAddr, 0, clntAddrLen ); // Inicializamos a cero la estructura de datos del cliente 
        clntSock = -1; // Inicializando el socket 
    
        //aceptamos la conexión de un cliente y hacemos control de errores
        if ( ( clntSock = accept( servSock, ( struct sockaddr * ) &clntAddr, &clntAddrLen ) ) < 0 )
        {
            perror("accept() failed"); // recuerda colocar función de tratamiento de errores
            return -1;
        }
        else
        {
            fputs( "Manejando cliente", stdout ); // Imprime cosas hay que probarlo

            sockClients[ contClnt ] = clntSock; // Colocamos el socket del cliente en la estructura de datos
            contClnt++; // Aumentamos el contador de sockets de clientes
        }
    
        // Activamos centinela al tener todas las conexiones necesarias
        if ( contClnt == num_conex )
        {
            esperar = false;
        }

    }

    close(servSock); // Cerramos el socket de escucha

      return (0);
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

/**
*	Nombre: construirStart
*	Uso:	Montar una trama que de inicio a la simulacion
*	Argumentos:	int numConexiones 	--> El parametro de la cantidad de conexiones
*				int vecinos 	--> El número de vecinos que tiene cada grupo
*				int ciclos 		--> El numero de ciclos que tiene la simulacion
*				struct trama *start 	--> La trama por referencia que montamos como señal de inicio
*/
void construirStart( int numConexiones, int vecinos, int ciclos, struct trama *start )
{
    unsigned short n = (unsigned short) numConexiones;
    unsigned short v = (unsigned short) vecinos;
    unsigned short c = (unsigned short) ciclos;

    start->tipo = 0;
    start->x = n;
    start->y = v;
    start->z = c;
    start->origen = 65535;
    
    printf("\nTrama start generada, tipo: %d, n: %d, v: %d, c: %d, origen: %d\n", start->tipo, start->x, start->y, start->z, start->origen);

    return;
}

/**
*	Nombre: enviarTrama
*	Uso:	Escribe los datos leidos en el socket indicado
*    Argumentos:	int *sockClients 	-->		sockClients de sockets de aceptacion.
*   				int indice 	-->		Conexión a la que queremos enviar datos
*   				char buffer -->		Mensaje a enviar.
*   				int num 	-->		Numero de clientes.
*
*  Return:	Devolvemos 0 si todo fue bien, -1 si ocurrió algún error
*/
int enviarTrama(int *sockClients, int indice, struct trama mensaje, int num)
{      
    int rc;
    struct timeval t;
    fd_set conjuntodescriptores;                  //Conjunto de descriptores de socket
    int maxconjunto;                              //Socket maximo del conjunto de descriptores
  
    t.tv_sec=0;
	t.tv_usec=1000; 	/* 1 milisegundo */
    maxconjunto = inicializaConjunto(&conjuntodescriptores, sockClients, num);
      
    rc = select(maxconjunto + 1, NULL, &conjuntodescriptores, NULL, &t);

    if(rc < 0)
    {
    	printf("Error en el select a la hora de escribir\n");
    	return -1;
    }
    else if(rc == 0)
    {
    	printf("Tiempo de espera excedido, no hay sockets para escribir\n");
    }
    else if(rc > 0)
    {
    	if(FD_ISSET(sockClients[indice], &conjuntodescriptores))
		  {
        	if (write(sockClients[indice], &mensaje, sizeof(struct trama)) == -1)
        	{
           		printf("Error en la escritura");        
        	}
        	printf("Enviamos a %d un punto\n", indice);    
		  }
    }

	 return 0;
}

/**
*	Nombre: leerBuffer
*	Uso:	Lee de los sockets que tienen datos.
*	Argumentos:	int *sockClients -->	sockClients de sockets de aceptacion.
*				int num -->	Numero de conexiones que tenemos.
*				char buffer -->	String donde volcamos el mensaje recibido.
*				int *indice -->	Indice del cliente que leemos
*       fd_set *conjunto -->  El conjunto de descriptores de sockets
*       int maximo  --> El maximo descriptor del socket del conjunto
*
*	Return:	Devolvemos 1 si se leyó bien el buffer, -1 si ocurrió algún tipo de error y 0 si no leyó nada del buffer
*/
int leerBuffer(int *sockClients, int num, struct trama *mensaje)
{ 
    //Contador de numero de cadenas.
    unsigned i = 0;
    int rc, auxread = 0;
    fd_set conjuntodescriptores;                  //Conjunto de descriptores de socket
    int maxconjunto;                              //Socket maximo del conjunto de descriptores
  

    maxconjunto = inicializaConjunto(&conjuntodescriptores, sockClients, num);

    rc = select(maxconjunto + 1, &conjuntodescriptores, NULL, NULL, NULL);	//El ultimo campo es NULL para que espere hasta que sea pueda leer de algún socket

    if(rc < 0)
    {
    	printf("Error en el select a la hora de leer\n");
    	return -1;
    }
    else if(rc == 0)
    {
    	printf("Tiempo excedido, no hay sockets que leer\n");
    	return -1;
    }
    else if(rc > 0)
    {
    	while((i < num))
    	{
    		if(FD_ISSET(sockClients[i], &conjuntodescriptores))
    		{
    			auxread = read(sockClients[i], mensaje, sizeof(struct trama));
    			if(auxread < 0)
    			{
    				printf("Fallo de lectura\n");
    				return -1;
    			}
          		else if(auxread == 0)
	            {
            		printf("No se ha producido lectura de datos en el buffer\n");
            		return 0;
          		}
          		else
          		{
          			printf("Bytes leidos: %d\n", auxread);
    				mensaje->origen = (unsigned short) i;
                    printf("\nMensaje recibido, tipo: %d, n: %d, v: %d, c: %d, origen: %d\n", mensaje->tipo, mensaje->x, mensaje->y, mensaje->z, mensaje->origen);
          		}
    		}

    		i++;
    	}
    }

	 return 1;
}

/**
*   Nombre: getGrupo
*   Uso: En base a la posición de un socket en un vector determinamos a que grupo pertenece
*   Argumentos:     int i   --> La posición del descriptor del socket en el vector
*                   int v   --> La cantidad de clientes por grupo
*   Return: El grupo al cual pertenece el descriptor del socket introducido
*/
int getGrupo(int i, int v)
{
    return ((i/v)+1);
}

/**
*   Nombre: limiteSuperior
*   Uso: Nos devuelve la posición del vecino de grupo más alta en el vector
*   Argumentos: int grupo   --> El grupo del cual queremos averiguar el vecino más alto
*               int v       --> La cantidad de clientes por grupo
*   Return: La posición más alta de un cliente de un grupo en el vector de descriptores
*/
int limiteSuperior(int grupo, int v)
{
    return ((grupo * v) - 1);
}

/**
*   Nombre: limiteInferior
*   Uso: Nos devuelve la posición del vecino de grupo más pequeña en el vector
*   Argumentos: int grupo   --> El grupo del cual queremos averiguar el vecino más alto
*               int v       --> La cantidad de clientes por grupo
*   Return: La posición más pequeña de un cliente de un grupo en el vector de descriptores
*/
int limiteInferior(int grupo, int v)
{
    return ((grupo - 1) * v);
}

/*****************************************
-- Espera hasta que se pulsa la tecla Enter, 
   una vez se pulse se termina la ejecución del servidor.
*******************************************/
int EnterPulsado(void)
{
  fd_set conjunto;
  struct timeval t;
  
  FD_ZERO(&conjunto);
  FD_SET(0,&conjunto); /* 0 es el identificador del socket de stdin */
  
  t.tv_sec=0;
  t.tv_usec=1000; /* 1 milisegundo */
  
  return select(1,&conjunto,NULL,NULL,&t); /* En caso de error devuelve -1 y
                                              saldremos del programa. */
} 
