#ifndef CPU_MEM_SIM_H_
#define CPU_MEM_SIM_H_

bool validateAddressAccess(int ptr, bool kernelMode);
bool validateTimerInterrupt(int interrupt, int timer, bool kernelMode);

int getMaxSystemCodeEntry();
int getMaxUserProgramEntry();
int getReadStatus();
int getWriteStatus();
int preprocessLine(char *line);
int randomInteger(int n);
int readFromCPU(int *cpuToMemory);
int readFromMemory(int *memoryToCPU);
int timerInterrupt(int *cpuToMemory, int SP, int PC, int tempSP);

void closePipes(int *cpuToMemory, int *memoryToCPU, int cpuInt, int memoryInt);
void cpuProcess(int *cpuToMemory, int *memoryToCPU, int interrupt);
void errorExit(char *s);
void memoryProcess(int *cpuToMemory, int *memoryToCPU, char const *fileName);
void showAC(int port, int AC);
void pipeAddressToStack(int *cpuToMemory, int writeStatus, int SP, int PC);
void pipeReadStatusAndPTR(int *cpuToMemory, int PC, int readStatus);
void processFileInput(FILE *fp, int *memory);
void validateFile(int *memoryArray, char const *fileName);
void writeToCPU(int *memoryToCPU, int *memoryArray, int ptr);

#endif