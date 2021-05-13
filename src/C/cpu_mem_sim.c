#include <stdbool.h> 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "cpu_mem_sim.h"

/**
 * main
 * 
 * Exits if command line arguments aren't correct length (less than 2)
 * Sets values for filename and interrupt (interrupt default is 10,000)
 * Creates two pipes
 * Creates a child process (memory process)
 * 
 * @param argc holds count for command line arguments  
 * @param argv holds values from command line entries
 */
int main(int argc, char **argv) {
    int returnStatus = 0;
    int interrupt;
    char const *fileName;

    // checking argument counts, setting values
    if (argc == 3) {
        fileName = argv[1];
        interrupt = atoi(argv[2]);
    }
    else if (argc == 2) {
        fileName = argv[1];
        interrupt = 10000;
    } 
    else {
        errorExit("wrong number of arguments");
    }

    if (access(fileName, F_OK) != 0)
        errorExit("wrong file name or no file");

    int cpuToMemory[2];
    int memoryToCPU[2];

    if (pipe(cpuToMemory) == -1)     
        errorExit("pipe() failed");

    if (pipe(memoryToCPU) == -1)  
        errorExit("pipe() failed");

    // memory -- child
    pid_t childPid = fork();
    if (childPid == 0) {
        memoryProcess(cpuToMemory, memoryToCPU, fileName);
        exit(0);
    }
    // cpu -- parent
    else {
        cpuProcess(cpuToMemory, memoryToCPU, interrupt);
        waitpid(childPid, &returnStatus, 0);
    }

    return 0;
} /* end main */

/**
 * Acts as memory (child process)
 * Validates file input
 * 
 * @param cpuToMemory is for piping from CPU to Memory
 * @param memoryToCPU is for piping from Memory to CPU
 * @param fileName holds filename value user entered
 */
void memoryProcess(int *cpuToMemory, int *memoryToCPU, char const *fileName) {
    int memoryArray[2000] = {0};

    // validate and process file
    validateFile(memoryArray, fileName);
    closePipes(cpuToMemory, memoryToCPU, 1, 0);

    int ptr, tempValue;
    int currentStatus = 0;
    int const exitStatus = 99;

    // read = 82, write = 87 (ascii for r, w)
    int const readStatus = getReadStatus();
    int const writeStatus = getWriteStatus();
    
    // continue until cpu process sends exit signal, 99
    while (currentStatus != exitStatus) {
        currentStatus = readFromCPU(cpuToMemory);

        // if cpu wants to read from memory, write back value at address
        if (currentStatus == readStatus) {
            ptr = readFromCPU(cpuToMemory);
            writeToCPU(memoryToCPU, memoryArray, ptr);
        }

        // if cpu wants to write to memory, get ptr & value, and update address
        if (currentStatus == writeStatus) {
            ptr = readFromCPU(cpuToMemory);
            tempValue = readFromCPU(cpuToMemory);
            memoryArray[ptr] = tempValue;
        }
    }
} /* end memoryProcess */

/**
 * Acts as CPU (parent process)
 * 
 * @param cpuToMemory is for piping from CPU to Memory
 * @param memoryToCPU is for piping from Memory to CPU
 * @param interrupt holds value for when to interrupt processing
 */
