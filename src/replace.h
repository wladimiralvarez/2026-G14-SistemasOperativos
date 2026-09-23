
//función utilizada para reemplazar las variables que inicien con $ usando como criterio primero 
//revisar el mapeo local y luego el de linux, en caso de que no haya reemplazo se despliega ""

#ifndef EXPAND_H
#define EXPAND_H
#include <stddef.h>

void replace_variables(char *line, size_t max_len);

#endif