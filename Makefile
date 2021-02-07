all: GradeServer GradeClient 
GradeServer: main.c
	gcc -Wall -pthread -o GradeServer main.c
GradeClient: client.c
	gcc -lpthread -o GradeClient client.c
