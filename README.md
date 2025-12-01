# Assignment 3 Part 2

## How to Compile and Run

This project contains two main programs: `part2a.c` and `part2b.c`.

### Compilation

To compile the programs, open a terminal in this directory and run:

```
gcc parta.c -o parta
gcc partb.c -o partb
```

### Running the Programs

To run the programs with the provided test cases, where the different test cases are the number of TAs to test, use the following commands:

```
Example: Run with 2 TAs
./parta 2
./partb 2

Example: Run with 3 TAs
./parta 3
./partb 3
```

The exam files in `exam-files` will be run for every test case

## Design Discussion: Critical Section Problem

The solution is designed with the three classic requirements for solving the critical section problem:

1. **Mutual Exclusion**
   - The design ensures that only one process can enter the critical section at a time. This is achieved using synchronization mechanisms (such as semaphores, mutexes, or flags) to prevent race conditions and guarantee exclusive access. 
   - This allows only one TA (process) to check/access rubric questions line by line, as well as making sure only one TA updates the loaded exam into the shared memory per `examNN.txt` file.

2. **Progress**
   - The solution guarantees that if no process is in the critical section and some processes wish to enter, the selection of the next process is not postponed indefinitely. The algorithm avoids deadlock and ensures that waiting processes can eventually proceed. 
   - In our context, execution never stopped, as when one question of an exam is not being marked by an occupied TA, it will then be marked by the next TA (process) available. This ensures continuous execution for all exams marked. 

3. **Bounded Waiting**
   - The design ensures that each process has a bounded number of times it can be bypassed by other processes before entering the critical section. This prevents starvation and ensures fairness among all processes. 
   - Once a question is being marked by one TA (process), the other TA does not have to wait; rather, it will be handed the next available question to be marked, and so on for the rest of the exams. This ensures all exams get marked as quickly as possible while efficiently utilizing all the TAs available.
   

---