/* A simple server in the internet domain using TCP
   The port number is passed as an argument */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include "myqueue.h"
#define THREAD_POOL_SIZE 5
#define BUFSIZE 256
#define SIZE_OF_PASSWORD 32
#define SIZE_OF_GRADE_LIST_ENTRY 15

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex2 = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condition_var = PTHREAD_COND_INITIALIZER;
pthread_t thread_pool[THREAD_POOL_SIZE];

void INThandler(int);

enum permissions{Student, Assistant, Not_exist}; // This will be use to track what are the user permissions
void  INThandler(int sig)
{
    for (int i =0;i < THREAD_POOL_SIZE; i++) {
        pthread_kill(&thread_pool[i], sig);
    }
    exit(1);
}

typedef struct student{
	int id;
	char password[SIZE_OF_PASSWORD];
	int grade;
	struct student *next;
}student_t;


typedef struct assistant{
	int id;
	char password[SIZE_OF_PASSWORD];
	struct assistant *next;
}assistant_t;


// we have to make an data structure that keeps all the sockets if they had a user that loged in from
// and a nother 2 to remove and add those and to make it atomic and have the same mutex.
// function that starts


typedef struct user_logged{
	int socket_id;
	enum permissions permission;
	assistant_t *ass_curr;
	student_t *stud_curr;
}user_logged;

void * handle_connection(void* p_client_socket);

student_t *search_student_by_id(int id);

void error(const char *msg);

student_t *create_student(int id, char *password);

user_logged *create_user(int socket);

assistant_t *create_assistant(int id, char *password);

student_t *add_student_to_ordered_list(student_t *head, student_t *new_student);

assistant_t *add_assistant_to_ordered_list(assistant_t *head, assistant_t *new_assistant);

void GradeList(user_logged *user,int client_socket);

//Decalre on function
student_t *read_students_file();
assistant_t *read_assistants_file();
//Declare on pointers
student_t *students_list;
assistant_t *assistants_list;

void start(char *argv[]);

assistant_t *search_assistant(int id, char *password);
student_t *search_student_by_id(int id);

int Login(char *buf, user_logged *user, int id, char *password);

int Logout(char *buf, user_logged *user);

int ReadGrade(char *buf, user_logged *user, int id);

int UpdateGrade(char *buf, user_logged *user, int id, int grade);

void * thread_function(void *arg);



node_t* head = NULL;
node_t* tail = NULL;

void enqueue(int *client_socket) { // queue for commands from client.
    node_t *newnode = malloc(sizeof(node_t));
    newnode->client_socket = client_socket;
    newnode->next = NULL;
    if (tail == NULL) {
        head = newnode;
    } else {
        tail->next = newnode;
    }
    tail = newnode;
}


int* dequeue() {
    if (head == NULL) {
        return NULL;
    } else {
        int *result = head->client_socket;\
        node_t *temp = head;
        head = head->next;
        if (head == NULL) {tail = NULL;}
        free(temp);
        return result;
    }
}

assistant_t *search_assistant(int id, char *password){
	assistant_t *current_assistant = assistants_list;
	while (current_assistant && current_assistant->id <= id){
		if(current_assistant->id == id){// there is a assistant with this id
            char temp[256];
            strcpy(temp,current_assistant->password);
			if( strcmp(current_assistant->password, password) == 0){ // the password fits
				return current_assistant;
			} else {
				return NULL;
			}
		}
		current_assistant= current_assistant->next;
	}
	return NULL;
}

student_t *search_student_by_id(int id){
	student_t *current_student = students_list;
	while (current_student && current_student->id <= id){
		if(current_student->id == id){// there is a student with this id
				return current_student;
			}
		current_student= current_student->next;
	}
	return NULL;
}

// Function: Login
// Description: Checks how is the user and update the users permision
// Gets: Pointer to user_logged struct,  pointer to the head of students_list,
//       pointer to the head of assistants_list, id and password
// Returns: 1 if success 0 if faild
int Login(char *buf, user_logged *user, int id, char *password){
	student_t *stud = search_student_by_id(id);
	if(stud != NULL){
		if (strcmp(stud->password, password) == 0 ){ // check his password
			user->stud_curr = stud;
			user->permission = Student;
			snprintf(buf, 255, "Welcome Student %09d\n", user->stud_curr->id);
			return 1;
		} else {
			strcpy(buf,"Wrong user information\n");
			return 0;
		}
	}

	assistant_t *ass = search_assistant(id,password);
	if(ass != NULL){
		user->ass_curr = ass;
		user->permission = Assistant;
		snprintf(buf, 255, "Welcome TA %09d\n", user->ass_curr->id);
		return 1;
	}
	strcpy(buf,"Wrong user information\n");
	return 0;
}

