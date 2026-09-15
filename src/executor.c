// fork + execvp + waitpid y el lugar donde iran los pipes
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include "executor.h"
#include "builtins.h"
#include "signals.h"
#include "jobs.h"

//traduce el estado de waitpid a un codigo de salida legible, si el proceso murio por una señal el codigo es 128 + numero de señal
static int status_to_code(int status)
{
    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    if (WIFSIGNALED(status))
        return 128 + WTERMSIG(status);
    return 0;
}

//conecta stdin y stdout del hijo a los archivos que pidio el usuario
static int apply_redirections(command_t *cmd)
{
    int fd;

    if (cmd->infile != NULL) {

        fd = open(cmd->infile, O_RDONLY);
        if (fd == -1) {
            fprintf(stderr, "mishell: %s: %s\n", cmd->infile, strerror(errno));
            return -1;
        }

        //el descriptor 0 pasa a apuntar al archivo
        if (dup2(fd, STDIN_FILENO) == -1) {
            perror("mishell: dup2");
            return -1;
        }

        close(fd);
    }

    if (cmd->outfile != NULL) {

        //O_TRUNC vacia el archivo y O_APPEND escribe al final
        int flags = O_WRONLY | O_CREAT | (cmd->append ? O_APPEND : O_TRUNC);

        //0644 son los permisos por si hay que crearlo, lectura y escritura al dueño y lectura al resto
        fd = open(cmd->outfile, flags, 0644);
        if (fd == -1) {
            fprintf(stderr, "mishell: %s: %s\n", cmd->outfile, strerror(errno));
            return -1;
        }

        if (dup2(fd, STDOUT_FILENO) == -1) {
            perror("mishell: dup2");
            return -1;
        }

        close(fd);
    }

    return 0;
}

//corre un built in en la shell, con su redireccion puesta y devolviendo los descriptores como estaban
static int run_builtin_here(command_t *cmd)
{
    int saved_in, saved_out, code;

    if (cmd->infile == NULL && cmd->outfile == NULL)
        return run_builtin(cmd);

    //dup copia un descriptor al primer numero libre
    saved_in  = dup(STDIN_FILENO);
    saved_out = dup(STDOUT_FILENO);

    if (saved_in == -1 || saved_out == -1) {
        perror("mishell: dup");
        return 1;
    }

    if (apply_redirections(cmd) == -1)
        code = 1;
    else
        code = run_builtin(cmd);

    //los built ins imprimen con printf
    fflush(stdout);

    dup2(saved_in,  STDIN_FILENO);
    dup2(saved_out, STDOUT_FILENO);
    close(saved_in);
    close(saved_out);

    return code;
}

