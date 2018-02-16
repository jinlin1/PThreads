/**
 *  @Author Jin Hui Lin (hj73293@umbc.edu)
 *  This file creates a child threads, and neighborhood thread  
 *  Children thread trick-or-treats and collects candy
 *  Neighborhood thread refills candy in houses
 * 
 *  Sources: 
 *  http://www.yolinux.com/TUTORIALS/LinuxTutorialPosixThreads.html
 *  https://stackoverflow.com/questions/26753957/how-to-dynamically-allocateinitialize-a-pthread-array
 *  https://stackoverflow.com/questions/7961029/how-can-i-kill-a-pthread-that-is-in-an-infinite-loop-from-outside-that-loop
 */

#define _BSD_SOURCE
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define HOUSES 10

/**
 *  Struct House
 *  Contains information on the house
 */
struct House {

  unsigned int cordX;
  unsigned int cordY;
  unsigned int candy;
  pthread_mutex_t houseLock;
};

/**
 *  Struct Children
 *  Contains information on group trick or treating
 * 
 */
struct Children {
  unsigned int uniqueId;
  unsigned int startHouse;
  unsigned int size;
  unsigned int time;
  unsigned int currentHouse;
  unsigned int destinationHouse;
  unsigned int candy;
};

struct House Houses[HOUSES];

struct Children *children;

pthread_mutex_t lock;

/**
 * quit() - Checks if the lock is locked or freed
 * @lock: the mutex lock
 * Return: true if lock is freed and false if lock is acquired
 */
int quit(pthread_mutex_t *lock) {
  switch(pthread_mutex_trylock(lock)) {  
    case 0:
      pthread_mutex_unlock(lock);
      return 1;
    case EBUSY:
      return 0;
  }
  return 1;
}

/**
 * distance() - Calculates the distance given 2D coordinates
 * @x1: x coordinate of first point
 * @x2: x coordinate of second point
 * @y1: y coordinate of first point
 * @y2: y coordinate of second point
 * Return: distance between the two points 
 */
unsigned int distance(unsigned int x1, unsigned int x2, unsigned int y1, unsigned int y2) {

  return abs(x1 - x2) + abs(y1 - y2);

}

/**
 * getTreat() - Child thread to collect treats from houses
 * @argv: Children struct with group information 
 * Return: NULL 
 */

void *getTreat( void *argv) {

  struct Children *children = argv;

  //Initialize the children candy to 0
  //Initialize the current house to the start house
  children->currentHouse = children->startHouse; 
  children->candy = 0; 

  // Keep running until the lock is freed
  while(!quit(&lock)) {
    unsigned int minimumDistance = 20;                     //minimum distance between children and house
    unsigned int chosenHouse = children->startHouse;       //the next house to trick or treat 
    unsigned int tmpDistance;                              
    unsigned int tmpCandy;
    for(int i = 0; i < HOUSES; i++) {
      // Make sure the children doesn't visit the original house and the current house
      if(children->currentHouse != i && children->startHouse != i) {
        tmpDistance = distance(Houses[children->currentHouse].cordX, Houses[i].cordX, Houses[children->currentHouse].cordY, Houses[i].cordY);
        tmpCandy = Houses[i].candy;
        // Go to the house with enough candy and shortest distance 
        if(tmpDistance < minimumDistance && children->size <= tmpCandy) {
          chosenHouse = i;
          minimumDistance = tmpDistance; 
        }
      } 
    }
    // If the above algorithm failed to find a house
    // Pick the house with the shortest distance
    if(chosenHouse == children->startHouse) {
      minimumDistance = 20;
      for(int i = 0; i < HOUSES; i++) {
        // Make sure the children doesn't visit the original house and the current house
        if(children->currentHouse != i && children->startHouse != i) {
          tmpDistance = distance(Houses[children->currentHouse].cordX, Houses[i].cordX, Houses[children->currentHouse].cordY, Houses[i].cordY);
          if(tmpDistance <= minimumDistance) {
            chosenHouse = i;
            minimumDistance = tmpDistance; 
          }
        } 
      }
    }
    children->destinationHouse = chosenHouse;
    children->time = minimumDistance * 250;
    printf("Group: %u from house: %u to house: %u (travel time = %u ms )\n", children->uniqueId, children->currentHouse, children->destinationHouse, children->time); 
    // Sleep the children 
    usleep(children->time * 1000);
    // Acquire house lock to take candy from a house
    pthread_mutex_lock(&Houses[children->destinationHouse].houseLock);  
    // Take all the candy from the house if the size of the group is too big 
    if(children->size > Houses[children->destinationHouse].candy ) {
      children->candy += Houses[children->destinationHouse].candy;
      Houses[children->destinationHouse].candy = 0;
    } 
    else { 
      children->candy += children->size;
      Houses[children->destinationHouse].candy -= children->size;
    }

    pthread_mutex_unlock(&Houses[children->destinationHouse].houseLock); 
    
    children->currentHouse = chosenHouse;
  }
  return NULL;
}

/**
 * refillCandy() - refill candy from a house 
 * @argv: file pointer 
 * Return: NULL 
 */
