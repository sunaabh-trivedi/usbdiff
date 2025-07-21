CC = gcc
CFLAGS = -Wall -pg -O0

TARGET = usbdiff.exe

SRC_DIR = src
SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET)

clean:
	del /Q $(TARGET) $(SRC_DIR)\*.o 2>nul