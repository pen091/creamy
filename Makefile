CC=gcc
FLAGS=-pthread
OUTPUT=-o

all : server client stress_test

server :
	$(CC) server.c $(OUTPUT) server $(FLAGS)

client :
	$(CC) client.c $(OUTPUT) client $(FLAGS)

stress_test :
	$(CC) stress_test.c $(OUTPUT) stress_test $(FLAGS)

run :
	./client

clean :
	rm server
	rm stress_test
	rm client
