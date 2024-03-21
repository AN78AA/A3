/*
    Para compilar incluir la librería m (matemáticas)

    Ejemplo:
    gcc -o mercator mercator.c -lm
*/

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
sem_t *start_semaphore; // Semáforo para sincronizar el inicio de los cálculos
double *res; // Resultado de la suma de las sumas parciales

// Función para calcular un miembro de la serie de Mercator
double get_member(int n, double x) {
    int i;
    double numerator = 1;

    for(i = 0; i < n; i++) {
        numerator = numerator * x;
    }
    if (n % 2 == 0)
        return (-numerator / n);
    else
        return numerator / n;
}

// Función ejecutada por los procesos esclavos
void proc(int proc_num) {
    int i;
    sem_wait(start_semaphore); // Espera hasta que el semáforo se active
    sums[proc_num] = 0;
    for (i = proc_num; i < SERIES_MEMBER_COUNT; i += NPROCS) {
        sums[proc_num] += get_member(i + 1, x);
    }

    sem_post(start_semaphore); // Incrementa el semáforo para indicar que ha terminado
    exit(0);
}

// Función ejecutada por el proceso maestro
void master_proc() {
    int i;

    for (i = 0; i < NPROCS; i++) {
        sem_wait(start_semaphore); // Espera a que todos los procesos esclavos estén listos
    }
    *res = 0;

    for (i = 0; i < NPROCS; i++) {
        *res += sums[i]; // Suma las sumas parciales de los procesos esclavos
    }
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

    // Crear un segmento de memoria compartida
    shmid = shmget(0x1234, NPROCS * sizeof(double) + 2 * sizeof(int), 0666 | IPC_CREAT);
    shmstart = shmat(shmid, NULL, 0);
    sums = shmstart;
    proc_count = shmstart + NPROCS * sizeof(double);
    start_semaphore = (sem_t*)(shmstart + NPROCS * sizeof(double) + sizeof(int));
    res = shmstart + NPROCS * sizeof(double) + 2 * sizeof(int);

    // Inicializar el semáforo con 0
    sem_init(start_semaphore, 1, 0);

    *proc_count = 0;

    gettimeofday(&ts, NULL);
    start_ts = ts.tv_sec; // Tiempo inicial

    // Crear procesos esclavos
    for (i = 0; i < NPROCS; i++) {
        p = fork();
        if (p == 0)
            proc(i);
    }

    // Crear proceso maestro
    p = fork();
    if (p == 0)
        master_proc();

    // Activar el semáforo para que los procesos esclavos comiencen
    for (i = 0; i < NPROCS; i++){
        sem_post(start_semaphore);
    }

    printf("El recuento de ln(1 + x) miembros de la serie de Mercator es %d\n", SERIES_MEMBER_COUNT);
    printf("El valor del argumento x es %f\n", (double)x);

    // Esperar a que todos los procesos terminen
    for (int i = 0; i < NPROCS + 1; i++) {
        wait(NULL);
    }

    gettimeofday(&ts, NULL);
    stop_ts = ts.tv_sec; // Tiempo final
    elapsed_time = stop_ts - start_ts;
    printf("Tiempo = %lld segundos\n", elapsed_time);
    printf("El resultado es %10.8f\n", *res);
    printf("Llamando a la función ln(1 + %f) = %10.8f\n", x, log(1 + x));

    sem_destroy(start_semaphore);     // Destruir el semáforo y liberar la memoria compartida
    shmdt(shmstart);
    shmctl(shmid, IPC_RMID, NULL);
}
