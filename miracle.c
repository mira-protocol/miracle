#include "miracle.h"

MIRACLE_DEF const char *Miracle_serverStart(Miracle_Server *self, uint16_t port,
                                            Miracle_GetResponder getResponder) {
	self->getResponder  = getResponder;
	self->clientsCount  = 0;
	self->clientsCap    = MIRACLE_CLIENTS_CAP;
	self->clientThreads = Miracle_alloc(self->clientsCap * sizeof(Miracle_ClientThread));
	Miracle_assert(self->clientThreads != NULL);
	self->sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);

	if (self->sock < 0)
		return "socket";

	if (setsockopt(self->sock, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
		return "setsockopt";

	struct sockaddr_in addr = {
		.sin_family = AF_INET,
		.sin_addr   = {.s_addr = INADDR_ANY},
		.sin_port   = htons(port),
	};
	if (bind(self->sock, (struct sockaddr*)&addr, sizeof(addr)) < 0)
		return "bind";

	if (listen(self->sock, 10) < 0)
		return "listen";

	return NULL;
}

MIRACLE_DEF const char *Miracle_serverClose(Miracle_Server *self) {
	Miracle_free(self->clientThreads);
	return close(self->sock) != 0? "close" : NULL;
}

static void Miracle_setSocketBlocking(int sock, bool enable) {
	int flags = fcntl(sock, F_GETFL, 0);
	flags = enable? flags & ~O_NONBLOCK : flags | O_NONBLOCK;
	fcntl(sock, F_SETFL, flags);
}

static bool Miracle_stringRecieve(Miracle_String *self, int sock) {
	*self = Miracle_stringAlloc(MIRACLE_INTERNAL_STRINGS_CAP);

	char ch = 0;
	while (ch != '\n') {
		if (ch != 0)
			Miracle_stringAppend(self, ch);

		ssize_t size = recv(sock, &ch, 1, MSG_WAITALL);
		if (size <= 0) {
			Miracle_stringFree(self);
			return false;
		}
	}

	return true;
}

static bool Miracle_sendGetResponse(int sock, uint8_t id, Miracle_Get res) {
	uint64_t len = res.len;
	res.len = Miracle_hton64(res.len);

	if (send(sock, &id, sizeof(id), 0) < 0)
		return false;
	if (send(sock, &res.err, sizeof(res.err), 0) < 0)
		return false;
	if (send(sock, &res.len, sizeof(res.len), 0) < 0)
		return false;

	if (len > 0) {
		if (send(sock, res.contents, len, 0) < 0)
			return false;
	}

	return true;
}

static bool Miracle_clientHandlePacket(Miracle_Client *self, uint8_t id) {
	if (id != 'G')
		return false;

	Miracle_String domain, path;

	if (!Miracle_stringRecieve(&domain, self->sock))
		return false;

	if (!Miracle_stringRecieve(&path, self->sock))
		return false;

#ifdef Miracle_log
	Miracle_log("Recieved get request from client on socket %i\n"
	            "  domain: %s\n"
	            "  path: %s\n",
	            self->sock, domain.raw, path.raw);
#endif

	Miracle_assert(self->getResponder != NULL);
	Miracle_Get res = self->getResponder(domain, path);
	bool success = Miracle_sendGetResponse(self->sock, id, res);

#ifdef Miracle_log
	if (success)
		Miracle_log("Sent get response to client on socket %i\n"
		            "  err: %i\n"
		            "  len: %llu\n"
		            "  contents: %.*s\n",
		            self->sock, res.err, (unsigned long long)res.len, (int)res.len, res.contents);
	else
		Miracle_log("Failed to send get response to client on socket %i\n", self->sock);
#endif

	Miracle_stringFree(&domain);
	Miracle_stringFree(&path);
	Miracle_free(res.contents);
	return success;
}

static bool Miracle_clientPing(Miracle_Client *self) {
	/* TODO: Does not work for checking connection, wait for yeti to add a proper ping
	         packet to the specification */
	return send(self->sock, NULL, 0, 0) >= 0;
}

static void Miracle_clientHandler(Miracle_Client *self) {
#ifdef Miracle_log
	Miracle_log("New client connected on socket %i\n", self->sock);
#endif

	time_t timer = time(NULL);
	while (true) {
		Miracle_setSocketBlocking(self->sock, false);

		uint8_t id;
		ssize_t size = recv(self->sock, &id, 1, MSG_WAITALL);
		if (size > 0) {
			Miracle_setSocketBlocking(self->sock, true);
			if (!Miracle_clientHandlePacket(self, id))
				break;
		} else if (errno != EAGAIN && errno != EWOULDBLOCK)
			break;

		if (time(NULL) - timer > 0) {
			if (!Miracle_clientPing(self))
				break;

			timer = time(NULL);
		}
	}

#ifdef Miracle_log
	Miracle_log("Client on socket %i disconnected\n", self->sock);
#endif

	self->thread->finished = true;
	Miracle_free(self);
}

static void *Miracle_clientThread(void *self) {
	Miracle_clientHandler((Miracle_Client*)self);
	return NULL;
}

static Miracle_ClientThread *Miracle_serverGetNewClientThreadSlot(Miracle_Server *self) {
	for (size_t i = 0; i < self->clientsCount; ++ i) {
		if (self->clientThreads[i].reusable)
			return self->clientThreads + i;
	}

	if (self->clientsCount ++ >= self->clientsCap) {
		self->clientsCap   *= 2;
		self->clientThreads = Miracle_realloc(self->clientThreads,
		                                      self->clientsCap * sizeof(Miracle_ClientThread));
		Miracle_assert(self->clientThreads != NULL);
	}

	return self->clientThreads + self->clientsCount - 1;
}

static void Miracle_serverNewClient(Miracle_Server *self, int sock) {
	Miracle_Client *client = Miracle_alloc(sizeof(*client));
	Miracle_assert(client != NULL);

	Miracle_ClientThread *thread = Miracle_serverGetNewClientThreadSlot(self);
	thread->finished     = false;
	thread->reusable     = false;
	client->sock         = sock;
	client->getResponder = self->getResponder;
	client->thread       = thread;

	pthread_create(&thread->pthread, NULL, Miracle_clientThread, (void*)client);
}

static void Miracle_serverTidyClientThreads(Miracle_Server *self) {
	for (size_t i = 0; i < self->clientsCount; ++ i) {
		if (!self->clientThreads[i].finished)
			continue;

		pthread_join(self->clientThreads[i].pthread, NULL);
		self->clientThreads[i].reusable = true;
	}

	size_t i = self->clientsCount;
	while (i --> 0) {
		if (!self->clientThreads[i].reusable)
			break;
	}
	self->clientsCount = i + 1;
}

MIRACLE_DEF const char *Miracle_serverAccept(Miracle_Server *self) {
	struct sockaddr_in addr;
	int sock = accept(self->sock, (struct sockaddr*)&addr, &(socklen_t){sizeof(addr)});
	if (sock < 0) {
		if (errno != EAGAIN && errno != EWOULDBLOCK)
			return "accept";
	} else
		Miracle_serverNewClient(self, sock);

	Miracle_serverTidyClientThreads(self);
	return NULL;
}

MIRACLE_DEF Miracle_String Miracle_stringAlloc(size_t cap) {
	Miracle_String self = {
		.len = 0,
		.cap = cap + 1,
		.raw = Miracle_alloc(cap + 1),
	};
	Miracle_assert(self.raw != NULL);
	self.raw[0] = '\0';
	return self;
}

MIRACLE_DEF Miracle_String Miracle_stringFromCstring(const char *cstr) {
	size_t len = strlen(cstr);
	Miracle_String self = Miracle_stringAlloc(len + 1);
	strcpy(self.raw, cstr);
	self.len = len;
	return self;
}

MIRACLE_DEF void Miracle_stringFree(Miracle_String *self) {
	Miracle_assert(self->raw != NULL);
	free(self->raw);
	self->raw = NULL;
}

MIRACLE_DEF void Miracle_stringAppend(Miracle_String *self, char ch) {
	if (++ self->len >= self->cap) {
		self->cap *= 2;
		self->raw  = Miracle_realloc(self->raw, self->cap);
		Miracle_assert(self->raw != NULL);
	}

	self->raw[self->len - 1] = ch;
	self->raw[self->len]     = '\0';
}

MIRACLE_DEF uint64_t Miracle_hton64(uint64_t n) {
	return 1 == htonl(1)? n : ((uint64_t)htonl(n & 0xFFFFFFFF) << 32) | htonl(n >> 32);
}

MIRACLE_DEF uint64_t Miracle_ntoh64(uint64_t n) {
	return 1 == ntohl(1)? n : ((uint64_t)ntohl(n & 0xFFFFFFFF) << 32) | ntohl(n >> 32);
}
