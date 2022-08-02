/*
Course: CS 518
Team Members:
    * Abhinay Reddy Vongur (av730)
    * Anmol Sharma (as3593)
Machine used:
    * ilabu3 (ilabu3.cs.rutgers.edu) 
*/

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

pthread_t t1, t2;
int x = 0;
int loop = 10000;

/* Counter Incrementer function:
 * This is the function that each thread will run which
 * increments the shared counter x by LOOP times.
 *
 * Your job is to implement the incrementing of x
 * such that is sychronized among threads
 *
 */
void *inc_shared_counter(void *arg) {

    int i;
    //looping into the thread and incrementing the x value accordingly.
    for(i = 0; i < loop; i++){


        /* Implement Code Here */
        //incrementing the global variable by 1 in every iteration
        x=x+1;



        printf("x is incremented to %d\n", x);
    }

    return NULL;
}


/* Main function:
 * This is the main function that will run.
 *
 * Your job is two create two threads and have them
 * run the inc_shared_counter function().
 */
int main(int argc, char *argv[]) {

    if(argc != 2){
        printf("Bad Usage: Must pass in a integer\n");
        exit(1);
    }
    
    loop = atoi(argv[1]);

    printf("Going to run two threads to increment x up to %d\n", 2 * loop);
    // creation of 2 threads and saving their ids to t1 and t2
    pthread_create(&t1,NULL,&inc_shared_counter,NULL);
    pthread_create(&t2,NULL,&inc_shared_counter,NULL);
    // joining 2 worker thread so that main thread may not terminate before the worker threads.
    pthread_join(t1,NULL);
    pthread_join(t2,NULL);


    /* Implement Code Here */



    printf("The final value of x is %d\n", x);

    return 0;
}

