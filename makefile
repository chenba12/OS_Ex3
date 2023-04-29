CFLAGS=-Wall -Wextra -O2
LDFLAGS=-ldl
# Executable name
TARGET = stnc

# Source and object files
SOURCES = stnc.c utils.c tcpUtils.c udpUtils.c pipeUtils.c mmapUtils.c udsUtils.c
OBJECTS = $(SOURCES:.c=.o)
#used to remove the need for export LD_LIBRARY_PATH=.
RPATH=-Wl,-rpath=./

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)
