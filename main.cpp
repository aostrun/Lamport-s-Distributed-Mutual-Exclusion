#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <deque>
#include <algorithm>

#define DEFAULT_NUM_OF_PHILOSOPHERS   3
#define PHILOSOPHER_REQUEST_PROB      0.5

#define PIPE_READ   0
#define PIPE_WRITE  1

#define REQUEST_MSG   "request"
#define RESPONSE_MSG  "response"
#define EXIT_MSG      "exit"

typedef struct pipe_struct{
  int write_fds[2];
  int read_fds[2];
} duplex_pipe_t;

typedef struct msg_type{
  int id;
  int time;
  char msg[10];

} message_t;


bool msg_sort(message_t i, message_t j){
  return i.time < j.time;
}

void pipe_print(duplex_pipe_t pipe){
  printf("Pipe: write[%d][%d], read[%d][%d]\n", pipe.write_fds[0],
                                                pipe.write_fds[1],
                                                pipe.read_fds[0],
                                                pipe.read_fds[1]);
}

void print_message(message_t message){
  printf("[%d] Message: (%d, %d): %s\n", getpid(), message.id, message.time, message.msg);
}

int randomWithProb(double p){
  double rndDouble = (double)rand() / RAND_MAX;
  return rndDouble < p;
}


void create_response(message_t *message, int id, int loc_time){
  message->id = id;
  message->time = loc_time;
  strcpy(message->msg, RESPONSE_MSG);
}

void create_request(message_t *message, int id, int loc_time){
  message->id = id;
  message->time = loc_time;
  strcpy(message->msg, REQUEST_MSG);
}

void create_exit(message_t *message, int id, int loc_time){
  message->id = id;
  message->time = loc_time;
  strcpy(message->msg, EXIT_MSG);
}


int philosopher(int id, duplex_pipe_t pipe, int philosophers_num){

  std::deque<message_t> request_queue;
  message_t request, response, message, tmp;
  int local_logical_time = id * 200;
  int nwrite, nread, random_bool;
  int responses = 0;
  char wait_for_responses = 0;
  srand(id + time(NULL));

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





  while(1){

    sleep(1);
    // Randomly decide if philosopher want to request access to the table
    // get the LSB of the random value == boolean random
    if(wait_for_responses == 0 && randomWithProb(PHILOSOPHER_REQUEST_PROB)){
        // Create request, add it to the local queue and broadcast it to
        // other processes and wait for the responses
        printf("%d wants to access the table %d...\n", id, local_logical_time);
        create_request(&request, id, local_logical_time);
        nwrite = write(pipe.write_fds[PIPE_WRITE], &request, sizeof(request));
        wait_for_responses = 1;
        request_queue.push_back(request);
        responses = 0;
    }

    if(!request_queue.empty()){
      std::sort(request_queue.begin(), request_queue.end(), msg_sort);
    }

    // Read from pipe
    nread = read(pipe.read_fds[PIPE_READ], &message, sizeof(message));

    // Increment local logical time when new message arrives
    // local_time = max{local_time, message.time} + 1
    local_logical_time = (local_logical_time < message.time ? message.time : local_logical_time) + 1;

    // Parse the received message
    if(strncmp(message.msg, REQUEST_MSG, strlen(REQUEST_MSG)) == 0){
        // Received msg is a request
        /*
        if(wait_for_responses){
          // Process also wants to access the table
          // check if the request had lower time value
          if(message.time < request.time){
            // Respond to the request, this process has to wait
            printf("\t%d has to wait, %d has advantage\n", id, message.id);
            create_response(&response, message.id, local_logical_time);
            request_queue.push_front(message);
            wait_for_responses = 2;
            nwrite = write(pipe.write_fds[PIPE_WRITE], &response, sizeof(response));
          }else{
            // This process has the advantage
            request_queue.push_back(message);
          }
        }else{
        */
        // Respond to the request
        create_response(&response, message.id, local_logical_time);
        request_queue.push_back(message);
        nwrite = write(pipe.write_fds[PIPE_WRITE], &response, sizeof(response));


    }else if(strncmp(message.msg, RESPONSE_MSG, strlen(RESPONSE_MSG)) == 0){
        // Received msg is a response
        responses++;
        printf("%d got response, #: %d, wfr: %d\n", id, responses, wait_for_responses);
    }else if(strncmp(message.msg, EXIT_MSG, strlen(EXIT_MSG)) == 0){
        // Exit message arrived, pop the front request from queue

        tmp = request_queue.front();
        printf("%d removing (%d, %d)\n", id, tmp.id, tmp.time);
        request_queue.pop_front();
        if(wait_for_responses){
          // If this process wants to access the table but was not in
          // front of the queue before, check the next request in the queue
          // if next request is "mine", set wait_for_responses to 1
          message = request_queue.front();
          printf("\t%d has next in line (%d, %d)\n", id, message.id, message.time);

        }
    }

    tmp = request_queue.front();
    if(tmp.id == id && responses >= (philosophers_num - 1)){
      // All processes sent responses, access the table
      printf("%d accessing table...\n", id);
      sleep(2);
      // Remove own request from queue
      request_queue.pop_front();
      wait_for_responses = 0;
      responses = 0;

      printf("%d leaving table...\n", id);
      create_exit(&response, id, request.time);
      print_message(response);
      nwrite = write(pipe.write_fds[PIPE_WRITE], &response, sizeof(response));

      /*
      while(!request_queue.empty()){
        // While there is a request on a local queue, respond to it
        request = request_queue.front();
        request_queue.pop_front();

        create_response(response, request.id, local_logical_time);
        nwrite = write(pipe.write_fds[PIPE_WRITE], &response, sizeof(response));

      }
      */
    }

    //print_message(message);
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
      printf("Created child %d:%d\n", i+1, getpid());
      philosopher(i+1, pipes[i], philosophers_n);
      break;
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
  /**
    * Parent process is used as router for messages (requests)
    * between pipes (child processes).
   */
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
          //print_message(req_msg);
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
