#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/types.h>
#include <string.h>
#include <getopt.h>
#include <time.h>

void *barber_function(void *idp);
void *client_function(void *idp);
void *create_client();
void cut();

pthread_cond_t wake_up_barber = PTHREAD_COND_INITIALIZER;
pthread_cond_t wake_up_client = PTHREAD_COND_INITIALIZER;

pthread_mutex_t cutting = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t barber_state = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t waiting_room = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t client_state = PTHREAD_MUTEX_INITIALIZER;


int total_chairs;
int total_clients;
int free_chairs;
int client_id = 0;
int actual_serve_id = 0;
int actual_number_of_clients = 0;
int not_service_clients = 0;
int info =0;
int min_cut;
int max_cut;
int min_come;
int max_come;


typedef struct simple_list {
    int id;
    pthread_t tid;
    struct simple_list *next;
} queue;


queue *barber_queue = NULL;
queue *not_cut_clients_queue = NULL;
queue *cut_clients_queue = NULL;



queue *add_client_to_queue(queue **first, int id, pthread_t tid) 
{
    queue *new = malloc(sizeof(queue));
    new->id = id;
    new->tid = tid;
    new->next = NULL;

    if (*first == NULL) 
    {
        (*first) = new;
    } 
    else 
    {
        queue *temp = *first;
        while (temp->next != NULL) 
        {
            temp = temp->next;
        }
        temp->next = new;
    }
    return new;
}



queue *serve_client_from_queue() 
{
    queue *tmp = barber_queue;
    if(barber_queue != NULL)
    {
    	barber_queue = barber_queue->next;
    }
    else
    {	
    	exit(1);
    }
    actual_serve_id = tmp->id;
    return tmp;
}

void print_queue(queue *first)
{
    queue *temp=first;
    while(temp!=NULL)
    {
        printf("%d, ", temp->id);
        printf("%ld \n", temp->tid);
        temp = temp->next;
    }
}



void time_to_next_client() 
{
    int time = rand() % (max_come - min_come) + min_come;
    time = time * 1000;
    usleep(time);
}

void cut() 
{
 //   pthread_mutex_lock(&cutting);
    int time = rand() % (max_cut - min_cut) + min_cut;
    time = time * 1000;
    usleep(time);
  //  pthread_mutex_unlock(&cutting);
}

void  barber_break()
{
   //  pthread_mutex_lock(&cutting);
    int time = rand() % (100) + 10;
    time = time * 1000;
    usleep(time);
   // pthread_mutex_unlock(&cutting);
}

void *barber_function(void *idp) 
{
    int i = 0;
    pthread_mutex_lock(&barber_state);
    while (1) 
    {
        
    	while (actual_number_of_clients == 0)
        {
		// Fryzjer zasypia i czeka na wybudzenie go przez clienta
		pthread_cond_wait(&wake_up_barber, &barber_state);
		pthread_mutex_unlock(&barber_state);
	    }
   
        // Blokujemy poczekalnie zeby zmienc stan dostepnych krzesel i ilosc klientow
        pthread_mutex_lock(&waiting_room);
        free_chairs++;
        actual_number_of_clients--;
        pthread_mutex_unlock(&waiting_room);
        pthread_t tidb=barber_queue->tid;
        serve_client_from_queue();
        printf("Klient ID = %d jest obslugiwany. [tid=%ld] \n", actual_serve_id,tidb);
        printf("Zrezygnowało: %d W Poczekalni: %d/%d [Klient na fotelu %d]\n", not_service_clients, actual_number_of_clients, total_chairs, actual_serve_id);
        cut();
        pthread_cond_signal(&wake_up_client);
        barber_break();
        
        
        i++;
        if (i == (total_clients - not_service_clients)) 
        {
          
                printf("\n\nBrak klientow! \n");
                break;
            
        }
    }
    pthread_exit(NULL);
}

