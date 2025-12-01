#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>

#define NUM_EXAMS 21           
#define RUBRIC_LINES 5         
#define LINE_LEN 16         

typedef struct {
    int next_exam; //exam index
    int total_exams; //total exams
    int current_student; //exam loaded in
    char rubric[RUBRIC_LINES][LINE_LEN]; //rubric loaded from file
} SharedData;

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

//Load into exam into shared memory
void load_exam(SharedData *shptr, int exam_index) {
    char filename[64];
    snprintf(filename, sizeof(filename), "exam-files/exam%02d.txt", exam_index + 1);
    
    FILE *ef = fopen(filename, "r");
    if (ef) {
        fscanf(ef, "%d", &shptr->current_student);
        fclose(ef);
        printf("Loaded exam %s for (student %d) into shared memory\n", filename, shptr->current_student);
    }
}

//save rubric into shared memory 
void save_rubric(char buf[RUBRIC_LINES][LINE_LEN]) {
    FILE *file = fopen("rubric.txt", "w");
    if (file) {
        for (int i = 0; i < RUBRIC_LINES; i++) {
            fprintf(file, "%s\n", buf[i]);
        }
        fclose(file);
    }
}


void ta_child(int ta_id, SharedData *shptr) {
    pid_t pid = getpid();
    srand(time(NULL) + pid + ta_id);

    while (1) {

        int my_index = shptr->next_exam;  

        if (my_index >= shptr->total_exams) {
            break;
        }

        if (shptr->current_student == 9999) {
            printf("TA %d (pid %d): encountered STOP marker (student 9999) -> signalling finish.\n", ta_id, (int)pid);
            shptr->next_exam = shptr->total_exams;
            break;
        }

        //mark questions
        int rubric_modified = 0;
        for (int r = 0; r < RUBRIC_LINES; ++r) {
            char first_char = (shptr->rubric[r][0] != '\0') ? shptr->rubric[r][0] : ' ';
            printf("TA %d (pid %d): checking rubric line %d (char '%c')\n",ta_id, (int)pid, r + 1, first_char);
            
            struct timespec ts;
            ts.tv_sec = 0;
            ts.tv_nsec = 500000000L + (rand() % 500000001L);
            nanosleep(&ts, NULL);
            
            if (rand() % 100 < 30) {
                char *comma_pos = strchr(shptr->rubric[r], ',');
                if (comma_pos && comma_pos[1] != '\0') {
                    char *char_pos = comma_pos + 2;
                    if (*char_pos >= 'A' && *char_pos < 'Z') {
                        (*char_pos)++;
                        printf("TA %d (pid %d): CORRECTED rubric line %d (now '%c')\n", ta_id, (int)pid, r + 1, *char_pos);
                        rubric_modified = 1;
                    }
                }
            }
            
            printf("TA %d (pid %d): marking question %d for student %d\n", ta_id, (int)pid, r + 1, shptr->current_student);
            ts.tv_nsec = 1000000000L + (rand() % 1000000001L);
            nanosleep(&ts, NULL);
        }
        
        if (rubric_modified) {
            save_rubric(shptr->rubric);
        }

        //increment the next exam
        shptr->next_exam = my_index + 1;

        //load the next exam
        if (my_index + 1 < shptr->total_exams) {
            load_exam(shptr, my_index + 1);
        }
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
            ta_child(i + 1, shptr);
        }
    }

    
    int status;
    while (wait(&status) > 0) {
        /* wait until no more children */
    }

    shmdt((void *) shptr);
    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        perror("shmctl IPC_RMID");
    }

    printf("Parent: all TAs finished. Shared memory removed.\n");
    return 0;
}

/* 
Student 1: Ali Asghar Bundookwalla, 101299213
Student 2: Mohamed Gomaa, 101309418
*/