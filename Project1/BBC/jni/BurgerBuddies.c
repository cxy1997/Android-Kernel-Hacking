#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <semaphore.h>
pthread_mutex_t cntMutex, checkRack, queueMutex, CK, CA, CM;
sem_t *cashierAvailable;
pthread_t *Cooks, *Cashiers, *Customers;
sem_t finish, rackFull, rackEmpty, cashiersAllBusy;
int rack = 0, customersServed = 0, cooks, cashiers, customers, racksize, head = 0, tail = -1, waitForCashier = 0, waitForBurger = 0;
int *sleepingQueue;
struct sigaction actions;
void thread_exit_handler(int sig)
{ 
    pthread_exit(0);
}
int queueSize()
{
    return ((tail + 1 - head + cashiers + 1) % (cashiers + 1));
}
void enqueue(int id)
{
    //pthread_mutex_lock(&queueMutex);
    tail = (tail + 1) % (cashiers + 1);
    sleepingQueue[tail] = id;
    //pthread_mutex_unlock(&queueMutex);
}
int dequeue()
{
	int res = 0;
    //pthread_mutex_lock(&queueMutex);
    if (head != (tail + 1) % (cashiers + 1))
    {
    	res = sleepingQueue[head];
    	head = (head + 1) % (cashiers + 1);
    }
    //pthread_mutex_unlock(&queueMutex);
    return res;
}
void *cook(void *param)
{
	int id = *(int*)param; // get ID
    
    do
    {
    	pthread_mutex_lock(&CK); // only one cook can access the rack at one time
    	pthread_mutex_lock(&checkRack); // only one person can modify the rack at one time
        
        if (rack == racksize) // if the rack is full
        {
        	pthread_mutex_unlock(&checkRack); // make the rack available to cashiers but not other cooks
        	sem_wait(&rackFull); // wait until a cashier takes a burger away
            pthread_mutex_lock(&checkRack);
        }

        ++rack; // add a burger to the rack
        //printf("RackSize = %d, Rack = %d, ", racksize, rack);
	    printf("Cook [%d] makes a burger.\n", id);
        if (rack == 1 && waitForBurger)
        {
        	--waitForBurger;
        	//printf("There is a burger now!\n");
        	sem_post(&rackEmpty);
        }
        pthread_mutex_unlock(&checkRack); // release the rack

        pthread_mutex_unlock(&CK);
    	sleep(1); // crafting a burger
    } while (1); // infinite loop
	pthread_exit(0); // will not be executed
}
void *cashier(void *param)
{
	int id = *(int*)param; // get ID

    do
    {
    	pthread_mutex_lock(&queueMutex);
        enqueue(id); // add himself to the sleeping(available) queue
        //printf("Cashier [%d] is available, now there are %d cashiers in queue.\n", id, queueSize());
        if (queueSize() == 1 && waitForCashier) // if there are customers waiting for vacant cashier
        {
        	--waitForCashier;
        	//printf("The queue is no longer empty!\n");
        	sem_post(&cashiersAllBusy); // tell him that he can go find a cashier and get served now
        }
        pthread_mutex_unlock(&queueMutex);

    	sem_wait(&cashierAvailable[id-1]); // get occupied
    	//sleep(1); // wake up, stretching oneself
	    printf("Cashier [%d] accepts an order.\n", id);

        pthread_mutex_lock(&CA); // only one cashier can access the rack at one time
	    pthread_mutex_lock(&checkRack);

        if (rack == 0) // no burgers
        {
	        ++waitForBurger;
	        pthread_mutex_unlock(&checkRack); // let a cook access the rack
	        //printf("Cashier [%d] is waiting for burger.\n", id);
	        sem_wait(&rackEmpty); // will be signaled by a cook if a burger is crafted
	        pthread_mutex_lock(&checkRack);
        }

	    --rack; // fetch a burger
        //printf("RackSize = %d, Rack = %d, ", racksize, rack);

	    if (rack + 1 == racksize) sem_post(&rackFull);
	    pthread_mutex_unlock(&checkRack);
        printf("Cashier [%d] takes a burger to customer.\n", id);
	    pthread_mutex_unlock(&CA);	

        pthread_mutex_lock(&cntMutex);
        ++customersServed; // count the number of customers served
        //printf("%d customers have been served, cashier [%d] sleeps.\n", customersServed, id);
        if (customersServed == customers) sem_post(&finish); // tell the manager to close the shop
        pthread_mutex_unlock(&cntMutex);
    } while (1);
	pthread_exit(0); // will not be executed
}
void *customer(void *param)
{
	int id = *(int*)param;
    sleep(1);

	pthread_mutex_lock(&CM); // only one customer can access the queue at one time

    pthread_mutex_lock(&queueMutex);
    int tmp = queueSize();
    //printf("Customer [%d] checks queue, there are %d cashiers in queue.\n", id, queueSize());

    if (tmp == 0) // there are no cashiers available
    {
    	++waitForCashier; // increase the counter
        pthread_mutex_unlock(&queueMutex);
        //printf("Customer [%d] is waiting for cashier.\n", id);
    	sem_wait(&cashiersAllBusy); // will be signaled if a cashier has finished his job
        pthread_mutex_lock(&queueMutex);
    }
	printf("Customer [%d] comes.\n", id);

    int serverID = dequeue(); // get a cashier
    pthread_mutex_unlock(&queueMutex);
    sem_post(&cashierAvailable[serverID-1]); // wake the cashier up

	pthread_mutex_unlock(&CM);

    //printf("Customer [%d] is served by cashier [%d].\n", id, serverID);
    
    //printf("Customer [%d] gets a burger.\n", id); //

    //printf("Customer [%d] leaves.\n", id);
	pthread_exit(0); // can be interpreted as waiting for the burger and doing nothing
}
void *manager(void *param)
{
	sem_wait(&finish); // all the customers have been served
    int i;
    for (i = 0; i < cooks; ++i) pthread_kill(Cooks[i], SIGUSR1); // dismiss the cooks
    for (i = 0; i < cashiers; ++i) pthread_kill(Cashiers[i], SIGUSR1); // dismiss the cashiers
    pthread_exit(0);
}
int main(int argc,char ** argv)
{	
    // Here we use function pthread_kill to shut a thread down because pthread_cancel is not supported in Android.
    memset(&actions, 0, sizeof(actions)); 
    sigemptyset(&actions.sa_mask);
    actions.sa_flags = 0; 
    actions.sa_handler = thread_exit_handler;
    int rc = sigaction(SIGUSR1,&actions,NULL);

    int i, id;
    if (argc != 5) // exception handling
    {
    	printf("Unmatched parameters!\n");
    	printf("The correct format should be:\n");
    	printf("BBC #Cooks #Cashiers #Customers #RackSize\n");
    	return 0;
    }

    // string converted to integer
    cooks = atoi(argv[1]);
    cashiers = atoi(argv[2]);
    customers = atoi(argv[3]);
    racksize = atoi(argv[4]);
    sleepingQueue = (int *)malloc((cashiers + 1) * sizeof(int));
    printf("Cooks [%d], Cashiers [%d], Customers [%d]\n", cooks, cashiers, customers);
    printf("Begin run.\n");
    // used to convey value of ID's
    int max = cooks;
    if (max < cashiers) max = cashiers;
    if (max < customers) max = customers;
    int *numbers = (int *)malloc((max + 1) * sizeof(int));
    for (i = 0; i <= max; ++i) numbers[i] = i;
    
    // mutexes & semaphores initialzation
    pthread_mutex_init(&cntMutex, NULL);
    sem_init(&finish, 0, 0);
    pthread_mutex_init(&queueMutex, NULL);
    pthread_mutex_init(&checkRack, NULL);
    sem_init(&rackEmpty, 0, 0);
    sem_init(&rackFull, 0, 0);
    sem_init(&cashiersAllBusy, 0, 0);
    pthread_mutex_init(&CK, NULL);
    pthread_mutex_init(&CA, NULL);
    pthread_mutex_init(&CM, NULL);
    cashierAvailable = (sem_t *)malloc(cashiers * sizeof(sem_t));
    for (i = 0; i < cashiers; ++i)
        sem_init(&cashierAvailable[i], 0, 0);

    // cook threads
    Cooks = (pthread_t *)malloc(cooks * sizeof(pthread_t));
    for (i = 0; i < cooks; ++i)
    {
    	pthread_create(&Cooks[i], NULL, cook, (void *)&numbers[i+1]);
    }

    // cashier threads
    Cashiers = (pthread_t *)malloc(cashiers * sizeof(pthread_t));
    for (i = 0; i < cashiers; ++i)
    {
    	pthread_create(&Cashiers[i], NULL, cashier, (void *)&numbers[i+1]);
    }

    // customer threads
    Customers = (pthread_t *)malloc(customers * sizeof(pthread_t));
    for (i = 0; i < customers; ++i)
    {
    	pthread_create(&Customers[i], NULL, customer, (void *)&numbers[i+1]);
    }

    // manager thread
    pthread_t Manager;
    pthread_create(&Manager, NULL, manager, NULL);
    pthread_join(Manager, NULL);

    // memory releasing
    free(numbers);
    free(Cooks);
    free(Cashiers);
    free(Customers);
    for (i = 0; i < cashiers; ++i)
        sem_destroy(&cashierAvailable[i]);
    free(cashierAvailable);
    pthread_mutex_destroy(&cntMutex);
    pthread_mutex_destroy(&checkRack);
    sem_destroy(&rackFull);
    sem_destroy(&rackEmpty);
    sem_destroy(&cashiersAllBusy);
    pthread_mutex_destroy(&queueMutex);
    sem_destroy(&finish);
    pthread_mutex_destroy(&CK);
    pthread_mutex_destroy(&CA);
    pthread_mutex_destroy(&CM);
}