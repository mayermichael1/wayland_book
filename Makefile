BUILDDIR = out
FLAGS = -Wall -Wextra

all: client server

client: $(BUILDDIR)
	gcc $(FLAGS) client.cpp -o $(BUILDDIR)/client -lwayland-client

server: $(BUILDDIR)
	gcc $(FLAGS) server.cpp -o $(BUILDDIR)/server -lwayland-server

$(BUILDDIR):
	mkdir $(BUILDDIR)

