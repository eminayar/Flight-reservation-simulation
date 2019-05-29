#include <cstdio>
#include <string>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <chrono>
#include <thread>
#include <ctime>
#include <vector>
#include <pthread.h>
#include <mutex>
#include <semaphore.h>
#include <fcntl.h>
#include <unistd.h>

using namespace std;

typedef pair <int,int> ii;
int request[500],seats[500],done[500];
pthread_cond_t request_condition[500];
pthread_mutex_t result_mutex,request_condition_mutex[500];
pthread_mutex_t seat_mutex;
vector <ii> res;

struct thread_data{
    int id,number_of_seats;
};

void *server(void *threadarg) {
    //get thread id
    struct thread_data *my_data;
    my_data = (struct thread_data *) threadarg;
    bool flag=true;
    //rezervation value
    int rez=-1;
    //acquire lock to check rezervation requests
    pthread_mutex_lock(&request_condition_mutex[my_data->id]);
    while( flag ){
        //wait until a request is made. This releases the acquired lock until it receives a signal, then acquires the lock again.
        pthread_cond_wait(&request_condition[my_data->id], &request_condition_mutex[my_data->id]);
        //acquire lock to access the seat data
        pthread_mutex_lock(&seat_mutex);
        //look who is sitting at the requested place
        int seat_value = seats[request[my_data->id]];
        //if no one has taken the seat yet take it
        if( seat_value == -1 ){
            //place our client to the seat
            seats[request[my_data->id]]=my_data->id;
            //where the client is seated
            rez=request[my_data->id];
            //let the client know his request is granted
            done[my_data->id]=1;
            //we can stop this thread now
            flag=false;
            //acquire lock to append the result with the current pair
            pthread_mutex_lock(&result_mutex);
            //append the result
            res.push_back( ii(my_data->id+1,rez+1) );
            //release the result lock so that other servers can append it too.
            pthread_mutex_unlock(&result_mutex);
        }
        //we are done with the seats.
        pthread_mutex_unlock(&seat_mutex);
    }
    //release the final lock. Since in every iteration in while lock is acquired and released by the cond, finally it stays as acquired.
    //so we need to release it.
    pthread_mutex_unlock(&request_condition_mutex[my_data->id]);
   
    pthread_exit(NULL);
    
    return NULL;
}

void *client(void *threadarg) {
    //get thread id and the number of seats
    struct thread_data *my_data;
    my_data = (struct thread_data *) threadarg;
    //initialize random seed differently for every client
    srand(time(0)*(my_data->id+1));
    //this thread sleeps for a random period between 50-200
    std::this_thread::sleep_for(std::chrono::milliseconds(rand()%151+50));
    bool flag=true;
    while(flag){
        //acquire lock to check if this client is placed somewhere
        pthread_mutex_lock(&request_condition_mutex[my_data->id]);
        if(done[my_data->id]){
            break;
        }
        //release the lock so others can do the same
        pthread_mutex_unlock(&request_condition_mutex[my_data->id]);
        //randomly select a seat
        int my_request =rand()%my_data->number_of_seats;
        //acquire lock to make a request to the server. This lock is between the server and the clients since it uses id.
        pthread_mutex_lock(&request_condition_mutex[my_data->id]);
        //make the request
        request[my_data->id]=my_request;
        //signal the server to let it know a request has been made.
        pthread_cond_signal(&request_condition[my_data->id]);
        //release the request lock so that the server can evaluate the request
        pthread_mutex_unlock(&request_condition_mutex[my_data->id]);
    }
    pthread_exit(NULL);
    return NULL;
}

int main(int argc , char** argv) {
    int NUM_THREADS=stoi(argv[1]);
    pthread_t client_thread[NUM_THREADS],server_thread[NUM_THREADS]; //client and server threads
    struct thread_data td[NUM_THREADS]; //send thread id and number of threads data to each thread
    memset( done , 0 , sizeof(done) ); //no client is done initially
    memset( seats , -1 , sizeof(seats) ); //no seats are taken initially
    
    //initialize contiditonal locks and conditions
    for( int i=0 ; i<NUM_THREADS ; i++ ){
        request_condition[i] = PTHREAD_COND_INITIALIZER;
        request_condition_mutex[i] = PTHREAD_MUTEX_INITIALIZER;
    }
    
    //initialize seat and result mutexes
    seat_mutex = PTHREAD_MUTEX_INITIALIZER;
    result_mutex = PTHREAD_MUTEX_INITIALIZER;
    
    //open output file
    FILE* fp;
    fp = fopen("output.txt","w");
    fprintf(fp, "Number of total seats: %d\n",NUM_THREADS);
    
    //create server and client threads
    for( int i=0 ; i<NUM_THREADS ; i++ ){
        td[i].id=i;
        td[i].number_of_seats=NUM_THREADS;
        pthread_create(&client_thread[i], NULL, client, (void *)&td[i]);
        pthread_create(&server_thread[i], NULL, server, (void *)&td[i]);
    }
    
    //wait for every client to have a seat
    for( int i=0 ; i<NUM_THREADS ; i++ ){
        pthread_join(client_thread[i],NULL);
        pthread_join(server_thread[i],NULL);
    }
    
    //output the client-seat pairs
    for( int i=0 ; i<(int)res.size() ; i++ )
        fprintf(fp, "Client%d reserves Seat%d\n",res[i].first,res[i].second);

    //final notice and close the file
    fprintf(fp, "All seats are reserved.\n");
    fclose(fp);
    
    pthread_exit(NULL);
    return 0;
}
