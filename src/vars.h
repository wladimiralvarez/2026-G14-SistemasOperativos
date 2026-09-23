
// límites maximos relacionados a las variables y declaracion de inicializacion, setter, getter
// y observador que trabaja en conjunto con el setter 

#ifndef VARS_H
#define VARS_H

#define MAX_VARS 64
#define MAX_VAR_NAME 64
#define MAX_VAR_VALUE 256

void init_vars(void);
void set_var(const char *name, const char *value);
const char *get_var(const char *name);
int check_set(const char *line);

#endif