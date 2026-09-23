
//definir el struct mas un arreglo para mapear localmente y los metodos para inicializar/modificar

#include "vars.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "mishell.h"

typedef struct {
    char name[MAX_VAR_NAME];
    char value[MAX_VAR_VALUE];
}mishell_var_par;

static mishell_var_par var_table[MAX_VARS];
static int var_count = 0;

void init_vars(void){
    var_count = 0;
    set_var("USER","Grupo 14");     //variables de prueba que no cuentan dentro del maximo
    set_var("SHELL","mishell");
}

void set_var(const char *name, const char *value){
    for (int i = 0; i < var_count; i++){
        if (strcmp(var_table[i].name, name) == 0){
            strncpy(var_table[i].value, value, MAX_VAR_VALUE -1);     //si ya existe reemplaza y retorna
            return;
        } 
    }
    if (var_count < MAX_VARS){
        strncpy(var_table[var_count].name, name, MAX_VAR_NAME);
        strncpy(var_table[var_count].value, value, MAX_VAR_VALUE);  //si no existe agrega en la posicion del contador
        var_count++;
        return;
    }
}

const char *get_var(const char *name){
    for (int i = 0; i < var_count; i++){
        if (strcmp(var_table[i].name, name) == 0){
            return var_table[i].value;
        }
    }
    return getenv(name);  
}

int valid_name(const char *str){    //si se realiza un seteo comprueba que el nombre que se le asignará sea valido
    if (str == NULL || *str == '\0' || isdigit((unsigned char) *str)){  //revisa que el string no sea nulo ni inicie con un caracter invalido
        return 0;
    }
    for (int i = 0; str[i] != '\0'; i++){
        if (!isalnum((unsigned char)str[i]) && str[i] != '_'){
            return 0;
        }
    }
    return 1;
}

int check_set(const char *line){  //chequea si se está seteando algún valor en la shell  (mediante nombre=valor)
    char buffer[MAX_LINE];   //inicializacion de la copia
    strncpy(buffer, line, sizeof(buffer));
    buffer[sizeof(buffer)-1] = '\0';
    buffer[strcspn(buffer,"\n\r")] = '\0';

    //se separan si hay un signo "=" y desde esa posición se revisa el par nombre/valor
    char *pos_eq = strchr(buffer, '=');
    if (pos_eq == NULL){
        return 0;  
    }
    *pos_eq = '\0';
    char *name = buffer;
    char *value = pos_eq + 1;

    if (!valid_name(name) || *value == ' '){
        return 0;
    }

    set_var(name, value);
    return 1;

}