void cpuProcess(int *cpuToMemory, int *memoryToCPU, int interrupt) {
    closePipes(cpuToMemory, memoryToCPU, 0, 1);

    int PC, SP, IR, AC, X, Y; 
    int tempValue, tempSP, timer;

    bool kernelMode = false;
    PC = 0;
    timer = 0;

    // SP set to 1000
    SP = getMaxUserProgramEntry() + 1;

    // read = 82, write = 87 (ascii for r, w)
    int const readStatus = getReadStatus();
    int const writeStatus = getWriteStatus();

    // Before exiting loop, CPU sends exit signal (99) to memory, in case 50
    while (IR != 50) {
        /* 
            Validating memory accesses are OK to perform; dependent on kernelMode and ptr value.
            Exit if invalid.
        */
        if (validateAddressAccess(PC, kernelMode)) {
            pipeReadStatusAndPTR(cpuToMemory, PC, readStatus);
            IR = readFromMemory(memoryToCPU);
        }
        else {
            errorExit("Memory violation: accessing address in wrong mode");
        } 

        /*
            Validate memory acceses with every read or write. Some cases have multiple validations. 
            Cases based on Instruction Register value (IR).
            Timer interrupts checked after each instruction execution.

            Dependent on case, some cases increment PC twice.
            Timer increments in every case, after instruction execution.

            If Timer interrupt valid, SP and PC registers saved, 
            then PC set to 1000 and SP switched to system stack.
        */
        switch (IR) {
            case 1:
                /* Load the value into the AC */
                PC += 1;
                if (validateAddressAccess(PC, kernelMode)) {
                    pipeReadStatusAndPTR(cpuToMemory, PC, readStatus);
                    AC = readFromMemory(memoryToCPU);
                }
                else {
                    errorExit("Memory violation: accessing address in wrong mode");
                }

                PC += 1;
                timer += 1;
                if (validateTimerInterrupt(interrupt, timer, kernelMode)) {
                    kernelMode = true;
                    PC = timerInterrupt(cpuToMemory, SP, PC, getMaxSystemCodeEntry());
                    SP = getMaxSystemCodeEntry() - 1;
                }
                break;

            case 2:
                /* Load the value at the address into the AC */
                PC += 1;
                if (validateAddressAccess(PC, kernelMode)) {
                    pipeReadStatusAndPTR(cpuToMemory, PC, readStatus);
                    tempValue = readFromMemory(memoryToCPU);
                }
                else {
                    errorExit("Memory violation: accessing address in wrong mode");
                }

                if (validateAddressAccess(tempValue, kernelMode)) {
                    pipeReadStatusAndPTR(cpuToMemory, tempValue, readStatus);
                    AC = readFromMemory(memoryToCPU);
                } 
                else {
                    errorExit("Memory violation: accessing address in wrong mode");
                }

                PC += 1;
                timer += 1;
                if (validateTimerInterrupt(interrupt, timer, kernelMode)) {
                    kernelMode = true;
                    PC = timerInterrupt(cpuToMemory, SP, PC, getMaxSystemCodeEntry());
                    SP = getMaxSystemCodeEntry() - 1;
                }
                break;

            case 3:
                /* Load the value from the address found in the given address into the AC */
                PC += 1;
                if (validateAddressAccess(PC, kernelMode)) {
                    pipeReadStatusAndPTR(cpuToMemory, PC, readStatus);
                    tempValue = readFromMemory(memoryToCPU);
                } 
                else {
                    errorExit("Memory violation: accessing address in wrong mode");
                }

                if (validateAddressAccess(tempValue, kernelMode)) {
                    pipeReadStatusAndPTR(cpuToMemory, tempValue, readStatus);
                    tempValue = readFromMemory(memoryToCPU);
                } 
                else {
                    errorExit("Memory violation: accessing address in wrong mode");
                }

                if (validateAddressAccess(tempValue, kernelMode)) {
                    pipeReadStatusAndPTR(cpuToMemory, tempValue, readStatus);
                    AC = readFromMemory(memoryToCPU);
                } 
                else {
                    errorExit("Memory violation: accessing address in wrong mode");
                }

                PC += 1;
                timer += 1;
                if (validateTimerInterrupt(interrupt, timer, kernelMode)) {
                    kernelMode = true;
                    PC = timerInterrupt(cpuToMemory, SP, PC, getMaxSystemCodeEntry());
                    SP = getMaxSystemCodeEntry() - 1;
                }
                break;

            case 4:
                /* Load the value at (address+X) into the AC */
                PC += 1;
                if (validateAddressAccess(PC, kernelMode)) {
                    pipeReadStatusAndPTR(cpuToMemory, PC, readStatus);
                    tempValue = readFromMemory(memoryToCPU);
                } 
                else {
                    errorExit("Memory violation: accessing address in wrong mode");
                }

                tempValue += X;
                if (validateAddressAccess(tempValue, kernelMode)) {
                    pipeReadStatusAndPTR(cpuToMemory, tempValue, readStatus);
                    AC = readFromMemory(memoryToCPU);
                } 
                else {
                    errorExit("Memory violation: accessing address in wrong mode");
                }

                PC += 1;
                timer += 1;
                if (validateTimerInterrupt(interrupt, timer, kernelMode)) {
                    kernelMode = true;
                    PC = timerInterrupt(cpuToMemory, SP, PC, getMaxSystemCodeEntry());
                    SP = getMaxSystemCodeEntry() - 1;
                }
                break;

            case 5:
                /* Load the value at (address+Y) into the AC */
                PC += 1;
                if (validateAddressAccess(PC, kernelMode)) {
                    pipeReadStatusAndPTR(cpuToMemory, PC, readStatus);
                    tempValue = readFromMemory(memoryToCPU);
                } 
                else {
                    errorExit("Memory violation: accessing address in wrong mode");
                }

                tempValue += Y;
                if (validateAddressAccess(tempValue, kernelMode)) {
                    pipeReadStatusAndPTR(cpuToMemory, tempValue, readStatus);
                    AC = readFromMemory(memoryToCPU);
                } 
                else {
                    errorExit("Memory violation: accessing address in wrong mode");
                }

                PC += 1;
                timer += 1;
                if (validateTimerInterrupt(interrupt, timer, kernelMode)) {
                    kernelMode = true;
                    PC = timerInterrupt(cpuToMemory, SP, PC, getMaxSystemCodeEntry());
                    SP = getMaxSystemCodeEntry() - 1;
                }
                break;

            case 6:
                /* Load from (Sp+X) into the AC. */
                PC += 1;
                int tempAddr = SP + X;
                if (validateAddressAccess(tempAddr, kernelMode)) {
                    pipeReadStatusAndPTR(cpuToMemory, tempAddr, readStatus);
                    AC = readFromMemory(memoryToCPU);
                } 
                else {
                    errorExit("Memory violation: accessing address in wrong mode");
                }

                timer += 1;
                if (validateTimerInterrupt(interrupt, timer, kernelMode)) {
                    kernelMode = true;
                    PC = timerInterrupt(cpuToMemory, SP, PC, getMaxSystemCodeEntry());
                    SP = getMaxSystemCodeEntry() - 1;
                }
                break;

            case 7:
                /* Store the value in the AC into the address */
                PC += 1;
                if (validateAddressAccess(PC, kernelMode)) {
                    pipeReadStatusAndPTR(cpuToMemory, PC, readStatus);
                    tempValue = readFromMemory(memoryToCPU);
                } 
                else {
                    errorExit("Memory violation: accessing address in wrong mode");
                }

                if (validateAddressAccess(tempValue, kernelMode)) {
                    pipeAddressToStack(cpuToMemory, writeStatus, tempValue, AC);
                }                
                else {
                    errorExit("Memory violation: accessing address in wrong mode");
                }

                PC += 1;
                timer += 1;
                if (validateTimerInterrupt(interrupt, timer, kernelMode)) {
                    kernelMode = true;
                    PC = timerInterrupt(cpuToMemory, SP, PC, getMaxSystemCodeEntry());
                    SP = getMaxSystemCodeEntry() - 1;
                }
                break;

            case 8:
                /* Gets a random int from 1 to 100 into the AC */
                PC += 1;
                AC = randomInteger(PC);

                timer += 1;
                if (validateTimerInterrupt(interrupt, timer, kernelMode)) {
                    kernelMode = true;
                    PC = timerInterrupt(cpuToMemory, SP, PC, getMaxSystemCodeEntry());
                    SP = getMaxSystemCodeEntry() - 1;
                }
                break;

            case 9:
                /*
                    If port = 1, writes AC as an int to the screen
                    If port = 2, writes AC as a char to the screen
                */
                PC += 1;
                int port;
                if (validateAddressAccess(PC, kernelMode)) {
                    pipeReadStatusAndPTR(cpuToMemory, PC, readStatus);
                    port = readFromMemory(memoryToCPU);
                } 
                else {
                    errorExit("Memory violation: accessing address in wrong mode");
                }

                showAC(port, AC);
                PC += 1;
                timer += 1;
                if (validateTimerInterrupt(interrupt, timer, kernelMode)) {
                    kernelMode = true;
                    PC = timerInterrupt(cpuToMemory, SP, PC, getMaxSystemCodeEntry());
                    SP = getMaxSystemCodeEntry() - 1;
                }
                break;

            case 10:
                /* Add the value in X to the AC */
                PC += 1;
                AC += X;

                timer += 1;
                if (validateTimerInterrupt(interrupt, timer, kernelMode)) {
                    kernelMode = true;
                    PC = timerInterrupt(cpuToMemory, SP, PC, getMaxSystemCodeEntry());
                    SP = getMaxSystemCodeEntry() - 1;
                }
                break;

            case 11:
                /* Add the value in Y to the AC */
                PC += 1;
                AC += Y;

                timer += 1;
                if (validateTimerInterrupt(interrupt, timer, kernelMode)) {
                    kernelMode = true;
                    PC = timerInterrupt(cpuToMemory, SP, PC, getMaxSystemCodeEntry());
                    SP = getMaxSystemCodeEntry() - 1;
                }
                break;

            case 12:
                /* Subtract the value in X from the AC */
                PC += 1;
                AC -= X;

                timer += 1;
                if (validateTimerInterrupt(interrupt, timer, kernelMode)) {
                    kernelMode = true;
                    PC = timerInterrupt(cpuToMemory, SP, PC, getMaxSystemCodeEntry());
                    SP = getMaxSystemCodeEntry() - 1;
                }
                break;

            case 13:
                /* Subtract the value in Y from the AC */
                PC += 1;
                AC -= Y;

                timer += 1;
                if (validateTimerInterrupt(interrupt, timer, kernelMode)) {
                    kernelMode = true;
                    PC = timerInterrupt(cpuToMemory, SP, PC, getMaxSystemCodeEntry());
                    SP = getMaxSystemCodeEntry() - 1;
                }
                break;

            case 14:
                /* Copy the value in the AC to X */
                PC += 1;
                X = AC;

                timer += 1;
                if (validateTimerInterrupt(interrupt, timer, kernelMode)) {
                    kernelMode = true;
                    PC = timerInterrupt(cpuToMemory, SP, PC, getMaxSystemCodeEntry());
                    SP = getMaxSystemCodeEntry() - 1;
                }
                break;

            case 15:
                /* Copy the value in X to the AC */
                PC += 1;
                AC = X;

                timer += 1;
                if (validateTimerInterrupt(interrupt, timer, kernelMode)) {
                    kernelMode = true;
                    PC = timerInterrupt(cpuToMemory, SP, PC, getMaxSystemCodeEntry());
                    SP = getMaxSystemCodeEntry() - 1;
                }
                break;

            case 16:
                /* Copy the value in the AC to Y */
                PC += 1;
                Y = AC;

                timer += 1;
                if (validateTimerInterrupt(interrupt, timer, kernelMode)) {
                    kernelMode = true;
                    PC = timerInterrupt(cpuToMemory, SP, PC, getMaxSystemCodeEntry());
                    SP = getMaxSystemCodeEntry() - 1;
                }
                break;

            case 17:
                /* Copy the value in Y to the AC */
                PC += 1;
                AC = Y;

                timer += 1;
                if (validateTimerInterrupt(interrupt, timer, kernelMode)) {
                    kernelMode = true;
                    PC = timerInterrupt(cpuToMemory, SP, PC, getMaxSystemCodeEntry());
                    SP = getMaxSystemCodeEntry() - 1;
                }
                break;

            case 18:
                /* Copy the value in AC to the SP */
                PC += 1;
                SP = AC;

                timer += 1;
                if (validateTimerInterrupt(interrupt, timer, kernelMode)) {
                    kernelMode = true;
                    PC = timerInterrupt(cpuToMemory, SP, PC, getMaxSystemCodeEntry());
                    SP = getMaxSystemCodeEntry() - 1;
                }
                break;

            case 19:
                /* Copy the value in SP to the AC */
                PC += 1;
                AC = SP;

                timer += 1;
                if (validateTimerInterrupt(interrupt, timer, kernelMode)) {
                    kernelMode = true;
                    PC = timerInterrupt(cpuToMemory, SP, PC, getMaxSystemCodeEntry());
                    SP = getMaxSystemCodeEntry() - 1;
                }
                break;

            case 20:
                /* Jump to the address */
                PC += 1;
                if (validateAddressAccess(PC, kernelMode)) {
                    pipeReadStatusAndPTR(cpuToMemory, PC, readStatus);
                    PC = readFromMemory(memoryToCPU);
                } 
                else {
                    errorExit("Memory violation: accessing address in wrong mode");
                }

                timer += 1;
                if (validateTimerInterrupt(interrupt, timer, kernelMode)) {
                    kernelMode = true;
                    PC = timerInterrupt(cpuToMemory, SP, PC, getMaxSystemCodeEntry());
                    SP = getMaxSystemCodeEntry() - 1;
                }
                break;

            case 21:
                /* Jump to the address only if the value in the AC is zero */
                PC += 1;
                
                if (AC == 0) {
                    if (validateAddressAccess(PC, kernelMode)) {
                        pipeReadStatusAndPTR(cpuToMemory, PC, readStatus);
                        PC = readFromMemory(memoryToCPU);
                    } 
                    else {
                        errorExit("Memory violation: accessing address in wrong mode");
                    }
                }
                else {
                    PC += 1;
                }

                timer += 1;
                if (validateTimerInterrupt(interrupt, timer, kernelMode)) {
                    kernelMode = true;
                    PC = timerInterrupt(cpuToMemory, SP, PC, getMaxSystemCodeEntry());
                    SP = getMaxSystemCodeEntry() - 1;
                }
                break;

            case 22:
                /* Jump to the address only if the value in the AC is not zero */
                PC += 1;
                
                if (AC != 0) {
                    if (validateAddressAccess(PC, kernelMode)) {
                        pipeReadStatusAndPTR(cpuToMemory, PC, readStatus);
                        PC = readFromMemory(memoryToCPU);
                    } 
                    else {
                        errorExit("Memory violation: accessing address in wrong mode");
                    }
                }
                else {
                    PC += 1;
                }

                timer += 1;
                if (validateTimerInterrupt(interrupt, timer, kernelMode)) {
                    kernelMode = true;
                    PC = timerInterrupt(cpuToMemory, SP, PC, getMaxSystemCodeEntry());
                    SP = getMaxSystemCodeEntry() - 1;
                }
                break;

            case 23:
                /* Push return address onto stack, jump to the address */
                PC += 1;
                SP -= 1;

                if (validateAddressAccess(SP, kernelMode)) {
                    pipeAddressToStack(cpuToMemory, writeStatus, SP, PC);
                }
                else {
                    errorExit("Memory violation: accessing address in wrong mode");
                }

                if (validateAddressAccess(PC, kernelMode)) {
                    pipeReadStatusAndPTR(cpuToMemory, PC, readStatus);
                    PC = readFromMemory(memoryToCPU);
                } 
                else {
                    errorExit("Memory violation: accessing address in wrong mode");
                }

                timer += 1;
                if (validateTimerInterrupt(interrupt, timer, kernelMode)) {
                    kernelMode = true;
                    PC = timerInterrupt(cpuToMemory, SP, PC, getMaxSystemCodeEntry());
                    SP = getMaxSystemCodeEntry() - 1;
                }
                break;

            case 24:
                /* Pop return address from the stack, jump to address */
                PC = SP;
                if (validateAddressAccess(PC, kernelMode)) {
                    pipeReadStatusAndPTR(cpuToMemory, PC, readStatus);
                    PC = readFromMemory(memoryToCPU);
                } 
                else {
                    errorExit("Memory violation: accessing address in wrong mode");
                }

                SP += 1;
                PC += 1;

                timer += 1;
                if (validateTimerInterrupt(interrupt, timer, kernelMode)) {
                    kernelMode = true;
                    PC = timerInterrupt(cpuToMemory, SP, PC, getMaxSystemCodeEntry());
                    SP = getMaxSystemCodeEntry() - 1;
                }
                break;

            case 25:
                /* Increment the value in X */
                PC += 1;
                X += 1;

                timer += 1;
                if (validateTimerInterrupt(interrupt, timer, kernelMode)) {
                    kernelMode = true;
                    PC = timerInterrupt(cpuToMemory, SP, PC, getMaxSystemCodeEntry());
                    SP = getMaxSystemCodeEntry() - 1;
                }
                break;

            case 26:
                /* Decrement the value in X */
                PC += 1;
                X -= 1;

                timer += 1;
                if (validateTimerInterrupt(interrupt, timer, kernelMode)) {
                    kernelMode = true;
                    PC = timerInterrupt(cpuToMemory, SP, PC, getMaxSystemCodeEntry());
                    SP = getMaxSystemCodeEntry() - 1;
                }
                break;

            case 27:
                /* Push AC onto stack */
                PC += 1;
                SP -= 1;

                if (validateAddressAccess(SP, kernelMode)) {
                    pipeAddressToStack(cpuToMemory, writeStatus, SP, AC);
                }
                else {
                    errorExit("Memory violation: accessing address in wrong mode");
                }

                timer += 1;
                if (validateTimerInterrupt(interrupt, timer, kernelMode)) {
                    kernelMode = true;
                    PC = timerInterrupt(cpuToMemory, SP, PC, getMaxSystemCodeEntry());
                    SP = getMaxSystemCodeEntry() - 1;
                }
                break;

            case 28:
                /* Pop from stack into AC */
                PC += 1;
                if (validateAddressAccess(SP, kernelMode)) {
                    pipeReadStatusAndPTR(cpuToMemory, SP, readStatus);
                    AC = readFromMemory(memoryToCPU);
                } 
                else {
                    errorExit("Memory violation: accessing address in wrong mode");
                }

                SP += 1;
                timer += 1;
                if (validateTimerInterrupt(interrupt, timer, kernelMode)) {
                    kernelMode = true;
                    PC = timerInterrupt(cpuToMemory, SP, PC, getMaxSystemCodeEntry());
                    SP = getMaxSystemCodeEntry() - 1;
                }
                break;

            case 29:
                /* Perform system call */
                kernelMode = true;
                PC += 1;
                tempSP = getMaxSystemCodeEntry();

                if (validateAddressAccess(tempSP, kernelMode)) {
                    pipeAddressToStack(cpuToMemory, writeStatus, tempSP, PC);
                }
                else {
                    errorExit("Memory violation: accessing address in wrong mode");
                }

                tempSP -= 1;
                if (validateAddressAccess(tempSP, kernelMode)) {
                    pipeAddressToStack(cpuToMemory, writeStatus, tempSP, SP);
                }
                else {
                    errorExit("Memory violation: accessing address in wrong mode");
                }
                
                SP = tempSP;
                PC = 1500;

                timer += 1;
                if (validateTimerInterrupt(interrupt, timer, kernelMode)) {
                    kernelMode = true;
                    PC = timerInterrupt(cpuToMemory, SP, PC, getMaxSystemCodeEntry());
                    SP = getMaxSystemCodeEntry() - 1;
                }
                break;

            case 30:
                /* Return from system call */
                if (validateAddressAccess(SP, kernelMode)) {
                    pipeReadStatusAndPTR(cpuToMemory, SP, readStatus);
                    tempSP = readFromMemory(memoryToCPU);
                } 
                else {
                    errorExit("Memory violation: accessing address in wrong mode");
                }

                SP += 1;
                if (validateAddressAccess(SP, kernelMode)) {
                    pipeReadStatusAndPTR(cpuToMemory, SP, readStatus);
                    PC = readFromMemory(memoryToCPU);
                } 
                else {
                    errorExit("Memory violation: accessing address in wrong mode");
                }

                kernelMode = false;
                SP = tempSP;
                
                timer += 1;
                if (validateTimerInterrupt(interrupt, timer, kernelMode)) {
                    kernelMode = true;
                    PC = timerInterrupt(cpuToMemory, SP, PC, getMaxSystemCodeEntry());
                    SP = getMaxSystemCodeEntry() - 1;
                }
                break;

            case 50:
                pipeReadStatusAndPTR(cpuToMemory, 0, 99);
                break;

            default:
                errorExit("No case!");
        }
    }
} /* end cpuProcess */

