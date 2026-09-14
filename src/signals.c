//manejo de señales
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include "signals.h"
#include "jobs.h"

static void install(int signum, void (*handler)(int), int flags)
{
    struct sigaction sa;
   
    memset(&sa, 0, sizeof(sa));

    sa.sa_handler = handler;

    sigemptyset(&sa.sa_mask);

    sa.sa_flags = flags;

    if (sigaction(signum, &sa, NULL) == -1)
        perror("mishell: sigaction");
}

//recoge a los hijos que ya murieron, el kernel manda SIGCHLD cada vez que uno termina
static void on_sigchld(int signum)
{
    int   saved_errno = errno;
    pid_t pid;
    int   status;

    (void)signum;

    //en ciclo porque las señales no se encolan
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
        jobs_mark_done(pid, status);

    errno = saved_errno;
}

void signals_setup_shell(void)
{
    //SA_RESTART reintenta la llamada interrumpida, sin él fgets falla con EINTR
    install(SIGINT,  SIG_IGN, SA_RESTART);
    install(SIGQUIT, SIG_IGN, SA_RESTART);
    install(SIGCHLD, on_sigchld, SA_RESTART | SA_NOCLDSTOP);

    // evita que la shell se pause a sí misma ante CTRL + Z
    install(SIGTSTP, SIG_IGN, SA_RESTART); 

    // evitan que la shell se congele sola cuando intente recuperar 
    // el control de la terminal con tcsetpgrp()
    install(SIGTTIN, SIG_IGN, SA_RESTART); 
    install(SIGTTOU, SIG_IGN, SA_RESTART);

}

void signals_reset_child(void)
{
    install(SIGINT,  SIG_DFL, 0);
    install(SIGQUIT, SIG_DFL, 0);
    install(SIGCHLD, SIG_DFL, 0);

    //hce que el proceso hijo sí responda al ctrl+z
    install(SIGTSTP, SIG_DFL, 0); 
    
    //hacen que el hijo atienda las señales de pausarse si intenta
    // leer o escribir en la terminal estando en background
    install(SIGTTIN, SIG_DFL, 0);
    install(SIGTTOU, SIG_DFL, 0);

}
