#include <stdio.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/types.h>
#include <memory.h>
#include <getopt.h>

void *barber_funcion(void *idp);
void *client_function(void *idp);
void *create_client();
void cut();

pthread_mutex_t serve_client;
pthread_mutex_t client_waiting;

sem_t barber_readiness;
sem_t client_readiness;
sem_t chair_access;
sem_t work_done;

int total_chairs;
int total_clients;
int free_chairs;
int client_id = 0;
int actual_serve_id = 0;
int actual_cutting_client = 0;
int actual_number_of_clients = 0;
int not_service_clients = 0;
int cut_clients = 0;
        // Odblokowujemy mute
int min_cut = 0;
int max_cut = 0;
int min_come =0;
int max_come =0;
int info = 0;

typedef struct simple_list 
{
    int id;
    pthread_t tid;
    struct simple_list *next;
} queue;

queue *barber_queue = NULL;
queue *cut_clients_queue = NULL;
queue *not_cut_clients = NULL;




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
    int time = rand() % (max_cut - min_cut) + min_cut;
    time = time * 1000;
    usleep(time);
}


void  barber_break()
{
    int time = rand() % (100) + 10;
    time = time * 1000;
    usleep(time);
}

// praca barbera
void *barber_funcion(void *idp)
 {
    int i = 0;

    while (1) {
        // Blokuje semafor 'client_readiness', czeka aż jakiś clients będzie gotowy
        sem_wait(&client_readiness);

        // Blokuje semafor 'chair_access' aby nikt nie siadał podczas gdy inny clients wstaje *glupio brzmi ale dziala*
        sem_wait(&chair_access);

        // Zwiekszamy o 1 liczbe dostepnych krzesel w poczekalni i jednoczesnie zmniejszamy w niej ilosc klientow
        free_chairs++;
        actual_number_of_clients--;

        // Odblokowujemy semafor "chair_access"
        sem_post(&chair_access);

        // Odblokowujemy semafor "barber_readiness", do przyjecia kolejnego klienta 
        sem_post(&barber_readiness);

        // Aktualizujemy id klienta ktory jest obecnie obslugiwany
        pthread_t actual_tid=barber_queue->tid;
        serve_client_from_queue();
        cut_clients++;
         printf("Klient ID=%d jest obslugiwany [tid=%ld]\n", actual_serve_id,actual_tid);
        printf("Rezygnacji: %d W Poczekalni: %d/%d [Klient na fotelu: %d ]\n", not_service_clients, actual_number_of_clients, total_chairs, actual_serve_id);
        // Blokujemy mutex "serve_client" 
       // pthread_mutex_lock(&serve_client);

        // Fryzjer strzyże klienta
        cut();

        // Odblokowujemy mutex "serve_client" co znaczy ze barber skonczyl swoja prace
       // pthread_mutex_unlock(&serve_client);
        sem_post(&work_done);
        barber_break();
        // Fryzjer konczy prace jesli nie ma żadnych klientow
        i++;
        if (i == (total_clients - not_service_clients))
            break;
    }
    pthread_exit(NULL);
}