// Function: ReadGrade
// Description: Save the grade of a student in a buffer. Studernt allow only to ask for his personal grade,
//              TA allowed to ask for a specific student grade he wants
// Gets: Pointer to buffer, struct pointer to user_logged struct,  pointer to the head of students_list,
//       id (if it is a student id = 0, TA need to send a number > 0)
// Returns: 1 if success 0 if faild
int ReadGrade(char *buf, user_logged *user, int id){
	// check if the user is a student
	if(user->permission == Student){
		if (id != 0){ // the student not allowed to sent id argument
			strcpy(buf, "Action not allowed\n");
			return 0;
		} else { // save his grade
			snprintf(buf, 255, "%d\n", user->stud_curr->grade);
			return 1;
		}
	}

	// if its not a student check if it is a TA
	if(user->permission == Assistant){
		if (id == 0){ // TA did not add id of a student as an argument
			strcpy(buf, "Missing argument\n");
			return 0;
		} else {
			student_t *stud = search_student_by_id(id); // find the right student struct
			if(stud != NULL){ // save his grade
				snprintf(buf, 255, "%09d:%d\n", stud->id, stud->grade);
				return 1;
			} else { // did not find student with such id
				strcpy(buf, "Invalid id\n");
				return 0;
			}

		}
	}

	//otherwise the user not loged in
	strcpy(buf, "Not logged in\n");
	return 0;
}

// Function: UpdateGrade
// Description: Update the grade of a specific student. this command allowed only for TA.
// Gets: Pointer to buffer, struct pointer to user_logged struct, id and grade
// Returns: 1 if success 0 if faild
int UpdateGrade(char *buf, user_logged *user, int id, int grade){

	if(user->permission == Student){// student not allowed to use the function
		strcpy(buf, "Action not allowed\n");
		return 0;
	}

	else if(user->permission == Assistant){ // TA is allowed to update a grade
		student_t *stud = search_student_by_id(id);
		if(stud != NULL){ // if the student exist in the linked lis, update his grade
			stud->grade = grade;
		} else{
			// if there is no such of student, creat new one and add him to the linked list
			stud = create_student(id,"");
			stud->grade = grade;
			students_list = add_student_to_ordered_list(students_list, stud);
		}
		return 1;
	} else{// otherwise the user not loged in
		strcpy(buf, "Not logged in\n");
		return 0;
	}


}


void error(const char *msg)
{
    perror(msg);
    exit(1);
}


// Function: create_student
// Description: Create new student struct.
// Gets: id and password
// Returns: Pointer to new student struct

student_t *create_student(int id, char *password){

	if (0 >= id){
		printf ("Invalid student id");
		return NULL;
	}
	student_t *new_student = malloc(sizeof(student_t));
	if(NULL == new_student){
		printf ("Failed with creating new student");
		return NULL;
	}
    bzero(new_student, sizeof(student_t));
	strncpy(new_student->password ,password, SIZE_OF_PASSWORD);
	new_student->id = id;
	new_student->grade = 0;
	new_student->next = NULL;

	return new_student;

}

user_logged *create_user(int socket){

	user_logged *new_user= malloc(sizeof(user_logged));
	if(NULL == new_user){
		printf ("Failed with creating new user");
		return NULL;
	}

	new_user->socket_id = socket;
	new_user->permission = Not_exist;
	new_user-> ass_curr = NULL;
	new_user-> stud_curr = NULL;
	return new_user;
}
// Function: create_assistant
// Description: Create new assistant struct.
// Gets: id and password
// Returns: Pointer to new assistant struct
assistant_t *create_assistant(int id, char *password){

	if (0 >= id){
		printf ("Invalid assistant id");
		return NULL;
	}
	if (0 == strlen(password)){
		printf ("Invalid assistant Password");
		return NULL;
	}

	assistant_t *new_assistant = malloc(sizeof(assistant_t));
	if(NULL == new_assistant){
		printf ("Failed with creating new assistant");
		return NULL;
	}
	bzero(new_assistant, sizeof(assistant_t));
	strncpy(new_assistant->password ,password, SIZE_OF_PASSWORD);
	new_assistant->id = id;
	new_assistant->next = NULL;

	return new_assistant;

}

// Function: add_student_to_ordered_list
// Description: Adds new student to the link list. The list is ordered in an ascending id order.
// Gets: Pointer to the head of link list of student structs and a pointer to a new student struct
// Returns: Pointer to the head of student link list

student_t *add_student_to_ordered_list(student_t *p_student_list, student_t *p_new_student){

	student_t *current, *prev = NULL;

	if(NULL == p_student_list){
		return p_new_student;
	}
	if(p_new_student->id < p_student_list->id){
		p_new_student->next = p_student_list;
		return p_new_student;
	}

	current = p_student_list;
	while(current && current->id < p_new_student->id){
		prev = current;
		current = current->next;
	}
	prev->next = p_new_student;
	p_new_student->next = current;
	return p_student_list;
}


