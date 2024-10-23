#ifndef MIRA_H_HEADER_GUARD
#define MIRA_H_HEADER_GUARD

#include <stdint.h>  /* uint8_t, uint16_t, uint64_t */
#include <stdlib.h>  /* malloc, realloc, free, size_t */
#include <stdbool.h> /* bool, true, false */
#include <time.h>    /* time, time_t */
#include <errno.h>   /* errno */
#include <string.h>  /* strcpy, strlen */

#include <fcntl.h>      /* fcntl */
#include <arpa/inet.h>  /* htons, htonl, ntohl */
#include <unistd.h>     /* close */
#include <sys/socket.h> /* socket, send, recv */
#include <pthread.h>    /* pthread_t, pthread_create, pthread_join */

#ifndef MIRACLE_DEF
#	define MIRACLE_DEF
#endif

#ifndef Miracle_alloc
#	define Miracle_alloc(SIZE) malloc(SIZE)
#endif

#ifndef Miracle_realloc
#	define Miracle_realloc(PTR, SIZE) realloc(PTR, SIZE)
#endif

#ifndef Miracle_free
#	define Miracle_free(PTR) free(PTR)
#endif

#ifndef Miracle_assert
#	include <assert.h> /* assert */

#	define Miracle_assert(COND) assert(COND)
#endif

#ifndef MIRACLE_CLIENTS_CAP
#	define MIRACLE_CLIENTS_CAP 32
#endif

#ifndef MIRACLE_INTERNAL_STRINGS_CAP
#	define MIRACLE_INTERNAL_STRINGS_CAP 32
#endif

typedef enum {
	MIRACLE_SUCCESS = 0,
	MIRACLE_PAGE_NOT_FOUND,
	MIRACLE_BAD_PATH,
	MIRACLE_UNKNOWN_DOMAIN,
} Miracle_Error;

typedef struct {
	uint8_t  err; /* Miracle_Error */
	uint64_t len;
	char    *contents;
} Miracle_Get;

/* TODO: Text input packet */
typedef struct {
	char *prompt;
} Miracle_TextInput;

typedef struct {
	pthread_t pthread;
	bool      finished, reusable;
} Miracle_ClientThread;

typedef struct {
	char  *raw;
	size_t len, cap;
} Miracle_String;

typedef Miracle_Get (*Miracle_GetResponder)(Miracle_String domain, Miracle_String path);

typedef struct {
	int sock;
	Miracle_GetResponder  getResponder;
	Miracle_ClientThread *thread;
} Miracle_Client;

typedef struct {
	int sock;
	Miracle_GetResponder getResponder;

	/* Clients pool */
	size_t clientsCount, clientsCap;
	Miracle_ClientThread *clientThreads;
} Miracle_Server;

MIRACLE_DEF const char *Miracle_serverStart(Miracle_Server *self, uint16_t port,
                                            Miracle_GetResponder getResponder);
MIRACLE_DEF const char *Miracle_serverClose (Miracle_Server *self);
MIRACLE_DEF const char *Miracle_serverAccept(Miracle_Server *self);

/* Utils */
MIRACLE_DEF Miracle_String Miracle_stringAlloc(size_t cap);
MIRACLE_DEF Miracle_String Miracle_stringFromCstring(const char *cstr);
MIRACLE_DEF void Miracle_stringFree  (Miracle_String *self);
MIRACLE_DEF void Miracle_stringAppend(Miracle_String *self, char ch);

MIRACLE_DEF uint64_t Miracle_hton64(uint64_t n);
MIRACLE_DEF uint64_t Miracle_ntoh64(uint64_t n);

#endif