void *refillCandy( void *argv) {

  FILE* fp = argv;
  char buffer[80];
  unsigned int houseNumber;
  unsigned int refill;
  usleep(250 * 1000);

  // Read the the filepointer and refill a house line is read
  // Terminate thread when all lines are read or simulation ended
  while(!quit(&lock) && fgets(buffer, sizeof(buffer), fp)) {
    sscanf(buffer, "%u %u", &houseNumber, &refill);
    pthread_mutex_lock(&Houses[houseNumber].houseLock);
    printf("Neighborhood: added %u to %u \n", refill, houseNumber);
    Houses[houseNumber].candy += refill;
    pthread_mutex_unlock(&Houses[houseNumber].houseLock);
    usleep(250 * 1000);
  }

  return NULL;

}

/**
 * main() - print the simulation details
 * @argc: number of arguments
 * @argv: arguments
 * Return: 0
 */
int main(int argc, char* argv[]) {
  if(argc == 3) {
    unsigned int numOfChildren;

    FILE* fp;

    // Open file to be read
    fp = fopen(argv[1], "r");
    if(fp == NULL) {
      perror("fopen");
      exit(0);
    }  

    char buffer[80];
    // Get first line in file
    if(fgets(buffer , sizeof(buffer), fp) == NULL) { 
      fprintf(stderr, "Unable to read from file. \n");
      exit(0); 
    }

    // Store first line as the number of children
    if(sscanf(buffer, "%u", &numOfChildren) != 1) {
      fprintf(stderr, "Unable to convert line in line to unsigned int. \n");
      exit(0); 
    }

    // Read the next 10 lines
    // Store the information in an array of houses
    for(int i = 0; i < HOUSES; i++) {
      if(fgets(buffer , sizeof(buffer), fp) == NULL) { 
        fprintf(stderr, "Unable to read from file. \n");
        exit(0); 
      }
      if(sscanf(buffer, "%u %u %u", &Houses[i].cordX, &Houses[i].cordY, &Houses[i].candy) != 3) {
        fprintf(stderr, "Unable to convert line in line to unsigned int. \n");
        exit(0); 
      }
      // Initialize lock for each house
      pthread_mutex_init(&Houses[i].houseLock,NULL);
    }  

    pthread_t *threads; 
    threads = malloc(sizeof(pthread_t)*numOfChildren);

    children = malloc(sizeof(struct Children)*numOfChildren);   

    // Initialize lock for turning off simulation
    pthread_mutex_init(&lock,NULL);
    pthread_mutex_lock(&lock);

    // Read the number of lines specified by the first line
    // Store the children information in an array and create child thread
    for(int i = 0; i < numOfChildren; i++) {
      if(fgets(buffer , sizeof(buffer), fp) == NULL) { 
        fprintf(stderr, "Unable to read from file. \n");
        exit(0); 
      }
       
      if(sscanf(buffer, "%u %u", &children[i].startHouse, &children[i].size) != 2) {
        fprintf(stderr, "Unable to convert line in line to unsigned int. \n");
        exit(0); 
      }
      children[i].uniqueId = i; 
      pthread_create(&threads[i], NULL, getTreat, (void*) &children[i]);
    }

    // Create the neighborhood thread
    pthread_t neighborThread;
    
    pthread_create(&neighborThread, NULL, refillCandy, (void*) fp); 

    int time;
    if(sscanf(argv[2], "%d", &time) != 1) {
        fprintf(stderr, "Unable to convert argument to time. \n");
        exit(0);
    }

    // The length of the simulation
    for( int i = 0; i <= time; i++) {
      unsigned int totalCandy = 0;

      // Don't sleep when time = 0 
      if(i != 0) {
        if(sleep(1) == -1) {
          perror("sleep");
          exit(0);
        }
      }

      // Don't display the simulation after it has ended
      if(i != time) {
        printf("After %d seconds: \n", i);
        printf("  Group statuses: \n");

        for( int j = 0; j < numOfChildren; j++) {
          printf("    %u: size %u, going to %u, collected  %u \n", children[j].uniqueId, children[j].size, children[j].destinationHouse, children[j].candy); 
          totalCandy += children[j].candy; 
        }
        printf("  House statues: \n");

        for( int k = 0; k < HOUSES; k++) {
          printf("    %u @ (%u, %u): %u available \n", k, Houses[k].cordX, Houses[k].cordY, Houses[k].candy);
        }
        printf("  Total candy: %u \n", totalCandy);
      }
    }

    // Unlock the simulation lock 
    pthread_mutex_unlock(&lock);  
    // Wait for the children thread is terminate
    for(int i = 0; i < numOfChildren; i++) {
      pthread_join(threads[i], NULL);
    } 

    // Display the simulation after the simulation has ended
    printf("After %d seconds: \n", time);
    printf("  Group statuses: \n");

    int totalCandy = 0;

    for( int j = 0; j < numOfChildren; j++) {
      printf("    %u: size %u, going to %u, collected  %u \n", children[j].uniqueId, children[j].size, children[j].destinationHouse, children[j].candy); 
      totalCandy += children[j].candy; 
    }
    printf("  House statues: \n");

    for( int k = 0; k < HOUSES; k++) {
      printf("    %u @ (%u, %u): %u available \n", k, Houses[k].cordX, Houses[k].cordY, Houses[k].candy);
    }
    printf("  Total candy: %u \n", totalCandy);

  } 
  else {
    fprintf(stderr, "Invalid number of arugments. \n");
  }

}
