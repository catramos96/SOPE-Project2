/*
 * Brief: Source file of the main program 'gerador'
 * Autors: Catarina Ramos, Mário Fernandes;
 * Class: 5
 * Group: 14
 */

#include "resources.h"

static clock_t function_time;       //function start time of generator
static int file_gerador;            //file descriptor of gerador.log
static sem_t *semaphore;            //to sync with park

/*
 * THREAD DETACHED CAR (VIATURA)
 * This thread communicates with the controller car_info->destinatio fifo and sends a request
 * to enter the park. It waits for the answer via the private fifo of the car. If the access to the park is denied,
 * because it's full or closed, the thread terminates, otherwise, the thread waits for another answer from the controller
 * so it can exit the park.
 * The car_info->duration represents the time since the creation of the struct that represents the car until the
 * exit of the park.
 * After receiving an answer form the controller fifo, is written on gerador.log the car status.
 * The car private fifo name is FIFOID, for example: FIFO16 (where ID = 16);
 */

void * car(void * argc){

struct generator_info * car_info = (struct generator_info *) argc;
    int fifo_dest;          //controller fifo
    char fifo_pathname[36];
    char answer = ' ';      //answer from controller
    int fifo_car;
    int n_read = 0;
    clock_t current_time;
	char info[256];


    /*
     * CRITIC SESSION ------------------------------------ SYNC WITH PARK TO KNOW IF IT IS CLOSED
     */
    if(sem_wait(semaphore) == -1)
        error("Wait for semaphore error\n");


   //ATTEMPT TO CONNECT TO CONTROLLER FIFO

    sprintf(fifo_pathname,"fifo%c",car_info->destination);
    if((fifo_dest = open(fifo_pathname,O_WRONLY)) == -1){

	    if(errno != ENOENT)
		    error("Could not open controller fifo\n");


	    //IF CONSTROLLER FIFO IS CLOSED/DOESN'T EXIST
        //WRITE INFO ON GERADOR.LOG

	    sprintf(info," %16d ; %14d ; %4c ; %14d ; %16s ; %16s\n",(int)car_info->time,(int)car_info->id_car,car_info->destination,(int)car_info->parking_time, "?","encerrado");
    	if(write(file_gerador,info,strlen(info)) == -1)
        	error("Problem on writing on file\n");


        if(sem_post(semaphore) == -1)
            error("Post semaphore error\n");
        /*
        * END CRITIC SESSION ------------------------------------
        */

        free(car_info);
        return NULL;
	 }


    //MAKE PRIVATE FIFO

    sprintf(fifo_pathname,"fifo%d",car_info->id_car);
    if(mkfifo(fifo_pathname,0666) != 0)
        error("fifo_pathname could not be created!\n");


    //SEND REQUEST TO ACCESS PARK (and open the private fifo there)

    if(write(fifo_dest,car_info,sizeof(struct generator_info)) == -1)
        error("Error on writting to fifo_dest\n");


    if(sem_post(semaphore) == -1)
        error("Post semaphore error\n");
    /*
     * END CRITIC SESSION ------------------------------------
     */


	//OPEN PRIVATE FIFO

	if((fifo_car = open(fifo_pathname,O_RDONLY)) == -1)
        error("Could not open private fifo\n");


    //WAIT ANSWER FROM CONTROLLER TO ENTER

    while((n_read = read(fifo_car,&answer,sizeof(char))) == 0){
    }

    switch (answer) {
        case 'e': {
            sprintf(car_info->state, "entrada");
            break;
        }
        case 'f': {
            sprintf(car_info->state, "cheio!");
            break;
        }
        case 'c': {
            sprintf(car_info->state, "encerrado");
            break;
        }
        case 'x': {
            sprintf(car_info->state, "saida");
            break;
        }
        default: {
            error("wrong answer\n");
        }
    }


    //WRITE CAR STATUS

    sprintf(info," %16d ; %14d ; %4c ; %14d ; %16s ; %16s\n",(int)car_info->time,(int)car_info->id_car,car_info->destination,(int)car_info->parking_time, "?",car_info->state);
    if(write(file_gerador,info,strlen(info)) == -1)
        error("Problem on writing on file\n");


    //TERMINATE THREAD IF PARK = CLOSED | FULL

    if(answer == 'f' || answer == 'c'){
        close(fifo_car);
        unlink(fifo_pathname);
        free(car_info);
        return NULL;
    }


   //WAIT FOR EXIT INSTRUCTION FROM CONTROLLER

    n_read = 0;
    while((n_read = read(fifo_car,&answer,sizeof(char))) == 0 || answer != 'x'){	//'x' - exit
    }

	if(n_read == -1)
		error("Error on getting 2 answer from private car fifo\n");


   //CALCULATES CAR DURATION (LIFE TIME)

    if ((current_time = clock()) == -1)
        error("Couldn't start clock\n");

    car_info->duration = current_time - function_time;
    car_info->time = car_info->duration + car_info->time;


    //WRITE CAR STATUS

    sprintf(info," %16d ; %14d ; %4c ; %14d ; %16d ; %16s\n",(int)car_info->time,(int)car_info->id_car,car_info->destination,(int)car_info->parking_time, (int)car_info->duration,"saida");
    if(write(file_gerador,info,strlen(info)) == -1)
        error("Problem on writing on file\n");

    close(fifo_car);
    unlink(fifo_pathname);
    free(car_info);
    return NULL;
}


