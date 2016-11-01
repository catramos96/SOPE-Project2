/*
 * Brief: Source file of the main program 'parque'
 * Autors: Catarina Ramos, Mário Fernandes;
 * Class: 5
 * Group: 14
 */


#include "resources.h"


static int closing_park;
static int n_spaces_occupied;
static int n_spaces_total;
static clock_t  opening_time;   //since the opening of the park
static int file_parque;         //file descriptor of parque.log
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;     //sync of entrance/exit and number of spaces_ocupied
/*
 *To know if the controllers fifo was opened (in case the generator wasn't executed
 * the fifo controller blocks until the fifo is opened elsewhere)
 */
static int fifoN_opened = 0;
static int fifoS_opened = 0;
static int fifoE_opened = 0;
static int fifoW_opened = 0;


/*
 * THREAD DETACHED ATTENDANT (ARRUMADOR)
 * Attends a car entrance until he leaves the park.
 */

void * attendant(void *argc){
    struct generator_info * car = (struct generator_info *) argc;
    char info[100];
    int fifo_car;
    char fifo_pathname[16];
    clock_t current_time;
    char state[16]; //c - close , e - enter , x - exit , f - full
    char st_code;


    //CONNECT TO PRIVATE CAR FIFO

    sprintf(fifo_pathname,"fifo%d",car->id_car);
    if((fifo_car = open(fifo_pathname,O_WRONLY)) == -1)        //NON_BLOCK ?
        error("Could not open fifo car\n");


    /*
     * CRITIC SESSION ----------------------------------------
     */
    if(pthread_mutex_lock(&mutex) != 0)
        error("Could not lock mutex\n");


    //CHECK PARK CONDITION

    if(n_spaces_total-n_spaces_occupied > 0 && !closing_park){
        st_code = 'e';
        sprintf(state,"estacionamento");
        n_spaces_occupied++;
    }
    else if(closing_park){
        st_code = 'c';
        sprintf(state,"encerrado");
    }
    else if (n_spaces_total-n_spaces_occupied <= 0 && !closing_park){
        st_code = 'f';
        sprintf(state,"cheio!");
    }


    //TIME OF REQUEST RECEIVED (ENTER)

    if ((current_time = clock()) == -1)
        error("Couldn't start clock\n");
    current_time -= opening_time;


    //WRITE PARK STATUS

    sprintf(info," %16d ; %13d ; %15d ; %15s \n",(int)current_time,n_spaces_occupied,car->id_car,state);
    write(file_parque,info,strlen(info));


    if(pthread_mutex_unlock(&mutex) != 0)
        error("Could not unlock mutex\n");
    /*
     * END CRITIC SESSION ----------------------------------------
     */


    //INFORM FIFO CAR ABOUT PARK CONDITION

    if(write(fifo_car,&st_code,sizeof(st_code)) == -1)
        error("Could not write on fifo car\n");


    //PARK FULL OR CLOSED

    if(st_code == 'f' || st_code == 'c'){
        close(fifo_car);
        free(car);
        return NULL;
    }


    //PARKING TIME (TIMER)

    pthread_t t;
    pthread_create(&t,NULL,timer,&car->parking_time);
    pthread_join(t,NULL);


    // TIME OF LEAVING

    if ((current_time = clock()) == -1)
        error("Couldn't start clock\n");
    current_time -= opening_time;


    // INFORM FIFO CAR TO LEAVE

    st_code = 'x';  //exit - x
    if(write(fifo_car,&st_code,sizeof(st_code)) == -1)
        error("Could not write on fifo car\n");


    /*
     * CRITIC SESSION ----------------------------------------
     */
    if(pthread_mutex_lock(&mutex) != 0)
        error("Could not lock mutex\n");


    n_spaces_occupied--;


    //WRITE PARK STATUS

    sprintf(info," %16d ; %13d ; %15d ; %15s \n",(int)current_time,n_spaces_occupied,car->id_car,"saida");
    if(write(file_parque,info,strlen(info)) == -1)
        error("Could not write on parque.log\n");


    if(pthread_mutex_unlock(&mutex) != 0)
        error("Could not unlock mutex\n");
    /*
     * END CRITIC SESSION ----------------------------------------
     */


    close(fifo_car);
    free(car);
    return NULL;
}

//------------------------------------------------------------------------------------------

/*
 * THREAD CONTROLLER (controlador)
 * Controls the entrance and exit of cars to the park.
 * When the park closes, it doesn't answer for more requests.
 */

