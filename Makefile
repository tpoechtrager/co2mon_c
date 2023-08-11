CC = gcc
CFLAGS = -Wall -Wextra -O2
LIBS = -lhidapi-libusb

SOURCES = main.c
EXECUTABLE = co2mon

all: $(SOURCES)
	$(CC) $(CFLAGS) $(SOURCES) -o $(EXECUTABLE) $(LIBS)

clean:
	rm -f $(EXECUTABLE)
