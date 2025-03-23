BUILDDIR = out
FLAGS = -fpermissive
DEBUG = -O0 -g

all: client server

client: $(BUILDDIR)
	gcc -o $(BUILDDIR)/client $(FLAGS) $(DEBUG) client.cpp -lwayland-client -lrt -lxkbcommon

server: $(BUILDDIR)
	gcc -o $(BUILDDIR)/server $(FLAGS) $(DEBUG) server.cpp -lwayland-server

$(BUILDDIR):
	mkdir $(BUILDDIR)

