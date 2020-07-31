#pragma comment(lib,"cabinet.lib")

#include <iostream>
#include <stdexcept>
#include <string>
#include <windows.h>
#include <compressapi.h>

#include "tchar.h"

#if defined(UNICODE) || defined(_UNICODE)
#define cout wcout
#define cerr wcerr
#endif

#define STATUS_SUCCESS 0
#define STATUS_ERROR 1

void print_usage();

int compress_file();

int decompress_file();

int write_output_buffer();

int cleanup(int);