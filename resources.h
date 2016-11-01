/*
 * Brief: Header file with shared information
 * Autors: Catarina Ramos, Mário Fernandes;
 * Class: 5
 * Group: 14
 */

#ifndef SOPE_PROJ2_RESOURCES_H
#define SOPE_PROJ2_RESOURCES_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <limits.h>
#include <time.h>
#include <sys/wait.h>
#include <pthread.h>
#include <semaphore.h>

/*
 * GENERIC ERROR MESSAGE
 */
#define error(message) \
    do { \
            perror(message); \
            exit(EXIT_FAILURE);\
    } while (0)

/*
 * CAR INFO (GERADOR.LOG)
 */
struct generator_info{
    clock_t time;
    int id_car;
    char destination;
    clock_t parking_time;
    clock_t duration;
    char state[16];     //e - enter / entrada , x - exit / saida , f - full / cheio , c - closed / encerrado
};

/*
 * PARK STATUS (PARQUE.LOG)
 */
struct park_info{
    clock_t time;
    int space;
    int id_car;
    char * state;     //entrada , saida , cheio , encerrado
};

/*
 * THREAD TIMER
 * Recieves the argument in clock_t and makes the count down.
 */
void * timer(void * argc){
    clock_t start_time, current_time;
    clock_t end_time = *(clock_t *) argc;

    if ((start_time = clock()) == -1)
        error("Couldn't start clock\n");

    if ((current_time = clock()) == -1)
        error("Couldn't start clock\n");
    current_time -= start_time;

    while(current_time < end_time) {
        if ((current_time = clock()) == -1)
            error("Couldn't start clock\n");
        current_time -= start_time;
    }

    return NULL;
};



#endif //SOPE_PROJ2_RESOURCES_H
