#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "types.h"

void init_buffers(void);
void cleanup(void);
void show_usage(bool windows);

void prepare_ok_response(void);
void prepare_error_response(void);
