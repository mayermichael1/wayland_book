BUILDDIR = out
FLAGS = -Wall -Wextra

all: client server

client: $(BUILDDIR)
	gcc -o $(BUILDDIR)/client $(FLAGS) client.cpp -lwayland-client

server: $(BUILDDIR)
	gcc -o $(BUILDDIR)/server $(FLAGS) server.cpp -lwayland-server

$(BUILDDIR):
	mkdir $(BUILDDIR)

