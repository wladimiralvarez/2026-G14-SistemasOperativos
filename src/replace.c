#include "replace.h"
#include "vars.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "mishell.h"

void replace_variables(char *line, size_t max_len){
    char buffer[MAX_LINE] = {0};  //puede perderse informacion en casos limite
    int i = 0;
    long unsigned int j = 0;

    while (line[i] != '\0' && j < sizeof(buffer) - 1) {  //lee y reescribe toda la cadena realizando reemplazos al detectar un $ 
        if (line[i] == '$') {
            i++;

            char var_name[MAX_VAR_NAME] = {0};
            int k = 0;

            while (line[i] != '\0' && (isalnum((unsigned char)line[i]) || line[i] == '_')) {
                if (k < MAX_VAR_NAME - 1) {
                    var_name[k++] = line[i];
                }
                i++;
            }

            const char *val = get_var(var_name);
            if (val != NULL) {
                for (int l = 0; val[l] != '\0' && j < sizeof(buffer) - 1; l++) {
                    buffer[j++] = val[l];
                }
            }
        } else {
            buffer[j++] = line[i++];
        }
    }
    buffer[j] = '\0';

    strncpy(line, buffer, max_len - 1);  //si finaliza sin problemas copia la "decodificacion" a la linea original y finaliza con \0
    line[max_len - 1] = '\0';
}