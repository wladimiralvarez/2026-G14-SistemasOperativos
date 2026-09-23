// bucle principal: leer -> parsear -> ejecutar.
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mishell.h"
#include "parser.h"
#include "executor.h"
#include "signals.h"
#include "jobs.h"
#include "vars.h"
#include "replace.h"

// imprime el prompt
static void print_prompt(void)
{
    char cwd[MAX_PATH_LEN];

    if (getcwd(cwd, sizeof(cwd)) == NULL)
        strcpy(cwd, "?");

    printf("mishell:%s$ ", cwd);

    //el prompt no termina en \n, sin esto se queda en el buffer y no se ve
    fflush(stdout);
}

int main(void)
{
    char       line[MAX_LINE];
    pipeline_t pl;
    int        parsed;
    int        code = 0;

    //la shell ignora SIGINT/SIGQUIT desde el arranque
    signals_setup_shell();

    // tabla de background vacía
    jobs_init();

    for (;;) {

        // avisa de los jobs que terminaron, antes del prompt
        jobs_report_finished();

        print_prompt();

         //fgets() lee hasta \n o hasta llenar el buffer, deja el string terminado en \0, devuelve null en eof
        if (fgets(line, sizeof(line), stdin) == NULL) {
            printf("\n");   
            break;
        }

        if (check_set(line)){  //se comprueba si hay algun seteo de variables locales en la linea leida
            continue;
        }
        replace_variables(line, sizeof(line));  //se reemplazan las variables que tienen un $ al inicio por sus valores correspondientes a los mapeos disponibles

        parsed = parse_line(line, &pl);

        if (parsed <= 0)
            continue;       // 0 si línea vacía, -1 si el error ya fue reportado 

        //si fork falla salimos avisando del error
        if (execute_pipeline(&pl) == EXEC_FATAL) {
            code = 1;
            break;
        }
    }

    return code;
}