// Funkcja opisujaca zachowanie nowo powstalego watku clienta
void *client_function(void *idp) 
{
    int x=0;
    pthread_mutex_lock(&waiting_room);
    client_id++;
    x=client_id;
    printf("Nowy klient ID = %d, [tid = %lu] \n", client_id, pthread_self());
        if (free_chairs >= 1) 
        {
            actual_number_of_clients++;
            free_chairs--;
            add_client_to_queue(&barber_queue, client_id, pthread_self());
            add_client_to_queue(&cut_clients_queue, client_id, pthread_self() );
            if (info) 
            {
            printf("Kolejka osob oczekajacych zwiekszyla sie\n");
            print_queue(barber_queue);
            }

            printf("Zrezygnowalo: %d Poczekalnia: %d/%d [Klient na fotelu: %d]\n", not_service_clients, actual_number_of_clients, total_chairs, actual_serve_id);
            pthread_mutex_unlock(&waiting_room);

            // Budzimy fryjera
            while(1)
            {
                if(barber_queue->tid == pthread_self())
                {
                pthread_mutex_lock(&client_state);
                pthread_cond_signal(&wake_up_barber);
                pthread_cond_wait(&wake_up_client,  &client_state);
                printf("Klient ID = %d został obsłużony [tid=%ld] \n",x, pthread_self());
                pthread_mutex_unlock(&client_state);
                break;
                }
            }
            
        } else 
        {
            add_client_to_queue(&not_cut_clients_queue, client_id, pthread_self());
            not_service_clients++;
            if (info) 
            {
            printf("Kolejka zrezygnowanych zwiekszyla sie\n");
            print_queue(not_cut_clients_queue);
            }
            printf("Klient ID=%d wyszedl!(zostal dodany do listy zrezygnowanych) [tid=%ld]\n", client_id,pthread_self());
            printf("Zrezygnowali: %d W Poczekalni: %d/%d [Na krześle: %d] \n", not_service_clients, actual_number_of_clients, total_chairs, actual_serve_id);
            // odblokuwuje dostep do poczekalni
            pthread_mutex_unlock(&waiting_room);
        }
    
    pthread_exit(NULL);
}


/* Tworzymy watki clientow, tyle ile podalismy podczas uruchomienia programu */
void *create_client() 
{
    int temporary;
    int i = 0;

    while (i < total_clients) 
    {
        pthread_t client;
        temporary = pthread_create(&client, NULL, (void *) client_function, NULL);
        if (temporary) 
        {
            printf("Blad tworzenia watku pracownika");
            exit(1);
        }
        i++;
        time_to_next_client();
    }
    return 0;   
}




int main(int argc, char *argv[]) 
{
    srand( (unsigned int)time(NULL) );

     int opt;
      // początkowa wartość zmiennej info

    // Definicja dostępnych opcji
    struct option long_options[] = 
    {
        {"info", no_argument, NULL, 'i'},
        {NULL, 0, NULL, 0}
    };

    while ((opt = getopt_long(argc, argv, "i", long_options, NULL)) != -1) 
    {
        switch (opt) 
        {
            case 'i':
                info = 1;
                break;
            default:
                break;
        }
    }

    // Przykładowe działanie w przypadku użycia opcji --info/-i
    if (info) 
    {
        printf("Opcja --info/-i została użyta.\n");
    }


    int temporary;

    printf("Podaj ilosc krzesel w poczekalni: \n");
    scanf("%d", &total_chairs);

    printf("Podaj calkowita ilosc klientow: \n");
    scanf("%d", &total_clients);
    printf("Ilosc krzesel = %d,  Ilosc klientow = %d\n\n", total_chairs, total_clients);
    printf("Podaj maksymalny czas strzyzenia\n");
    scanf("%d", &max_cut);

    printf("Podaj minimalny czas strzyzenia \n");
    scanf("%d", &min_cut);

    printf("Podaj maksymalny czas przyjscia kolejnego klienta\n");
    scanf("%d", &max_come);

    printf("Podaj minimalny czas przyjscia kolejnego klienta \n");
    scanf("%d", &min_come);
    

    free_chairs = total_chairs;

    /* Tworzymy dwa watki - fryzjera oraz watek opowiedzialny za dodawanie klientow */
    pthread_t client_thread;
    pthread_t barber_thread;

    /* Tworzenie watku 'barbera' */
    temporary = pthread_create(&barber_thread, 0, barber_function, 0);
    if (temporary != 0) {
        printf("Blad tworzenia watku fryzjera!\n");
        exit(1);
    } else {
        printf("Pomyslnie utworzono watek fryzjera!\n");
    }


    /* Tworzenie watku  */
    temporary = pthread_create(&client_thread, NULL, (void *) create_client, NULL);
    if (temporary != 0) 
    {
        printf("Blad tworzenia watku klienta!\n");
        exit(1);
    } else {
        printf("Pomyslnie utworzono watek klienta!\n");
    }

    pthread_join(barber_thread, NULL);
    pthread_join(client_thread, NULL);

    printf("\nIlosc klientow ktora musiala opuscic lokal: %d\n", not_service_clients);
    printf("Ci klienci nie zostali obsłużeni: \n");
    print_queue(not_cut_clients_queue);
    printf("Ci klienci zostali obsłużeni \n");
    print_queue(cut_clients_queue);
    /* Zwalniamy pamiec */
    pthread_cond_destroy(&wake_up_barber);
    pthread_mutex_destroy(&cutting);
    pthread_mutex_destroy(&barber_state);
    pthread_mutex_destroy(&waiting_room);
    return 0;
}
