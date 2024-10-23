#include <stdio.h>   /* fprintf, stderr */
#include <stdlib.h>  /* exit, EXIT_FAILURE */
#include <stdbool.h> /* bool, true, false */

#define Miracle_log(...) printf("[MIRACLE] " __VA_ARGS__)
#include <miracle.h>
#include <miracle.c>

#define PORT 25565

void must(const char *fail, const char *msg) {
	if (fail == NULL)
		return;

	fprintf(stderr, "Error: %s: %s() fail", msg, fail);
	exit(EXIT_FAILURE);
}

Miracle_Get getResponder(Miracle_String domain, Miracle_String path) {
	(void)domain;
	(void)path;

	Miracle_String contents = Miracle_stringFromCstring("Hello, world!");
	return (Miracle_Get){
		.err      = MIRACLE_SUCCESS,
		.len      = (uint64_t)contents.len,
		.contents = contents.raw,
	};
}

int main(void) {
	Miracle_Server server;
	must(Miracle_serverStart(&server, PORT, getResponder), "Could not start server");

	printf("Listening on port %i\n", PORT);

	while (true)
		must(Miracle_serverAccept(&server), "Could not accept connection");

	must(Miracle_serverClose(&server), "Could not close server");
	return 0;
}
