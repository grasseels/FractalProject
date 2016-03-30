#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "libfractal/fractal.h"
#define d "-d"
#define mt "--maxthreads"
#define MAXLEN 1000

int nbFiles =0;
int printAll = 0;
int maxThreads = 1;
int stdinAvailable = 1;

pthread_mutex_t mutex_buffer;
pthread_mutex_t mutex_closing;
sem_t empty;

struct fract *buffer;
int isReading;


//PRODUCTEUR
void *readerFunc(void *param) {
  FILE *fichier = NULL;

  fichier = fopen((char *) param, "r");
  if (fichier == NULL) fprintf(stderr, "Erreur lors de l'ouverture du fichier : %s\n", (char *) param);
  char current[MAXLEN];
  while (fgets(current, MAXLEN, fichier) != NULL){
    if (current[0] != '\n' && current[0] != '#'){
      //Production de l item
      char *n = strtok(str, delim);
      char *name = (char *) malloc(strlen(n) * sizeof(char));
      name = n;
      int w = atoi(strtok(NULL, delim));
      int h = atoi(strtok(NULL, delim));
      double a = atof(strtok(NULL, delim));
      double b = atoi(strtok(NULL, delim));
      struct fract *temp = fractal_new(name, w, h, a, b);

      sem_wait(&empty);
      pthread_mutex_lock(&mutex_buffer);
      //Section Critique

      int placed = 0;
      for (int i = 0; !placed; i++) {
        if (buffer[i] == NULL) {
          buffer[i] = temp;
          placed ++;
        }
      }
      //La structure a été placée pour le calcul : fin de la Section Critique
      pthread_mutex_unlock(&mutex_buffer);

    }
  }

  //On essaie de fermer le fichier
  int err = fclose(fichier);
  if (err != 0) fprintf(stderr, "Erreur lors de la fermeture du fichier : %s\n", (char *) param);

  //Pour marquer qu'on a bien ferme le fichier
  pthread_mutex_lock(&mutex_closing);
  isReading--;
  pthread_mutex_unlock(&mutex_closing);

  return NULL;
}


//CONSOMMATEUR
void *computeFunc (void *param) {
  int arg = (int *) param; //La case du buffer qui lui est attribuee
  struct fract *best = NULL;//La fractale avec la meilleure moyenne
  int bestAverage = 0; //La valeur de la meilleure moyenne

  while (isReading != 0 || buffer[arg] != NULL) {
    struct fract *temp;
    while (buffer[arg] == NULL) {} //Pas besoin d'aller plus loin si il n'y a rien a traiter pour notre thread

    pthread_mutex_lock(&mutex_buffer);
    //Section Critique
    temp = buffer[arg];
    buffer[arg] = NULL;
    pthread_mutex_unlock(&mutex_buffer);
    sem_post(&empty);

    int w = fractal_get_width(temp);
    int h = fractal_get_height(temp);
    double average = 0;
    double total = w * f;

    for (int x = 0; x < w; x++) {
      for (int y = 0; y < h; y++) {
        int val = fractal_compute_value(temp, x, y)
        average += val;
        fractal_set_value(temp, x, y, val);
      }
    }

    average = average / total;

    temp->average = average;

    if (printAll) write_bitmap_sdl(temp, temp->name);

    if ( average > bestAverage) {
      fractal_free(best);
      best = temp;
      bestAverage = average;
    }
    else {
      fractal_free(temp);
    }
  }

  return best;
}

int main(int argc, char const *argv[]) {
  const char *files[argc];
  const char *fileOut = argv[argc-1];

  //On checke les arguments d'entree
  for (int i = 1; i < argc-1; i++) {

    //On regarde si l'argument i est -d
    if (strcmp(argv[i], d) == 0) printAll = 1; //On imprime tout
    else {
      if (strcmp(argv[i], mt) == 0) { //Si l'utilisateur a indique un nombre max de thread,
        maxThreads = atoi(argv[i+1]); // on regle le nombre maximal de thread a ce nombre
        i++; // et on saute une case du tableau d'arguments
      }
      else { // Si ce n'est ni un -d ni un --maxthreads, c'est un nom de fichier
        if (!stdinAvailable && (strcmp(argv[i], "-") == 0))
            fprintf(stderr, "L'entree standard a ete indiquee plusieurs fois comme source. Elle n'est prise en compte qu'une fois\n");
        else {
          if (strcmp(argv[i], "-") == 0) stdinAvailable = 0;
          files[nbFiles] = argv[i];
          nbFiles++;
        }
      }
    }
  }

  printf("Nombre de fichier : %i\n", nbFiles);
  printf("Doit on tout imprimer ? : %i\n", printAll);
  printf("Fichier de sortie : %s\n", fileOut);
  for (int i = 0; i < nbFiles; i++) printf("Fichier de donnee n° %i : %s\n", i+1, files[i]);

  //Initialisation des semaphores et du mutex
  pthread_mutex_init(&mutex, NULL);
  sem_init(&empty, 0, N);
  sem_init(&full, 0,0);
  buffer = (struct fract *) malloc(maxThreads * sizeof(struct fract));
  isReading = nbFiles;

  pthread_t readThreads[nbFiles]
  pthread_t computeThreads[maxThreads];
  int arg[maxThreads];
  int err;

  for (int i = 0; i < nbFiles; i++) {
    if (strcmp(argv[i])==0) {
      err = pthread_create(&(readThreads[i]), NULL, &stdReaderFunc, NULL);
      if (err != 0) fprintf(stderr, "Erreur lors de la creation du reader n° %i (Entree standard)\n", i);
    }
    else {
      err = pthread_create(&(readThreads[i]), NULL, &readerFunc, &(files[i]));
      if (err != 0) fprintf(stderr, "Erreur lors de la creation du reader n° %i\n", i);
    }
  }

  for (int i = 0; i < maxThreads; i++) {
    arg[i] = i;
    err=pthread_create(&(computeThreads[i]), NULL, &(computeFunc), &(arg[i]));
    if (err != 0) fprintf(stderr, "Erreur lors de la creation du thread de calcul n° %i\n", i);
  }

  struct fract *best;
  err=pthread_join(computeThreads[0], (void **) &best);
  if (err != 0) fprintf(stderr, "Erreur lors du join du thread de calcul n° 1\n");

  for (int i = 1; i < maxThreads; i++) {
    struct fract *temp;
    err=pthread_join(computeThreads[0], (void **) &temp);
    if (err != 0) fprintf(stderr, "Erreur lors du join du thread de calcul n° %i\n", i);
    if (best->average < temp->average) {
      fractal_free(best);
      best = temp;
    }
    else fractal_free(temp);
  }

    err = write_bitmap_sdl(best, &fileOut);
    if (err != 0) fprintf(stderr, "Erreur lors de l'ecriture de la meilleure fractale\n");
    return err;
}
