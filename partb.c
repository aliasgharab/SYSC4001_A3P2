#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/sem.h>


#define SEM_KEY 1234
#define SEM_RUBRIC_WRITE 0
#define SEM_EXAM_CHANGE  1
#define SEM_EXAM_LOAD    2
#define SEM_QUESTION_CLAIM 3  


#define NUM_EXAMS 21           
#define RUBRIC_LINES 5         
#define LINE_LEN 16         

typedef struct {
    int next_exam; //exam index
    int total_exams; //total exams
    int current_student; //exam loaded in
    int questions_marked[RUBRIC_LINES]; //current question being marked
    char rubric[RUBRIC_LINES][LINE_LEN]; //rubric loaded from file
} SharedData;

void sem_wait(int semid, int sem_num) {
    struct sembuf sb = {sem_num, -1, 0};
    semop(semid, &sb, 1);
}

void sem_signal(int semid, int sem_num) {
    struct sembuf sb = {sem_num, 1, 0};
    semop(semid, &sb, 1);
}

//Load the rubric into the shared memory segment
int load_rubric(char buf[RUBRIC_LINES][LINE_LEN]) {
    FILE *file = fopen("rubric.txt", "r");
    
    if (file == NULL) {
        printf("rubric.txt not found\n");
        return -1;
    }

    for (int i = 0; i < RUBRIC_LINES; i++) {
        if (fgets(buf[i], LINE_LEN, file) == NULL) {
            printf("rubric.txt ended early\n");
            fclose(file);
            return -1;
        }
        // Remove newline characters if present
        buf[i][strcspn(buf[i], "\n")] = '\0';
    }
    
    fclose(file);
    return 0;
}

//Load exam into the shared memory segment

void load_exam(SharedData *shptr, int exam_index) {
    char filename[64];
    snprintf(filename, sizeof(filename), "exam-files/exam%02d.txt", exam_index + 1);
    
    FILE *ef = fopen(filename, "r");
    if (ef) {
        fscanf(ef, "%d", &shptr->current_student);
        fclose(ef);

        // Reset questions when new exam starts
        for (int i = 0; i < RUBRIC_LINES; i++) {
            shptr->questions_marked[i] = 0;
        }

        printf("Loaded exam %s (student %d) into shared memory\n", filename, shptr->current_student);
    }
}

//save the rubric into the shared memory segment
void save_rubric(char buf[RUBRIC_LINES][LINE_LEN]) {
    FILE *file = fopen("rubric.txt", "w");
    if (file) {
        for (int i = 0; i < RUBRIC_LINES; i++) {
            fprintf(file, "%s\n", buf[i]);
        }
        fclose(file);
    }
}

int claim_question(SharedData *shptr, int semid) {
    sem_wait(semid, SEM_QUESTION_CLAIM);
    int question = -1;
    for (int i = 0; i < RUBRIC_LINES; i++) {
        if (shptr->questions_marked[i] == 0) {
            shptr->questions_marked[i] = 1;  // Mark as claimed
            question = i;
            break;
        }
    }
    sem_signal(semid, SEM_QUESTION_CLAIM);
    return question;
}