/**
 * Confirms address access based on pointer value and mode state
 * 
 * @param ptr to memory access
 * @param kernelMode current mode of CPU
 * @return true or false
 */
bool validateAddressAccess(int ptr, bool kernelMode) {
    if ((kernelMode == false && (ptr >= 0 && ptr <= 999)) ||
        (kernelMode == true && (ptr >= 1000 && ptr <= 1999)))
        return true;
    else
        return false;
} /* end */

/**
 * Confirms timer interrupt based on number of instructions processed (timer count)
 * Doesn't let timer interrupt overlap with other system call
 * 
 * @param interrupt value entered by user (or defaults to 10,000)
 * @param timer is the current number of instructions executed
 * @param kernelMode current mode of CPU
 * @return true or false
 */
bool validateTimerInterrupt(int interrupt, int timer, bool kernelMode) {
    if ((timer % interrupt == 0) && (kernelMode == false))
        return true;
    else
        return false;
} /* end */

/**
 * Returns max pointer for system code, 1999
 */
int getMaxSystemCodeEntry() {
    return 1999;
} /* end */

/**
 * Returns max pointer for user program, 999
 */
int getMaxUserProgramEntry() {
    return 999;
} /* end */

/**
 * Returns the read status value used throughout program (r = 82 on ascii table)
 */