/*
 * MAIN THREAD GERADOR
 * Thread that generates cars for a certain time. The time between cars generations are 50% 0 ticks, 30% min_time ticks
 * or 20% 2*min_time ticks.
 * The car generator generates random information about the car and creates a thread car where the previous
 * information.
 */

int main(int argc , char *argv[]) {
    int time_generator;
    clock_t min_time;
    clock_t start_time, curr_time, end_time;
    pthread_t new_car;
    pthread_attr_t attr;
    srand(time(NULL));
    int ID = 1;
	pthread_t t;        //timer
    char info[256];

    //Checks the number of arguments
    if (argc != 3)
        error("Wrong number of arguments\n");

    //Checks if the arguments are valid
    if ((time_generator = atoi(argv[1])) == 0 || (min_time = atoi(argv[2])) == 0)
        error("Invalid Arguments, must be an integer!\n");


    /*
     * GERADOR.LOG
     */

    if ((file_gerador = open("gerador.log", O_WRONLY | O_CREAT | O_TRUNC, 0666)) == -1) {
        error("Couldn't open gerador.log on gerador.main\n");
    }

    sprintf(info,"     t(ticks)     ;     id_car     ; dest ;    t_parking   ;     duration     ;      observ     \n");
    if(write(file_gerador,info,strlen(info)) == -1)
        error("Problem on writting on file\n");

    /*
     * SEMAPHORE (PARQUE<=>GERADOR)
     */

    semaphore = malloc(sizeof(sem_t));
    
    if((semaphore = sem_open("semaphore", O_CREAT | O_EXCL, 0666, 1)) == SEM_FAILED){
        if(errno == EEXIST){
            if((semaphore = sem_open("semaphore", 0)) == SEM_FAILED) {
                error("Couldn't create semaphore\n");
            }
        }
        else
            error("Couldn't create semaphore\n");
    }

    /*
     * CLOCKS
     */

    end_time = CLOCKS_PER_SEC * time_generator;           //time_generator (seconds) in clock ticks

    if ((start_time = clock()) == -1)                  //Start clock
        error("Couldn't start clock\n");
    function_time = start_time;

    if ((curr_time = clock()) == -1)      //calculates the current time
        error("Couldn't start clock\n");
    curr_time -= start_time;


    //FOR DETACHED THREADS CAR

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);


    //START GENERATING CARS

    while (curr_time < end_time) {
        struct generator_info *info_car = malloc(sizeof(struct generator_info));


        //RANDOM INFO

        info_car->id_car = ID++;

        int i = rand() % 4;                 //controller (N/S/W/E)
        switch (i) {
            case 0:
                info_car->destination = 'N';
                break;
            case 1:
                info_car->destination = 'S';
                break;
            case 2:
                info_car->destination = 'W';
                break;
            case 3:
                info_car->destination = 'E';
                break;
        }

        info_car->parking_time = (rand() % 10 + 1) * min_time;
        info_car->duration = -1;    //undefined
        sprintf(info_car->state," ");


        //IF TIME HAS ENDED THEN BREAK

        if ((curr_time = clock()) == -1)
            error("Couldn't start clock\n");
        curr_time -= start_time;
        if (curr_time >= end_time)
            break;

        info_car->time = curr_time;


        //CREATE DETACHED THREAD CAR

        if (pthread_create(&new_car, &attr, car, info_car) != 0)
            error("Pthread_create erro for new_car");


        //RANDOM TIME BETWEEN TWO CARS GENERATIONS

	    clock_t * wait_time = malloc(sizeof(clock_t));
        *wait_time = rand() % 10 + 1;

        if (*wait_time <= 5)                         //50% , mult 0x
            *wait_time = 0;
        else if (*wait_time > 5 && *wait_time <= 8)   //30% , mult 1x
            *wait_time = min_time;
        else                                        //20% , mult 2x
            *wait_time = 2 * min_time;


        //TIMER

   	    pthread_create(&t,NULL,timer,wait_time);
   	    pthread_join(t,NULL);
	    free(wait_time);
    }

    sem_unlink("semaphore");
pthread_exit(0);
}

