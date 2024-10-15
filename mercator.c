/*           
        Para compilar incluir la librería m (matemáticas)

        Ejemplo:
            gcc -o mercator mercator.c -lm -pthread
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/stat.h>

// Semaforos a utilizar
#define SEM_MASTER "/sem_master" // De master a slaves
#define SEM_SLAVE "/sem_slave" // Semaforo de slaves cuando terminan
#define MUTEX_SEM "/mutex_sem" // Importante: Empleo de exlusividad mutua

#define NPROCS 4
#define SERIES_MEMBER_COUNT 200000

typedef struct { 
    double sums[NPROCS];
    double x_val;
    double res;

    // Sin utilizar
    int proc_count;
    int start_all;
} SHARED;

SHARED *shared;

// Calcular cada termino n, entrega el numerador
double get_member(int n, double x)
{
    int i;
    double numerator = 1;

    for(i = 0; i < n; i++)
        numerator *= x;

    if (n % 2 == 0)
        return (-numerator / n);
    else
        return (numerator / n);
}

// Proceso esclavo
void proc(int proc_num)
{
    int i;

    // Abrir semaforos
    // Son locales, para referenciar cada una a su semaforo Global nombrado
    sem_t *sem_master = sem_open(SEM_MASTER, 0);
    sem_t *sem_slave = sem_open(SEM_SLAVE, 0);
    sem_t *mutex_sem = sem_open(MUTEX_SEM, 0);

    // Esperar a que inicie el master, reemplazando la linea
    // while(!(shared->start_all));
    sem_wait(sem_master); 

    // Seccion critica,
    double suma_proceso = 0.0;
    for(i = proc_num; i < SERIES_MEMBER_COUNT; i += NPROCS)
        suma_proceso += get_member(i + 1, shared->x_val);

    sem_wait(mutex_sem); // Semaforo EXCLUSIVIDAD 
    shared->sums[proc_num] = suma_proceso;

    sem_post(mutex_sem); // Compltar Exclusividad mutua
    sem_post(sem_slave); // SIGNAL de esclavos

    // Cerrar los semaforos
    sem_close(sem_master);
    sem_close(sem_slave);
    sem_close(mutex_sem);

    exit(0);
}

// Funcion maestra
void master_proc()
{
    int i;

    // Abrir los semaforos
    sem_t *sem_master = sem_open(SEM_MASTER, 0);
    sem_t *sem_slave = sem_open(SEM_SLAVE, 0);
    sem_t *mutex_sem = sem_open(MUTEX_SEM, 0);

    // Obtener el valor de x desde el archivo entrada.txt
    FILE *fp = fopen("entrada.txt", "r");
    if (fp == NULL) {
        exit(1);
    }

    fscanf(fp, "%lf", &shared->x_val);
    fclose(fp);

    // SIGNAL para que los esclavos inicien con los procesos
    // POR ESTA RAZON comienza en 0 el semaforo, incrementa por NPROCS
    for (i = 0; i < NPROCS; i++)
        sem_post(sem_master);

    // Espera al signal para que se completen los semaforos
    for (i = 0; i < NPROCS; i++)
        sem_wait(sem_slave);

    // Semaforo mutex para guardar el resultado
    sem_wait(mutex_sem);
    shared->res = 0;
    for (i = 0; i < NPROCS; i++)
        shared->res += shared->sums[i]; // Sumar resultados
    sem_post(mutex_sem); // Signal cuando termine de guardar en el semaforo binario

    // Cerrar los semaforos
    sem_close(sem_master);
    sem_close(sem_slave);
    sem_close(mutex_sem);

    exit(0);
}


sem_t *s_master;      
sem_t *s_slave;        
sem_t *s_exmut; 


int main()
{
    int *threadIdPtr;
    
    long long start_ts;
    long long stop_ts;
    long long elapsed_time;
    long lElapsedTime;
    struct timeval ts;
    int i;
    int p;
    int shmid;
    int status;

   // Solicita y conecta la memoria compartida
    shmid = shmget(0x1234,sizeof(SHARED),0666|IPC_CREAT);
    shared = shmat(shmid,NULL,0);
 
    // inicializa las variables en memoria compartida
    // shared->proc_count = 0;
    // shared->start_all = 0;
    for(i = 0; i < NPROCS; i++)
        shared->sums[i] = 0.0;


    // ------------------ Implementacion semaforos ----------------

    // Eliminar los semaforos si existen
    sem_unlink(SEM_MASTER);
    sem_unlink(SEM_SLAVE);
    sem_unlink(MUTEX_SEM);

    // Abrir los semaforos con los permisos
    s_master = sem_open(SEM_MASTER, O_CREAT | O_EXCL, S_IRUSR, 0);
    s_slave = sem_open(SEM_SLAVE, O_CREAT | O_EXCL, S_IRUSR, 0);
    s_exmut = sem_open(MUTEX_SEM, O_CREAT | O_EXCL, S_IRUSR, 1); // Mutex initialized to 1

    gettimeofday(&ts, NULL);
    start_ts = ts.tv_sec;

    // Procesos esclavos TODO: Implementar proc(i) dado los semaforos
    for(i = 0; i < NPROCS; i++)
    {
        p = fork();
        if(p == 0)
            proc(i);
    }

    p = fork();
    if(p == 0)
        master_proc();

    printf("El recuento de ln(1 + x) miembros de la serie de Mercator es %d\n", SERIES_MEMBER_COUNT);

    for(int i=0;i<NPROCS+1;i++)
    {
        wait(&status);
        if(status==0x100)   // Si el master_proc termina con error
        {
            fprintf(stderr,"Proceso no puede abrir el archivo de entrada\n");
            break;
        }
    }

    gettimeofday(&ts, NULL);
    stop_ts = ts.tv_sec;
    elapsed_time = stop_ts - start_ts;

    printf("Tiempo = %lld segundos\n", elapsed_time);
    printf("El resultado es %10.8f\n", shared->res);
    printf("Llamando a la función ln(1 + %f) = %10.8f\n",shared->x_val, log(1+shared->x_val));

    // Eliminar y desconectar semaforos
    sem_close(s_master);
    sem_close(s_slave);
    sem_close(s_exmut);
    sem_unlink(SEM_MASTER);
    sem_unlink(SEM_SLAVE);
    sem_unlink(MUTEX_SEM);

    // Desconecta y elimina la memoria compartida
    shmdt(shared);
    shmctl(shmid, IPC_RMID, NULL);

    return 0;
}