int getReadStatus() {
    return 82;
} /* end */

/**
 * Returns the write status value used throughout program (w = 87 on ascii table)
 */
int getWriteStatus() {
    return 87;
} /* end */

/**
 * Extracts integer values from line in file
 * 
 * @param line from file
 * @return integer value
 */
int preprocessLine(char *line) {
    char *c = line;
    int i = 0;
    while (c[i] != ' ' && c[i] != '\n') {
        i += 1;
    }
    c[i] = '\0';
    return atoi(c);
} /* end */

/**
 * Returns random integer [1, 100]
 * 
 * @param n is the PC value (helps with randomness)
 * @return random integer
 */
int randomInteger(int n) {
    int max = 100;
    time_t t = n;
    srand(time(&t));
    return rand() % max + 1;
} /* end */

/**
 * Reads value from CPU process
 * 
 * @param cpuToMemory pipe
 * @return value that is read
 */
int readFromCPU(int *cpuToMemory) {
    int value;
    if (read(cpuToMemory[0], &value, sizeof(value)) == -1)
        errorExit("cpu to memory read() failed");
    return value;
} /* end */

/**
 * Reads value from memory process
 * 
 * @param memoryToCPU pipe
 * @return value that is read
 */
int readFromMemory(int *memoryToCPU) {
    int value;
    if (read(memoryToCPU[0], &value, sizeof(value)) == -1)
        errorExit("memory to cpu read() failed");
    return value;
} /* end */

