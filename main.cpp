#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <queue>


#define DEFAULT_NUM_OF_PHILOSOPHERS 2

#define PIPE_READ   0
#define PIPE_WRITE  1

typedef struct pipe_struct{
  int write_fds[2];
  int read_fds[2];
} duplex_pipe_t;

typedef struct msg_type{
  int id;
  int time;
  char msg[10];
} message_t;


void pipe_print(duplex_pipe_t pipe){
  printf("Pipe: write[%d][%d], read[%d][%d]\n", pipe.write_fds[0],
                                                pipe.write_fds[1],
                                                pipe.read_fds[0],
                                                pipe.read_fds[1]);
}

void print_message(message_t message){
  printf("[%d] Message: (%d, %d): %s\n", getpid(), message.id, message.time, message.msg);
}



int philosopher(int id, duplex_pipe_t pipe){

  std::queue<message_t> request_queue;
  message_t request;
  int local_logical_time;
  int nwrite, nread;

  // Close the reading fd from the writing pipe
  // writing pipe will be used only for writing
  close(pipe.write_fds[PIPE_READ]);
  // Close the writing fd from the reading pipe
  // reading pipe will be used only for reading
  close(pipe.read_fds[PIPE_WRITE]);

  // initial sleep
  sleep(1);

  // send request to access the table
  // request access to the critical section
  request.id = id;
  request.time = local_logical_time;
  strcpy(request.msg, "request");



  nwrite = write(pipe.write_fds[PIPE_WRITE], &request, sizeof(request));

  while(1){
    nread = read(pipe.read_fds[PIPE_READ], &request, sizeof(request));
    print_message(request);
  }

  exit(0);

}

int create_pipe(duplex_pipe_t **fds, int* n){
  if(*fds == NULL){
    *fds = (duplex_pipe_t *) malloc(sizeof(duplex_pipe_t));
    *n = 1;
  }else{
    *n += 1;
    *fds = (duplex_pipe_t *) realloc(*fds, *n * sizeof(duplex_pipe_t));
  }

  if(pipe( ((*fds + (*n-1))->write_fds)) == -1){
    //Error
  }
  if(pipe( (*fds + (*n-1))->read_fds ) == -1){
    //Error
  }
}


int main(int argc, char *argv[]){

  int philosophers_n = DEFAULT_NUM_OF_PHILOSOPHERS;
  duplex_pipe_t *pipes = NULL;
  int process_num;
  //Create pipes for all of the processes
  for(int i=0; i < philosophers_n; i++){
    create_pipe(&pipes, &process_num);
    pipe_print(pipes[process_num-1]);
  }

  //Create processes
  for(int i=0; i < philosophers_n; i++){
    if(fork() == 0){
      // Child process
      philosopher(i+1, pipes[i]);
    }else{
      // Parent process
      // Close the writing fd from the writing pipe
      // Parent will only read from that pipe
      close(pipes[i].write_fds[PIPE_WRITE]);
      // Close the reading fd from the reading pipe
      // Parent will only write in that pipe
      close(pipes[i].read_fds[PIPE_READ]);

      // Read fd should be non-blocking
      fcntl(pipes[i].write_fds[PIPE_READ], F_SETFL, O_NONBLOCK);
    }
  }

  message_t req_msg;
  int nread, nwrite;
  while(1){
    // Route messages from children to other children
    for(int i = 0; i < philosophers_n; i++){

        nread = read(pipes[i].write_fds[PIPE_READ], &req_msg, sizeof(req_msg));
        if(nread == -1){
          // Pipe is empty
        }else if(nread == 0){
          //EOF
        }else{
          // Read message, forward it to the destination process
          print_message(req_msg);
          if(i == (req_msg.id - 1) ){
            // Request was sent by the process,
            // broadcast it to other processes
            for(int j=0; j < philosophers_n; j++){
                if( i != j){
                  nwrite = write(pipes[j].read_fds[PIPE_WRITE], &req_msg, sizeof(req_msg));
                }
            }
          }else{
              // Other process is sending response to the request
              nwrite = write(pipes[req_msg.id-1].read_fds[PIPE_WRITE], &req_msg, sizeof(req_msg));
          }
        }


    }
  }



}