int execute_pipeline(pipeline_t *pl)
{
    pid_t    pids[MAX_CMDS];
    pid_t    last;
    pid_t    pgid = 0;         // grupo de la tuberia, lo define el primer hijo
    sigset_t mask, prev;
    int      prev_read = -1;   // extremo de lectura del pipe del comando anterior
    int      i, status, code = 0;

    //un built in en primer plano se ejecuta en la shell, si estuviera dentro de una tuberia bash lo corre en un hijo
    if (pl->ncmds == 1 && !pl->background && is_builtin(pl->cmds[0].argv[0]))
        return run_builtin_here(&pl->cmds[0]);

    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &mask, &prev);

    for (i = 0; i < pl->ncmds; i++) {

        command_t *cmd = &pl->cmds[i];
        pid_t      pid;
        int        fd[2];

        //un pipe entre cada par de comandos, ncmds-1 en total
        if (i < pl->ncmds - 1 && pipe(fd) == -1) {
            perror("mishell: pipe");
            if (prev_read != -1)
                close(prev_read);
            sigprocmask(SIG_SETMASK, &prev, NULL);
            return -1;
        }

        //vaciamos antes del fork
        fflush(stdout);

        pid = fork();

        if (pid < 0) {
            perror("mishell: fork");
            sigprocmask(SIG_SETMASK, &prev, NULL);
            return EXEC_FATAL;
        }

        if (pid == 0) {


            //la mascara de señales tambien se hereda y sobrevive al exec
            sigprocmask(SIG_SETMASK, &prev, NULL);

            // asignamos un grupo nuevo a todos los procesos (foreground y background)
            if(pgid == 0)
                pgid = getpid();
            setpgid(0,pgid);    

            //si es foreground, le entrega la terminal
            if (!pl->background)
                tcsetpgrp(STDIN_FILENO, pgid);
            
            // ahora que tiene la terminal puede ser vulnerable a SIGTTOU    
            signals_reset_child();    

            //su entrada viene del pipe anterior
            if (prev_read != -1) {
                dup2(prev_read, STDIN_FILENO);
                close(prev_read);
            }

            //su salida va al pipe que acabamos de crear
            if (i < pl->ncmds - 1) {
                close(fd[0]);
                dup2(fd[1], STDOUT_FILENO);
                close(fd[1]);
            }

            //va despues de los pipes, asi una redireccion explicita le gana al pipe
            if (apply_redirections(cmd) == -1)
                _exit(1);

            //un built in dentro de una tuberia corre aqui
            if (is_builtin(cmd->argv[0])) {
                int c = run_builtin(cmd);
                fflush(stdout);   //_exit no vacia los buffers
                _exit(c);
            }

            execvp(cmd->argv[0], cmd->argv);

            // si execvp retorna es porque falló
            fprintf(stderr, "mishell: %s: %s\n", cmd->argv[0], strerror(errno));

            //_exit y no exit, el hijo heredó los buffers del padre y se imprimirían dos veces
            _exit(127);
        }

        //proceso padre

        pids[i] = pid;

        //el mismo setpgid lo hacen padre e hijo, gana el que llegue primero.
        if (pgid == 0)
            pgid = pid;
        setpgid(pid, pgid);


        //el padre no participa en la tuberia
        if (prev_read != -1)
            close(prev_read);

        if (i < pl->ncmds - 1) {
            close(fd[1]);
            prev_read = fd[0];   // se lo pasamos al hijo siguiente
        }
    }

    last = pids[pl->ncmds - 1];

    if (pl->background) {

        //registramos el job con el pgid debido al uso de pipes
        int id = jobs_add(pgid, pl->rawline);
        if (id > 0)
            printf("[%d] %d\n", id, (int)last);

        //ya esta anotado, ahora el manejador puede recogerlo sin perderselo
        sigprocmask(SIG_SETMASK, &prev, NULL);
        return 0;
    }
    // el padre cede la terminal al grupo
    if (tcsetpgrp(STDIN_FILENO, pgid) == -1) {
        perror("mishell: error al ceder la terminal");
    }

    for (i = 0; i < pl->ncmds; i++) {
        // usamos la flag WUNTRACED para que espere se llame a waitpid no solo cuando el hijo muera,
        // sino tambien cuando es pausado
        if (waitpid(pids[i], &status, WUNTRACED) == -1) {
            perror("mishell: waitpid");
            continue;
        }
        // dado que waitpid retorna si el proceso muere o es pausado, necesitamos revisar el status
        if (WIFSTOPPED(status)) {
            // se registra el job en la tabla como detenido usando su pgid
            int id = jobs_add(pgid, pl->rawline);
            if (id > 0){
                // actualizamos el estado del job
                job_t *job = jobs_get(id-1); //le restamos 1 al id para que sea indice de arreglo
                if (job != NULL) {
                    job->state = JOB_STOPPED; 
                }

                printf("\n[%d]+  Detenido\t%s\n", id, pl->rawline);
            }
            // Si se detiene uno, se detiene toda la pipe
            break;
        }


        if (pids[i] == last)
            code = status_to_code(status);
    }
    // le devolvemos el control de la terminal al padre
    if (tcsetpgrp(STDIN_FILENO, getpgrp()) == -1) {
        perror("mishell: error al recuperar la terminal");
    }
    
    sigprocmask(SIG_SETMASK, &prev, NULL);

    return code;
}