void ta_child(int ta_id, SharedData *shptr, int semid) {
    pid_t pid = getpid();
    srand(time(NULL) + pid + ta_id);

    while (1) {

        sem_wait(semid, SEM_EXAM_CHANGE);

        if (shptr->next_exam >= shptr->total_exams) {
            sem_signal(semid, SEM_EXAM_CHANGE);
            break;
        }

        sem_signal(semid, SEM_EXAM_CHANGE);

        while (1) {

            sem_wait(semid, SEM_EXAM_CHANGE);
            if (shptr->current_student == 9999) {
                printf("TA %d (pid %d): encountered STOP marker -> signalling finish.\n", ta_id, (int)pid);
                shptr->next_exam = shptr->total_exams;
                sem_signal(semid, SEM_EXAM_CHANGE);
                break;
            }
            sem_signal(semid, SEM_EXAM_CHANGE);

            int question = claim_question(shptr, semid);
            if (question == -1) break;
            
            printf("TA %d (pid %d): claiming question %d for student %d\n", ta_id, (int)pid, question + 1, shptr->current_student);
            
            // Rubric checking for this specific question
            char first_char = (shptr->rubric[question][0] != '\0') ? shptr->rubric[question][0] : ' ';
            printf("TA %d (pid %d): checking rubric line %d (char '%c')\n",ta_id, (int)pid, question + 1, first_char);
            
            struct timespec ts;
            ts.tv_sec = 0;
            ts.tv_nsec = 500000000L + (rand() % 500000001L);
            nanosleep(&ts, NULL);
            
            // Rubric correction (if needed)
            if (rand() % 100 < 30) {
                sem_wait(semid, SEM_RUBRIC_WRITE);
                char *comma_pos = strchr(shptr->rubric[question], ',');
                if (comma_pos && comma_pos[1] != '\0') {
                    char *char_pos = comma_pos + 2;
                    if (*char_pos >= 'A' && *char_pos < 'Z') {
                        (*char_pos)++;
                        printf("TA %d (pid %d): CORRECTED rubric line %d (now '%c')\n", 
                            ta_id, (int)pid, question + 1, *char_pos);
                        save_rubric(shptr->rubric);  // Save immediately after correction
                    }
                }
                sem_signal(semid, SEM_RUBRIC_WRITE);
            }
            
            // Mark the question
            printf("TA %d (pid %d): marking question %d for student %d\n", ta_id, (int)pid, question + 1, shptr->current_student);
            ts.tv_nsec = 1000000000L + (rand() % 1000000001L);
            nanosleep(&ts, NULL);
        }
        
        //increment the next exam
        sem_wait(semid, SEM_EXAM_CHANGE);
        shptr->next_exam = shptr->next_exam + 1;
        sem_signal(semid, SEM_EXAM_CHANGE);

        //load the next exam
        sem_wait(semid, SEM_EXAM_LOAD);
        if (shptr->next_exam < shptr->total_exams) {
            printf("TA %d (pid %d): ", ta_id, (int)pid);
            load_exam(shptr, shptr->next_exam);
        }
        sem_signal(semid, SEM_EXAM_LOAD);
    }

    _exit(0);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <num_TAs>\n", argv[0]);
        return 1;
    }

    int num_TAs = atoi(argv[1]);
    if (num_TAs <= 0) {
        fprintf(stderr, "num_TAs must be a positive integer\n");
        return 1;
    }

    size_t shmsz = sizeof(SharedData);
    int shmid = shmget(IPC_PRIVATE, shmsz, IPC_CREAT | 0600);
    if (shmid == -1) {
        perror("shmget");
        return 1;
    }

    SharedData *shptr = (SharedData *) shmat(shmid, NULL, 0);
    if (shptr == (void*)-1) {
        perror("shmat");
        shmctl(shmid, IPC_RMID, NULL);
        return 1;
    }

    int semid = semget(SEM_KEY, 4, IPC_CREAT | 0666);
    semctl(semid, SEM_RUBRIC_WRITE, SETVAL, 1);
    semctl(semid, SEM_EXAM_CHANGE, SETVAL, 1);
    semctl(semid, SEM_EXAM_LOAD, SETVAL, 1);
    semctl(semid, SEM_QUESTION_CLAIM, SETVAL, 1);  

    shptr->next_exam = 0;
    shptr->total_exams = NUM_EXAMS;
    load_rubric(shptr->rubric); 
    load_exam(shptr, 0); 

    for (int i = 0; i < num_TAs; ++i) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            continue;
        } else if (pid == 0) {
            ta_child(i + 1, shptr, semid);
        }
    }

    
    int status;
    while (wait(&status) > 0) {
        // wait until no more children
    }

    shmdt((void *) shptr);
    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        perror("shmctl IPC_RMID");
    }

    if (semctl(semid, 0, IPC_RMID) == -1) {
        perror("semctl IPC_RMID");
    }

    printf("Parent: all TAs finished. Shared memory removed.\n");
    return 0;
}

/* 
Student 1: Ali Asghar Bundookwalla, 101299213
Student 2: Mohamed Gomaa, 101309418
*/