
//tabla de procesos lanzados en background, la shell necesita recordar los hijos que lanzo con & para listarlos, avisar cuando terminan y monitorearlos con pmon
#ifndef JOBS_H
#define JOBS_H

#include <sys/types.h>
#include <signal.h>
#include "mishell.h"

//estados posibles de un job
typedef enum {
    JOB_FREE = 0,   // ranura vacía 
    JOB_RUNNING,    // corriendo en background 
    JOB_STOPPED,    // pausado
    JOB_DONE,        // terminó
} job_state_t;

typedef struct {
    int         id;                  //numero que ve el usuario
    pid_t       pid;

    //lo escribe el manejador de SIGCHLD y lo lee el bucle principal. 
    volatile sig_atomic_t state;

    int         status;              // valor que devolvió waitpid
    char        cmdline[MAX_LINE];   // texto original 
} job_t;

//deja la tabla vacia
void jobs_init(void);

//registra un nuevo job en background y retorna el id asignado
int jobs_add(pid_t pid, const char *cmdline);

// imprime los jobs activos
void jobs_list(void);

//marca un job como terminado
void jobs_mark_done(pid_t pid, int status);

void jobs_report_finished(void);

job_t *jobs_get(int index);

#endif 