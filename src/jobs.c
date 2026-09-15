//tabla de jobs en background, se usa un arreglo estatico por seguridad 
#include <stdio.h>
#include <string.h>

#include "jobs.h"

static job_t jobs[MAX_JOBS];


void jobs_init(void)
{
    memset(jobs, 0, sizeof(jobs));
}

int jobs_add(pid_t pid, const char *cmdline)
{
    int i;

    for (i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].state != JOB_FREE)
            continue;

        jobs[i].id     = i+1;
        jobs[i].pid    = pid;
        jobs[i].state  = JOB_RUNNING;
        jobs[i].status = 0;

        strncpy(jobs[i].cmdline, cmdline, MAX_LINE - 1);
        jobs[i].cmdline[MAX_LINE - 1] = '\0';

        return jobs[i].id;
    }

    fprintf(stderr, "mishell: tabla de jobs llena (máximo %d)\n", MAX_JOBS);
    return -1;
}

void jobs_list(void)
{
    int i;

    for (i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].state == JOB_FREE)
            continue;

        const char *state_str;
        if (jobs[i].state == JOB_RUNNING) {
            state_str = "Ejecutando";
        } else if (jobs[i].state == JOB_STOPPED) { 
            state_str = "Detenido";
        } else {
            state_str = "Terminado";
        }

        printf("[%d] %d %-12s %s\n",
               jobs[i].id,
               (int)jobs[i].pid,
               state_str,
               jobs[i].cmdline);

        // si ya informamos al usuario que ya terminó, lo borramos
        if (jobs[i].state == JOB_DONE) {
            jobs[i].state = JOB_FREE;
        }       
    }
}

job_t *jobs_get(int index)
{
    if (index < 0 || index >= MAX_JOBS)
        return NULL;
    if (jobs[index].state == JOB_FREE)
        return NULL;
    return &jobs[index];
}

//corre dentro del manejador de SIGCHLD, asi que solo escribe en la tabla
void jobs_mark_done(pid_t pid, int status)
{
    int i;

    for (i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].state == JOB_RUNNING && jobs[i].pid == pid) {
            jobs[i].status = status;
            jobs[i].state  = JOB_DONE;
            return;
        }
    }

    //si el pid no esta en la tabla era un hijo de primer plano
}

//corre en el bucle principal
void jobs_report_finished(void)
{
    int i;

    for (i = 0; i < MAX_JOBS; i++) {

        if (jobs[i].state != JOB_DONE)
            continue;

        printf("[%d]+ Done   %s\n", jobs[i].id, jobs[i].cmdline);

        //liberamos la ranura para no avisar dos veces del mismo job
        jobs[i].state = JOB_FREE;
    }
}
