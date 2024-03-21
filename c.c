#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <semaphore.h>

#define NPROCS 4
#define SERIES_MEMBER_COUNT 200000

double *sums;
double x = 1.0;

int *proc_count;
int *start_all;
double *res;

sem_t sem_proc_count;
sem_t sem_start_all;

double get_member(int n, double x) {
    int i;
    double numerator = 1;

    for(i = 0; i < n; i++)
        numerator = numerator * x;

    if (n % 2 == 0)
        return (-numerator / n);
    else
        return numerator / n;
}

void proc(int proc_num) {
    int i;
    while (!(*start_all));
    sums[proc_num] = 0;
    for (i = proc_num; i < SERIES_MEMBER_COUNT; i += NPROCS)
        sums[proc_num] += get_member(i + 1, x);

    sem_wait(&sem_proc_count);
    (*proc_count)++;
    sem_post(&sem_proc_count);

    exit(0);
}

void master_proc() {
    int i;

    sleep(1);
    *start_all = 1;
    sem_post(&sem_start_all);

    for (i = 0; i < NPROCS; i++) {
        sem_wait(&sem_proc_count);
    }

    *res = 0;

    for (i = 0; i < NPROCS; i++)
        *res += sums[i];

    exit(0);
}

int main() {
    int *threadIdPtr;

    long long start_ts;
    long long stop_ts;
    long long elapsed_time;
    long lElapsedTime;
    struct timeval ts;
    int i;
    int p;
    int shmid;
    void *shmstart;

    // Inicialización de semáforos
    sem_init(&sem_proc_count, 1, 1);
    sem_init(&sem_start_all, 1, 0);

    shmid = shmget(0x1234, NPROCS * sizeof(double) + 2 * sizeof(int), 0666 | IPC_CREAT);
    shmstart = shmat(shmid, NULL, 0);
    sums = (double*)shmstart;
    proc_count = (int*)(shmstart + NPROCS * sizeof(double));
    start_all = (int*)(shmstart + NPROCS * sizeof(double) + sizeof(int));
    res = (double*)(shmstart + NPROCS * sizeof(double) + 2 * sizeof(int));

    *proc_count = 0;
    *start_all = 0;

    // Medición del tiempo de inicio
    gettimeofday(&ts, NULL);
    start_ts = ts.tv_sec;

    // Creación de los procesos
    for (i = 0; i < NPROCS; i++) {
        p = fork();
        if (p == 0) {
            proc(i);
            break;
        }
    }

    // Creación del proceso maestro
    if (i == NPROCS) {
        master_proc();
    }

    // Impresión de información
    printf("El recuento de ln(1 + x) miembros de la serie de Mercator es %d\n", SERIES_MEMBER_COUNT);
    printf("El valor del argumento x es %f\n", (double)x);

    // Espera a que todos los procesos terminen
    for (int i = 0; i < NPROCS + 1; i++)
        wait(NULL);

    // Medición del tiempo final
    gettimeofday(&ts, NULL);
    stop_ts = ts.tv_sec;

    // Cálculo del tiempo de ejecución
    elapsed_time = stop_ts - start_ts;

    // Impresión del resultado y tiempo de ejecución
    printf("Tiempo = %lld segundos\n", elapsed_time);
    printf("El resultado es %10.8f\n", *res);
    printf("Llamando a la función ln(1 + %f) = %10.8f\n", x, log(1 + x));

    // Liberación de recursos
    shmdt(shmstart);
    shmctl(shmid, IPC_RMID, NULL);
    sem_destroy(&sem_proc_count);
    sem_destroy(&sem_start_all);

    return 0;
}
