# CPU Memory Simulation

A program that simulates basic CPU and memory interaction. The CPU and memory run as separate processes that communicate through pipes.

## Description

The program reads input into the memory process that the CPU process then executes. It does pseudo low-level processing. That is, it simulates stack processing, procedure calls, system calls, interrupt handling, and memory protection.

The memory process holds a 2000 length integer array: 0 - 999 for the user program; 1000 - 1999 for system code. It has two operations: reading a value at an address and writing a value to an address. The array holds the pseudo program that the CPU process executes.

The CPU process fetches a single instruction from the memory process. Instructions are read into a pseudo IR (instruction register). The CPU executes each instruction before fetching the next one. The CPU process has a pseudo instruction set, consisting of 31 different instructions. The user stack begins at the end of user memory (999) and grows down, while the system stack begins at the end of system memory (1999) and also grows down.

The program has two forms of interrupts: timer and system call. When both interrupts occur, the program enters kernel mode, but only one interrupt can occur at a time (for simplicity). A series of things happen when kernel mode is entered. The stack pointer (SP) switches to the system stack. The CPU process saves the stack pointer and program counter onto the system stack, so once the interrupt is handled, these values can be restored. A timer interrupt happens after some number of instructions. The user determines this value of instructions from a command line entry (the default is 10000).

Interrupts are checked after an instruction executes.

### Instruction Cycle with Interrupts

![instruction_cycle](https://github.com/charlesdungy/cpu-memory-simulation/blob/main/examples/instruction_cycle_a.png?raw=true)

More details can be found in the project description document (not yet added).

This was a school project. Four sample input files came with it to demo. No source files were included.

## Demo

This is a demo of the four different input files that are staged in examples.

![demo](https://raw.githubusercontent.com/charlesdungy/cpu-memory-simulation/main/examples/output.gif)

#### FWIW

This is how I created the above gif, using FFmpeg. I created the cpusim.mov file with a screen recording.

```bash 
$ ffmpeg -i cpusim.mov -vf "crop=510:in_h:342:0" output.gif
```

## License

MIT