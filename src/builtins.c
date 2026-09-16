/*
 * comandos internos: cd, exit, jobs, pmon
 * son internos porque, si fueran externos, fork crea un proceso hijo con su copia del estado incluyendo el directorio.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include "builtins.h"
#include "jobs.h"
#include "pmon.h"

static int builtin_cd(command_t *cmd);
static int builtin_exit(command_t *cmd);
static int builtin_jobs(command_t *cmd);
static int builtin_pmon(command_t *cmd);
static int builtin_bg(command_t *cmd);
static int builtin_fg(command_t *cmd);
/*
 * tabla: nombre -> función. 
 * agregar un built-in nuevo es agregar una línea aqui y escribir la función.
 */
static const struct {
    const char *name;
    int (*fn)(command_t *cmd);
} BUILTINS[] = {
    { "cd",   builtin_cd   },
    { "exit", builtin_exit },
    { "jobs", builtin_jobs },
    { "pmon", builtin_pmon },
    { "bg",   builtin_bg   },
    { "fg",   builtin_fg},
    { NULL,   NULL         }
};

int is_builtin(const char *name)
{
    int i;

    if (name == NULL)
        return 0;

    for (i = 0; BUILTINS[i].name != NULL; i++)
        if (strcmp(name, BUILTINS[i].name) == 0)
            return 1;

    return 0;
}

int run_builtin(command_t *cmd)
{
    int i;

    for (i = 0; BUILTINS[i].name != NULL; i++)
        if (strcmp(cmd->argv[0], BUILTINS[i].name) == 0)
            return BUILTINS[i].fn(cmd);

    return 1;
}
//cd
static int builtin_cd(command_t *cmd)
{
    const char *dir;

    if (cmd->argc > 2) {
        fprintf(stderr, "cd: demasiados argumentos\n");
        return 1;
    }

    //sin argumento va a $home
    dir = (cmd->argc == 2) ? cmd->argv[1] : getenv("HOME");

    if (dir == NULL) {
        fprintf(stderr, "cd: la variable HOME no está definida\n");
        return 1;
    }

    if (chdir(dir) == -1) {
        perror("cd");
        return 1;
    }

    return 0;
}
// exit
static int builtin_exit(command_t *cmd)
{
    int code = 0;

    if (cmd->argc >= 2) {

        char *fin;
        long  n = strtol(cmd->argv[1], &fin, 10);

        //fin queda en el primer caracter que no se pudo convertir, si no es el \0
        //final es que el argumento no era solo un numero
        if (fin == cmd->argv[1] || *fin != '\0') {
            fprintf(stderr, "exit: %s: se necesita un argumento numerico\n", cmd->argv[1]);
            exit(2);
        }

        code = (int)n;
    }

    // TODO R5: antes de salir, avisar si quedan jobs en background o mandarles SIGHUP. 

    exit(code);   
}

//jobs

static int builtin_jobs(command_t *cmd)
{
    (void)cmd;   //jobs no usa argumentos 
    jobs_list();
    return 0;
}
// pmon
static int builtin_pmon(command_t *cmd)
{
    int seconds = PMON_DEFAULT_SECS;

    if (cmd->argc >= 2) {
        seconds = atoi(cmd->argv[1]);
        if (seconds <= 0) {
            fprintf(stderr, "pmon: el intervalo debe ser un entero positivo\n");
            return 1;
        }
    }

    return pmon_run(seconds);
}

// bg
static int builtin_bg(command_t *cmd)
{
    if(cmd->argc != 2){
        fprintf(stderr, "bg: solo debes ingresar el indice del job como argumento\n");
        return 1;
    }
    int id = atoi(cmd->argv[1]);
    job_t* job = jobs_get(id - 1); // id - 1 = indice

    if(job == NULL || job->state == JOB_FREE){
        fprintf(stderr, "bg: el job seleccionado no existe\n");
        return 1;
    }

    if(job->state == JOB_DONE){
       fprintf(stderr, "bg: el job seleccionado ya está muerto\n");
       return 1;  
    }
    if(job->state == JOB_RUNNING){
       fprintf(stderr, "bg: el job seleccionado ya está corriendo en segundo plano\n");
       return 1;  
    }
    // imprimimos el comando que se va a despertar
    printf("[%d] %s &\n", id, job->cmdline);
    // actualizamos el estado
    job->state = JOB_RUNNING;
    // enviamos la señal a todo el grupo de continuar
    if(kill(-job->pid, SIGCONT)<0){ 
        perror("mishell: kill");
        return 1;
    }
    // exito
    return 0;

}

// fg
static int builtin_fg(command_t *cmd)
{
    if(cmd->argc != 2){
        fprintf(stderr, "fg: solo debes ingresar el indice del job como argumento\n");
        return 1;
    }
    int id = atoi(cmd->argv[1]);
    job_t* job = jobs_get(id - 1); // id - 1 = indice

    if(job == NULL || job->state == JOB_FREE    ){
        fprintf(stderr, "fg: el job seleccionado no existe\n");
        return 1;
    }

    // le cedemos la terminal al grupo
    tcsetpgrp(STDIN_FILENO, job->pid);
    // si estaba pausado lo despertamos
    if(job->state == JOB_STOPPED){
        if(kill(-job->pid, SIGCONT) < 0){
            perror("mishell: kill (fg)");
        }
        // actualizamos el estado
        job->state = JOB_RUNNING;   
    }
    // imprimimos el comando que se va a traer
    printf("%s\n", job->cmdline);
    int status;
    // realizamos constantemente el waitpid por si el kernel manda un SIGCHLD al
    // cambiar un proceso hijo de estado
    // incluso si el proceso aun no termina
    while (1) {
        int ret = waitpid(-job->pid, &status, WUNTRACED);
        
        //si waitpid falla (devuelve -1)
        if (ret == -1) {
            if (errno == EINTR) {
                // El kernel interrumpe con una señal, se ignora.
                continue;
            } 
            else if (errno == ECHILD) {
                // nuestro manejador ya actualiza el estado del job, solo salimos.
                break;
            } 
            else {
                // error
                perror("mishell: waitpid (fg)");
                break;
            }
        } 
        
        //si waitpid tuvo exito, vemos si se trata de una pausa o si murio el proceso.
        if (WIFSTOPPED(status)) {
            job->state = JOB_STOPPED;
            printf("\n[%d]+  Detenido\t%s\n", job->id, job->cmdline);
        } else {
            job->state = JOB_DONE; // Murió de forma natural
        }
        
        break; // Salimos del bucle infinito
    }
    // le devolvemos la terminal a la shell
    if (tcsetpgrp(STDIN_FILENO, getpgrp()) == -1) {
        perror("mishell: no se pudo recuperar la terminal en fg");
    }
    // exito
    return 0;

}
