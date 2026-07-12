CC := gcc
FLAGS := -I include/ 

SRC := $(wildcard src/*.c)
TARGET := bridge.out

$(TARGET): $(SRC)
	$(CC) $(FLAGS) $(SRC) -o $(TARGET)

test: tests/fdb_test.c tests/ring_buffer_test.c
	$(CC) $(FLAGS) src/fdb.c tests/fdb_test.c -o fdb_test.out
	$(CC) $(FLAGS) src/ring_buffer.c tests/ring_buffer_test.c -o ring_buffer_test.out

clean:
	rm -f bridge.out fdb_test.out ring_buffer_test.out 
