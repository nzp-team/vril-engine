#include <stddef.h>

#define STARTUP_MAX_ARGS 50

typedef struct startup_arguments_s {
    int    argc;
    char * argv[STARTUP_MAX_ARGS];
    char * file_buffer;
} startup_arguments_t;

int
Startup_LoadArguments(startup_arguments_t * arguments, int argc, char ** argv, const char * setup_path, char * error,
  size_t error_size);

void
Startup_FreeArguments(startup_arguments_t * arguments);

int
Startup_FindArgument(const startup_arguments_t * arguments, const char * name);

int
Startup_GetBaseDirectory(const startup_arguments_t * arguments, const char * default_directory, const char ** directory,
  char * error, size_t error_size);

void *
Startup_AllocateHeap(const startup_arguments_t * arguments, size_t default_size, size_t * heap_size, char * error,
  size_t error_size);