/**
 * Writes PC and SP to memory (for entering kernel mode)
 * 
 * @param cpuToMemory pipe
 * @param SP stack pointer value
 * @param PC program counter value
 * @param tempSP max system code value
 * @return 1000, which is PC value
 */
int timerInterrupt(int *cpuToMemory, int SP, int PC, int tempSP) {
    int writeStatus = getWriteStatus();
    pipeAddressToStack(cpuToMemory, writeStatus, tempSP, PC);

    tempSP -= 1;
    pipeAddressToStack(cpuToMemory, writeStatus, tempSP, SP);
    return 1000;
} /* end */

/**
 * Close pipe ends
 * 
 * @param cpuToMemory pipe
 * @param memoryToCPU pipe
 * @param cpuInt used to close a cpuToMemory side
 * @param memoryInt used to close a memoryToCPU side
 */    
void closePipes(int *cpuToMemory, int *memoryToCPU, int cpuInt, int memoryInt) {
    close(cpuToMemory[cpuInt]);
    close(memoryToCPU[memoryInt]);
} /* end */

/**
 * Prints error and exits program
 */
void errorExit(char *s) {
    fprintf(stderr, "\nERROR: %s - exiting!\n\n", s);
    exit(1);
} /* end */

/**
 * Write read status and ptr to memory
 * 
 * @param cpuToMemory pipe
 * @param ptr value at address that will be read
 * @param readStatus inform memory of status
 */
