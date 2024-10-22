#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <semaphore.h>  // Inclusion de la biblioteca de semáforos
#include <sys/types.h>
#include <sys/wait.h>

#define NUM_PLAYERS 4
#define CAPACIDAD 1  // Capacidad del buzón (1 para pasar un mensaje a la vez)

typedef int ELEMS;  // Tipo de elemento en el buzón

typedef struct {
    ELEMS buzon[CAPACIDAD];  // Array para almacenar elementos del buzón
    int ent;                 // Índice para entrada de elementos
    int sal;                 // Índice para salida de elementos
    sem_t s;                 // Semáforo para exclusión mutua
    sem_t e;                 // Semáforo para contar espacios vacíos (capacidad)
    sem_t n;                 // Semáforo para bloquear al consumidor cuando está vacío
} MAILBOX;

void mailbox_init(MAILBOX *b) {
    b->ent = 0;
    b->sal = 0;
    sem_init(&b->s, 1, 1);      // Inicializa el semáforo de exclusión mutua
    sem_init(&b->e, 1, CAPACIDAD);  // Inicializa el semáforo de capacidad con la capacidad máxima
    sem_init(&b->n, 1, 0);      // Inicializa el semáforo de elementos presentes en 0
}

void mailbox_send(MAILBOX *b, ELEMS e) {
    sem_wait(&b->e);         // Espera a que haya un espacio vacío
    sem_wait(&b->s);         // Entra en la sección crítica

    b->buzon[b->ent] = e;    // Coloca el elemento en el buzón
    b->ent = (b->ent + 1) % CAPACIDAD;  // Incrementa el índice de entrada, cíclicamente

    sem_post(&b->s);       // Sale de la sección crítica
    sem_post(&b->n);       // Incrementa el contador de elementos presentes
}

void mailbox_receive(MAILBOX *b, ELEMS *e) {
    sem_wait(&b->n);         // Espera hasta que haya un elemento
    sem_wait(&b->s);         // Entra en la sección crítica

    *e = b->buzon[b->sal];   // Recupera el elemento del buzón
    b->sal = (b->sal + 1) % CAPACIDAD;  // Incrementa el índice de salida, cíclicamente

    sem_post(&b->s);       // Sale de la sección crítica
    sem_post(&b->e);       // Incrementa el contador de espacios vacíos
}

void play_or_pass(int id) {
    // Simula jugar una ficha o pasar
    printf("Jugador %d toma su turno.\n", id);
    sleep(1);  // Simula el tiempo que tarda en jugar
}

MAILBOX mailboxes[NUM_PLAYERS];

void player(int id) {
    int next_player_id = (id - 1 + NUM_PLAYERS) % NUM_PLAYERS;  // Sentido contrario a las manecillas del reloj

    ELEMS msg;

    while (1) {
        // Espera un mensaje en su buzón indicando que es su turno
        mailbox_receive(&mailboxes[id], &msg);

        // Toma su turno: juega o pasa
        play_or_pass(id);

        // Envía un mensaje al siguiente jugador para indicar que es su turno
        mailbox_send(&mailboxes[next_player_id], msg);
    }
}

void initialize_mailboxes() {
    for (int i = 0; i < NUM_PLAYERS; i++) {
        mailbox_init(&mailboxes[i]);
    }
}

int main() {
    initialize_mailboxes();

    pid_t pids[NUM_PLAYERS];

    // Crea procesos para cada jugador
    for (int i = 0; i < NUM_PLAYERS; i++) {
        pids[i] = fork();
        if (pids[i] == 0) {
            // Proceso hijo
            player(i);
            exit(0);
        }
    }

    // Inicia el juego enviando un mensaje al jugador 0
    ELEMS start_msg = 1;  // Valor arbitrario para iniciar
    mailbox_send(&mailboxes[0], start_msg);

    // Espera a que los procesos terminen (opcional, ya que es un bucle infinito)
    for (int i = 0; i < NUM_PLAYERS; i++) {
        wait(NULL);
    }

    return 0;
}