void * controller(void * argc){
    char entrance = *(char *) argc;
    char fifo_pathname[16];
    int fifo;
    int err;
    struct generator_info car_info;
    pthread_t attendant_thread;
    pthread_attr_t attr;
    int n_read;

    //FOR DETACHED THREAD
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    /*
     * FIFO CONTROLER (GENERIC)
     */

    sprintf(fifo_pathname,"fifo%c",entrance);
    if((fifo = open(fifo_pathname,O_RDONLY | O_CREAT,0666)) == -1)
        error("Could not open fifo\n");

	if(strcmp(fifo_pathname,"fifoN") == 0)
		fifoN_opened = 1;
	if(strcmp(fifo_pathname,"fifoS") == 0)
		fifoS_opened = 1;
	if(strcmp(fifo_pathname,"fifoE") == 0)
		fifoE_opened = 1;
	if(strcmp(fifo_pathname,"fifoW") == 0)
		fifoW_opened = 1;

    /*
     * RECEIVE CARS REQUESTS
     */
    while(closing_park == 0){

        if((n_read = read(fifo,&car_info,sizeof(struct generator_info))) == -1)
            error("Could not read from fifo controller\n");

        else if(n_read > 0){

            struct generator_info * car = malloc(sizeof(struct generator_info));
            *car =  car_info;
            if((err = pthread_create(&attendant_thread, &attr,attendant,car)) != 0)
                error("Pthread_create erro for north_thread");

        }
    }

	close(fifo);
    unlink(fifo_pathname);
    pthread_exit(NULL);
}

//------------------------------------------------------------------------------------------

/*
 * MAIN THREAD PARQUE
 */

int main(int argc , char *argv[]) {
    double time;
    pthread_t north_thread, south_thread , west_thread , east_thread;
    clock_t start_time, end_time;
    closing_park = 0;
    n_spaces_occupied = 0;
    char info[256];

    //Checks the number of arguments
    if(argc != 3)
        error("Wrong number of arguments\n");

    //Checks if the arguments are valid
    if((n_spaces_total = atoi(argv[1])) == 0 || (time = atoi(argv[2])) == 0)
        error("Invalid Arguments, must be an integer!\n");


    /*
     * PARQUE.LOG
     */

    if ((file_parque = open("parque.log", O_WRONLY | O_CREAT | O_TRUNC, 0644)) == -1) {
        error("Couldn't open parque.log.\n");
    }

    sprintf(info,"     t(ticks)     ;     n_lug     ;     id_viat     ;      observ     \n");
    write(file_parque,info,strlen(info));

    /*
     * SEMAPHORE (PARQUE<=>GERADOR)
     */

    sem_t * semaphore = malloc(sizeof(sem_t));

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
     * FIFO CONTROLLERS
     */

    if(mkfifo("fifoN",0666) == -1)
        error("fifo controller could not be created!\n");

    if(mkfifo("fifoS",0666) == -1)
        error("fifo controller could not be created!\n");

    if(mkfifo("fifoE",0666) == -1)
        error("fifo controller could not be created!\n");

    if(mkfifo("fifoW",0666) == -1)
        error("fifo controller could not be created!\n");

    /*
     * THREADS CONTROLLER
     */

    if(pthread_create(&north_thread, NULL,controller, "N") != 0)
        error("Pthread_create erro for north_thread");

    if(pthread_create(&south_thread, NULL,controller, "S") != 0)
        error("Pthread_create erro for south_thread");

    if(pthread_create(&west_thread, NULL,controller, "W") != 0)
        error("Pthread_create erro for west_thread");

    if(pthread_create(&east_thread, NULL,controller, "E") != 0)
        error("Pthread_create erro for east_thread");

    /*
     * CLOCKS
     */

    end_time = time*CLOCKS_PER_SEC;

    if ((start_time = clock()) == -1)                  //Start clock
        error("Couldn't start clock\n");
    opening_time = start_time;

    /*
     * TIMER PARK OPEN
     */

    pthread_t t;
    pthread_create(&t,NULL,timer,&end_time);
    pthread_join(t,NULL);


    /*
     * CRITIC SESSION ---------------------------------------
     */
    if(sem_wait(semaphore) == -1)
        error("Wait for semaphore error\n");


    closing_park = 1;


    if(sem_post(semaphore) == -1)
        error("Post semaphore error\n");
    /*
     * END CRITIC SESSION ---------------------------------------
     */


    /*
     * IF the controllers fifo weren't opened in generator then the program will wait for it's opening
     * This way we can controll if the fifos were opened in both sides and skip in case they just opened in park
     */


	if(fifoN_opened == 0){
		pthread_cancel(north_thread);
		unlink("fifoN");
	}
	else
		 pthread_join(north_thread,NULL);

	if(fifoS_opened == 0){
		pthread_cancel(south_thread);
		unlink("fifoS");
	}
	else
		pthread_join(south_thread,NULL);

	if(fifoE_opened == 0){
		pthread_cancel(east_thread);
		unlink("fifoE");
	}
	else
		pthread_join(east_thread,NULL);

	if(fifoW_opened == 0){
		pthread_cancel(west_thread);
		unlink("fifoW");
	}
	else
		pthread_join(west_thread,NULL);

    sem_unlink("semaphore");
    pthread_exit(0);
}