// Function: add_assistant_to_ordered_list
// Description: Adds new assistant to the link list. The list is ordered in an ascending id order.
// Gets: Pointer to the head of link list of assistant structs and a pointer to a new assistant struct
// Returns: Pointer to the head of assistant link list


assistant_t *add_assistant_to_ordered_list(assistant_t *head, assistant_t *new_assistant){

	assistant_t *current, *prev = NULL;

	if(NULL == head){
		return new_assistant;
	}
	if(new_assistant->id < head->id){
		new_assistant->next = head;
		return new_assistant;
	}

	current = head;
	while(current && current->id < new_assistant->id){
		prev = current;
		current = current->next;
	}
	prev->next = new_assistant;
	new_assistant->next = current;
	return head;
}

int get_num_students(){
    student_t *p_student = students_list;
    int counter = 0;
    while(p_student){
        counter++;
        p_student = p_student->next;
    }
    return counter;
}
// Function: GradeList
// Description: Prints all the grades sorted by the id number
// Gets: Pointer to the head of link list of student structs
void GradeList(user_logged *user, int client_socket){
    int buffer_size = 0;
    char *buffer = NULL;
    unsigned int num_bytes = 0;
    //Lock
    pthread_mutex_lock(&mutex2);
    buffer_size = get_num_students()*SIZE_OF_GRADE_LIST_ENTRY +20;
    buffer = malloc(buffer_size);
	if(user->permission == Student){
		strcpy(buffer, "Action not allowed\n");
	} else if(user->permission == Assistant){
		student_t *p_student = students_list;
		int position = 0;
		while(p_student){
			int n = snprintf(buffer+position,SIZE_OF_GRADE_LIST_ENTRY, "%09d:%d\n",p_student->id,p_student->grade);
            position+=n;
			p_student = p_student->next;
		}
	} else{
		strcpy(buffer, "Not logged in\n");
	}
	//unlock
	pthread_mutex_unlock(&mutex2);
	num_bytes = htonl(strlen(buffer)+1);
    write(client_socket,&num_bytes,sizeof(num_bytes));
    write(client_socket,buffer,strlen(buffer));
    free(buffer);

}
// Function: Logout
// Description: Logout from the system
// Gets: Pointer to buffer and struct pointer to user_logged struct
// Returns: 1 if success 0 if faild
int Logout(char *buf, user_logged *user){
	if(user->permission == Student){
		int id = user->stud_curr->id;
		user->permission = Not_exist;
		user->stud_curr = NULL;
		snprintf(buf, 255, "Good bye %09d\n", id);
		return 1;

	} else if (user->permission == Assistant){

		int id = user->ass_curr->id;
		user->permission = Not_exist;
		user->ass_curr = NULL;
		snprintf(buf, 255, "Good bye %09d\n", id);
		return 1;

	} else {

		strcpy(buf, "Action not allowed\n");
		return 0;
	}
}
// Function: read_students_file
// Description: Creates a data structure from the students.txt file
// Returns: Pointer to the head of the link list of student structs

student_t *read_students_file()
{
	student_t *p_student_list = NULL;
	char line[256];

	FILE *file = fopen("students.txt","r");
	if(NULL == file){
		printf("Failed to open stutends.txt file");
		return NULL;
	}
	while(fgets(line,sizeof(line),file))
	{
		char *id = NULL;
		char *password = NULL;
		//separates the id and the password using ':'
		id = strtok(line,":");
		password = strtok(NULL,"\n");
		// create new student struct
		student_t *p_new_student = create_student(atoi(id),password);
		if (NULL == p_new_student){
			exit(1);
		}
		// update the head of the link list
		p_student_list = add_student_to_ordered_list(p_student_list, p_new_student);
	}
	fclose(file);
	return p_student_list;
}


// Function: read_assistants_file
// Description: Creates a data structure from the assistants.txt file
// Returns: Pointer to the head of the link list of assistant structs

assistant_t *read_assistants_file()
{
	assistant_t *head = NULL;
	char line[256];

	FILE *file = fopen("assistants.txt","r");
	if(NULL == file){
		printf("Failed to open assistants.txt file");
		return NULL;
	}

	while(fgets(line,sizeof(line),file))// read line by line
	{
		char *id;
		char *password;

		//separates the id and the password using ':'
        id = strtok(line,":");
		password = strtok(NULL,"\n");

		// create new assistant struct
		assistant_t *new_assistant = create_assistant(atoi(id),password);
		if (NULL == new_assistant){
			exit(1);
		}
		// update the head of the link list
		head = add_assistant_to_ordered_list(head, new_assistant);
	}
	fclose(file);
	return head;
}