void *client_function(void *idp) 
{
    
    sem_wait(&chair_access); //blokujemy semafor krzesła by dwóch klientów nie usiadło na tym samym miejscu

    client_id++;
    printf("Nowy Klient [ID = %d], [tid = %lu] \n", client_id, pthread_self());

    // Jesli sa jakies krzesla clients czeka na strzyzenie jesli nie klient wychodzi
    if (free_chairs >= 1) 
    {
        actual_number_of_clients++;
        free_chairs--;

        add_client_to_queue(&barber_queue, client_id, pthread_self());
        if (info) 
        {
        printf("Kolejka osob oczekajacych zwiekszyla sie\n");
        print_queue(barber_queue);
           }
        add_client_to_queue(&cut_clients_queue, client_id, pthread_self() );
       

        // Odblokowujemy semafor "client_readiness", ustawiamy klienta na gotowego do strzyenia
        sem_post(&chair_access);

        printf("Zrezygnowalo: %d Poczekalnia: %d/%d [Klient na fotelu: %d]\n", not_service_clients, actual_number_of_clients, total_chairs, actual_serve_id);
        while(1)
        {
            if(pthread_self()==barber_queue->tid)
            {
               break;
            }
        }
        pthread_mutex_lock(&client_waiting);
        sem_post(&client_readiness);
        // Odblokowujemy semafor "chair_access" inny klient może zająć miejsce
        // Blokujemy semafor "barber_readiness" - czekamy az barber bedzie gotowy do strzyzenia
        sem_wait(&barber_readiness);
        sem_wait(&work_done);
        pthread_mutex_unlock(&client_waiting);
        printf("Klient[ID = %d] zostal obsluzony [tid=%ld] \n", actual_serve_id, pthread_self());
    } 
    else
    {
        
        add_client_to_queue(&not_cut_clients, client_id, pthread_self());

        not_service_clients++;

        printf("Klient [ID = %d] wyszedl! (zostal dodany do listy zrezygnowanych) [tid=%ld]\n", client_id,pthread_self());
        if (info) 
           {
        printf("Kolejka zrezygnowanych zwiekszyla sie\n");
        print_queue(not_cut_clients);
           }

        printf("Rezygnacji: %d W Poczekalni: %d/%d [Klient na fotelu: %d ]\n", not_service_clients, actual_number_of_clients, total_chairs, actual_serve_id);
        // Odblokowuje semafor inny klient może zająć miejsce 
        sem_post(&chair_access);
    }
    pthread_exit(NULL);
}



//tworzenie wątkow klientow
void *create_client() 
{
    int temporary;
    int i = 0;

    while (i < total_clients) 
    {
        pthread_t clients;
        temporary = pthread_create(&clients, NULL, (void *) client_function, NULL);
        if (temporary)
            printf("Blad tworzenia watku pracownika");

        i++;
        time_to_next_client();
    }
    return 0;
}


int main(int argc,char *argv[]) 
{
    srand( (unsigned int)time(NULL) );
    //opcje
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

    printf("Podaj ilosc klientow: \n");
    scanf("%d", &total_clients);

    printf("Ilosc krzesel = %d,  Ilosc klientow = %d, \n\n", total_chairs, total_clients);
    free_chairs = total_chairs;
    
    printf("Podaj maksymalny czas strzyzenia\n");
    scanf("%d", &max_cut);

    printf("Podaj minimalny czas strzyzenia \n");
    scanf("%d", &min_cut);

    printf("Podaj maksymalny czas przyjscia kolejnego klienta\n");
    scanf("%d", &max_come);

    printf("Podaj minimalny czas przyjscia kolejnego klienta \n");
    scanf("%d", &min_come);
    

    pthread_t barber;
    pthread_t client_create;
    
    pthread_mutex_init(&client_waiting    , NULL);
    pthread_mutex_init(&serve_client, NULL);
    sem_init(&work_done, 0, 0);
    sem_init(&client_readiness, 0, 0);
    sem_init(&barber_readiness, 0, 0);
    sem_init(&chair_access, 0, 1); //krzesla dostepne

    temporary = pthread_create(&barber, NULL, (void *) barber_funcion, NULL);
    if (temporary != 0) {
        printf("Blad tworzenia watku fryzjera!\n");
        exit(1);
    } else {
        printf("Pomyslnie utworzono watek fryzjera!\n");
    }

    temporary = pthread_create(&client_create, NULL, (void *) create_client, NULL);
    if (temporary != 0) 
    {
        printf("Blad tworzenia watku klienta!\n");
        exit(1);
    } else 
    {
        printf("Pomyslnie utworzono watek klienta!\n\n");
    }

    pthread_join(barber, NULL);
    pthread_join(client_create, NULL);

    printf("\nIlosc klientow ktora musiala opuscic lokal: %d\n", not_service_clients);
    printf("Ci klienci nie zostali obsłużeni: \n");
    print_queue(not_cut_clients);
    printf("Ci klienci zostali obsłużeni \n");
    print_queue(cut_clients_queue);
    pthread_mutex_destroy(&serve_client);

    sem_destroy(&barber_readiness);
    sem_destroy(&client_readiness);
    sem_destroy(&chair_access);

    return 0;
}
