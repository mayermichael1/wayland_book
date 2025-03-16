BUILDDIR = out
FLAGS = -Wall -Wextra

all: client 

client: $(BUILDDIR)
	gcc $(FLAGS) client.cpp -o $(BUILDDIR)/client

$(BUILDDIR):
	mkdir $(BUILDDIR)

