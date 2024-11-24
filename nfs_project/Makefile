CC = gcc
CFLAGS = -l pthread

CLIENT_SRC = clients/client.c clients/client_utils.c
CLIENT_OUT = clients/client
CLIENT_LIBS = -lportaudio -lsndfile

STORAGE_SERVER_SRC = storage_server/storage_server.c
STORAGE_SERVER_OUT = storage_server/storage_server

NAMING_SERVER_SRC = naming_server/naming_server.c
NAMING_SERVER_OUT = naming_server/naming_server

all: client storage_server naming_server

client: $(CLIENT_SRC)
    @echo "Compiling client..."
    $(CC) $(CLIENT_SRC) -o $(CLIENT_OUT) $(CLIENT_LIBS) $(CFLAGS)
    @if [ $$? -eq 0 ]; then echo "Client compiled successfully."; else echo "Failed to compile client."; exit 1; fi

storage_server: $(STORAGE_SERVER_SRC)
    @echo "Compiling storage server..."
    $(CC) $(STORAGE_SERVER_SRC) -o $(STORAGE_SERVER_OUT) $(CFLAGS)
    @if [ $$? -eq 0 ]; then echo "Storage server compiled successfully."; else echo "Failed to compile storage server."; exit 1; fi

naming_server: $(NAMING_SERVER_SRC)
    @echo "Compiling naming server..."
    $(CC) $(NAMING_SERVER_SRC) -o $(NAMING_SERVER_OUT) $(CFLAGS)
    @if [ $$? -eq 0 ]; then echo "Naming server compiled successfully."; else echo "Failed to compile naming server."; exit 1; fi

clean:
	rm -f $(CLIENT_OUT) $(STORAGE_SERVER_OUT) $(NAMING_SERVER_OUT)

.PHONY: all client storage_server naming_server clean