void start(char *argv[]){

    socklen_t clilen;
    int sockfd, newsockfd, portno;
    struct sockaddr_in serv_addr, cli_addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0)
        error("ERROR opening socket");

    bzero((char *) &serv_addr, sizeof(serv_addr));

    portno = atoi(argv[1]);

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR on binding");

    listen(sockfd,5);
    clilen = sizeof(cli_addr);

    while(1)
    {
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        if (newsockfd < 0)
            error("ERROR on accept");
        int *pclient = malloc(sizeof(int));
        *pclient = newsockfd;
        pthread_mutex_lock(&mutex);
        enqueue(pclient);
        pthread_cond_signal(&condition_var);
        pthread_mutex_unlock(&mutex);
    }
    close(sockfd);
}

void * thread_function(void *arg) {
    while (1)
   {
        int *pclient = dequeue();
        pthread_mutex_lock(&mutex);
        if (pclient = dequeue() == NULL) {
            pthread_cond_wait(&condition_var,&mutex);
            pclient = dequeue();
			}
        pthread_mutex_unlock(&mutex);
        if (pclient != NULL){
            handle_connection(pclient);
        }
	}
}

void * handle_connection(void* p_client_socket){
    int client_socket = *((int*)p_client_socket);
    free(p_client_socket);
    int n;
    char *buffer = malloc(sizeof(char)*BUFSIZE);
    char *res = malloc(sizeof(char)*BUFSIZE);

	user_logged *current_user = create_user(client_socket);
	if (current_user == NULL)
		error("ERROR on malloc of user_logged struct");
    while (1) {
		bzero(buffer,BUFSIZE);
		bzero(res,BUFSIZE);
		n = read(client_socket,buffer,255);
		if (n == 0){
            free(buffer);
            free(res);
            free(current_user);
			break;
        }
		char * string = strtok(buffer, " ");

		if (strcmp(string, "Login") == 0)
		{ // "Login 3084831324 lol2kok"
            char *array[2];
            int i=0;
            array[i] = strtok(NULL," ");
            while(array[i]!=NULL)
            {
               array[++i] = strtok(NULL," ");
            }
            int id = atoi(array[0]);
            //lock
            pthread_mutex_lock(&mutex2);
            Login(res, current_user,id, array[1]);
            //unlock
            pthread_mutex_unlock(&mutex2);
			write(client_socket,res,255);
			bzero(res,256);
		}
		else if (strcmp(string, "ReadGrade") == 0)
		{
            int id=0;
			char *array[1];
			int i=0;
			array[i] = strtok(NULL," ");
		    while(array[i]!=NULL){
                array[++i] = strtok(NULL," ");
		    }
		    if(array[0] != NULL){
                id = atoi(array[0]);
		    }
		    //lock
		    pthread_mutex_lock(&mutex2);
		    ReadGrade(res,current_user,id);
		    //unlock
		    pthread_mutex_unlock(&mutex2);
		    write(client_socket,res,255);
		    bzero(res,256);
		}
		else if (strcmp(string, "UpdateGrade") == 0)
		{
		    char *array[2];
            int i=0;
            array[i] = strtok(NULL," ");
            while(array[i]!=NULL)
            {
               array[++i] = strtok(NULL," ");
            }
            int id = atoi(array[0]);
            int grade = atoi(array[1]);
            //lock
            pthread_mutex_lock(&mutex2);
            UpdateGrade(res, current_user,id, grade);
            //unlock
            pthread_mutex_unlock(&mutex2);
			write(client_socket,res,255);
			bzero(res,256);
		}
		else if (strcmp(string, "GradeList") == 0)
		{
            // lock and unlock inside the function itself
            GradeList(current_user,client_socket);
		}
		else if (strcmp(string, "Logout") == 0)
		{
			//lock
			pthread_mutex_lock(&mutex2);
            Logout(res, current_user);
            //unlock
            pthread_mutex_unlock(&mutex2);
            write(client_socket,res,255);
            bzero(res,256);
		}
        else if (strcmp(string, "Exit") == 0)
		{
            pthread_mutex_lock(&mutex2);
            Logout(res, current_user);
            pthread_mutex_unlock(&mutex2);
            bzero(res,256);
		}
    }
}
int main(int argc, char *argv[])
{
	// load users data
	signal(SIGINT, INThandler);
    students_list = read_students_file();
    assistants_list = read_assistants_file();
    if (argc < 2) {
       fprintf(stderr,"ERROR, no port provided\n");
       exit(0);
    } else {
        for (int i =0;i < THREAD_POOL_SIZE; i++) {
        pthread_create(&thread_pool[i], NULL, thread_function, NULL);
        }
        start(argv);
    }
    return 0;
}