void pipeReadStatusAndPTR(int *cpuToMemory, int ptr, int readStatus) {
    if (write(cpuToMemory[1], &readStatus, sizeof(readStatus)) == -1)
        errorExit("write() failed");
    
    if (write(cpuToMemory[1], &ptr, sizeof(ptr)) == -1)
        errorExit("write() failed");
} /* end */

/**
 * Pipe write status, address, value to memory process
 * 
 * @param cpuToMemory pipe
 * @param writeStatus inform memory of status
 * @param ptr address to write to
 * @param value value to write to address (ptr)
 */
void pipeAddressToStack(int *cpuToMemory, int writeStatus, int ptr, int value) {
    if (write(cpuToMemory[1], &writeStatus, sizeof(writeStatus)) == -1)
        errorExit("status, cpu to memory write() failed");

    if (write(cpuToMemory[1], &ptr, sizeof(ptr)) == -1)
        errorExit("ptr, cpu to memory write() failed");

    if (write(cpuToMemory[1], &value, sizeof(value)) == -1)
        errorExit("value, cpu to memory write() failed");
} /* end */

/**
 * Read file (integer values) into memory array
 * 
 * @param file is file name
 * @param memory is memory array
 */
void processFileInput(FILE *file, int *memory) {
    char line[256];
    int i = 0;

    /*
        If line begins with a period, change loader address (index value).
        Ignore any line that starts with a newline character or space
        Keep only integer values on line and store into memory array.
    */
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '.') {
            char *changeLoadAddress = line + 1;
            i = preprocessLine(changeLoadAddress);
        }
        else if (line[0] != '\n' && line[0] != ' ') {
            memory[i] = preprocessLine(line);
            i += 1;
        }
    }
} /* end */

/**
 * Prints value in AC (either char or int)
 * 
 * @param port determines printing of int (1) or char (2)
 * @param AC value in AC 
 */
void showAC(int port, int AC) {
    if (port == 1) {
        printf("%d", AC);
    }
    if (port == 2) {
        char charAC = AC;
        printf("%c", charAC);
    }
} /* end */

/**
 * Validates file name provided by user
 * 
 * @param memoryArray values stored in memory
 * @param fileName provided by user
 */
void validateFile(int *memoryArray, char const *fileName) {
    FILE *fp;
    if ((fp = fopen(fileName, "r")) != NULL) {
        processFileInput(fp, memoryArray);
        fclose(fp);
    } else {
        errorExit("File failed to open");
    }
} /* end */

/**
 * Pipe (write) from memory to cpu
 * 
 * @param memoryToCPU pipe
 * @param memoryArray values in memory
 * @param ptr value at this index written
 */
void writeToCPU(int *memoryToCPU, int *memoryArray, int ptr) {
    if (write(memoryToCPU[1], &memoryArray[ptr], sizeof(memoryArray[ptr])) == -1)
        errorExit("memory to cpu write() failed");
} /* end */