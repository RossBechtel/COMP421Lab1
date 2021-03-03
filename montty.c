#include <threads.h>
#include <terminals.h>
#include <hardware.h>
#include <stdio.h>
#include <string.h>

#define BUFFERSIZE 100

// Indicates if we are in a transmit interrupt cycle
static int inCycle[NUM_TERMINALS];

// Input buffer
static char inputBuffer[NUM_TERMINALS][BUFFERSIZE];
static int inputIn[NUM_TERMINALS];
static int inputOut[NUM_TERMINALS];
static int inputCount[NUM_TERMINALS];

// For keeping track of when we can backspace
static int currLineSize[NUM_TERMINALS];

// Echo buffer
static char echoBuffer[NUM_TERMINALS][BUFFERSIZE];
static int echoIn[NUM_TERMINALS];
static int echoOut[NUM_TERMINALS];
static int echoCount[NUM_TERMINALS];

// Output buffer
static char outputBuffer[NUM_TERMINALS][BUFFERSIZE];
static int outputIn[NUM_TERMINALS];
static int outputOut[NUM_TERMINALS];
static int outputCount[NUM_TERMINALS];

// Special character handling buffers
static char specialOutputBuffer[NUM_TERMINALS][2];
static int specialOutputIn[NUM_TERMINALS];
static int specialOutputOut[NUM_TERMINALS];
static int specialOutputCount[NUM_TERMINALS];

static char specialEchoBuffer[NUM_TERMINALS][2];
static int specialEchoIn[NUM_TERMINALS];
static int specialEchoOut[NUM_TERMINALS];
static int specialEchoCount[NUM_TERMINALS];

// Writing/reading conditionals
static cond_id_t writing[NUM_TERMINALS];
static cond_id_t reading[NUM_TERMINALS];

// Indicates whether a terminal has been initialized
static int termInitialized[NUM_TERMINALS];

// Indicates if the terminal driver was initialized
static int driverInitialized;

// For keeping track of terminal stats
static struct termstat termstats[NUM_TERMINALS];

/**
 * Adds to the echo buffer of terminal term the character c
**/
void echoAdd(int term, char c) {
    echoBuffer[term][echoIn[term]] = c;
    echoCount[term] += 1;
    echoIn[term] = (echoIn[term] + 1) % BUFFERSIZE;
}

/**
 * Removes from the echo buffer of terminal term, returning the removed 
 * character
**/
char echoRemove(int term) {
    char c = echoBuffer[term][echoOut[term]];
    echoCount[term] -= 1;
    echoOut[term] = (echoOut[term] + 1) % BUFFERSIZE;
    return c;
}

/**
 * Adds to the input buffer of terminal term the character c
**/
void inputAdd(int term, char c) {
    inputBuffer[term][inputIn[term]] = c;
    inputCount[term] += 1;
    inputIn[term] = (inputIn[term] + 1) % BUFFERSIZE;
}

/**
 * Removes last char from the input buffer of terminal term, returning the removed 
 * character 
**/
char inputRemove(int term) {
    char c = inputBuffer[term][inputOut[term]];
    inputCount[term] -= 1;
    inputOut[term] = (inputOut[term] + 1) % BUFFERSIZE;
    return c;
}

/**
 * Removes first char from the input buffer of terminal term, returning the removed 
 * character. Used for backspace
**/
char inputRemoveFirst(int term) {
    char c = inputBuffer[term][inputIn[term] - 1];
    inputCount[term] -= 1;
    inputIn[term] = (inputIn[term] - 1) % BUFFERSIZE;
    return c; 
}

/**
 * Adds to the output buffer of terminal term the character c
**/
void outputAdd(int term, char c) {
    outputBuffer[term][outputIn[term]] = c;
    outputCount[term] += 1;
    outputIn[term] = (outputIn[term] + 1) % BUFFERSIZE;
}

/**
 * Removes from the output buffer of terminal term, returning the removed 
 * character
**/
char outputRemove(int term) {
    char c = outputBuffer[term][outputOut[term]];
    outputCount[term] -= 1;
    outputOut[term] = (outputOut[term] + 1) % BUFFERSIZE;
    return c;
}

/**
 * Adds to special output buffer of terminal term the character c
**/
void specialOutputAdd(int term, char c) {
    specialOutputBuffer[term][specialOutputIn[term]] = c;
    specialOutputCount[term] += 1;
    specialOutputIn[term] = (specialOutputIn[term] + 1) % 2;
}

/**
 * Removes from special output buffer of terminal term, returning the removed 
 * character
**/
char specialOutputRemove(int term) {
    char c = specialOutputBuffer[term][specialOutputOut[term]];
    specialOutputCount[term] -= 1;
    specialOutputOut[term] = (specialOutputOut[term] + 1) % 2;
    return c;
}

/**
 * Adds to special echo buffer of terminal term the character c
**/
void specialEchoAdd(int term, char c) {
    specialEchoBuffer[term][specialEchoIn[term]] = c;
    specialEchoCount[term] += 1;
    specialEchoIn[term] = (specialEchoIn[term] + 1) % 2;
}

/**
 * Removes from special echo buffer of terminal term, returning the removed 
 * character
**/
char specialEchoRemove(int term) {
    char c = specialEchoBuffer[term][specialEchoOut[term]];
    specialEchoCount[term] -= 1;
    specialEchoOut[term] = (specialEchoOut[term] + 1) % 2;
    return c;
}


/**
 * When the receipt of a new character from a keyboard completes,
 * the terminal hardware signals a receive interrupt. This function reads a
 * character from the data register, processes that character appropriately,
 * adding it to the input and echo buffers of terminal term and starts the 
 * cycle of transmit interrupts if one is not already in motion.
**/
extern
void ReceiveInterrupt(int term) {
    Declare_Monitor_Entry_Procedure();
    // Make sure driver was initialized
    if(driverInitialized == 0) {
        printf("Driver must be initialized first!\n");
        return;
    }
    // Make sure terminal was already initialized
    if(termInitialized[term] == 0) {
        printf("Terminal %d not yet initialized!\n", term);
        return;
    }
    
    // Read the character
    char c = ReadDataRegister(term);
    termstats[term].tty_in += 1;
    
    // If not in a cycle, write to the data register 
    if(inCycle[term] == 0) {
        inCycle[term] = 1;
        // Return or new line handling
        if(c == '\r' || c == '\n') {
            inputAdd(term, '\n');
            currLineSize[term] = 0;
            WriteDataRegister(term, '\r');
            termstats[term].tty_out += 1;
            specialEchoAdd(term, '\n');
        // Backspace handling
        } else if(c == '\b' || c == '\177') {
            if(inputCount[term] > 0 && currLineSize[term] != 0) {
                inputRemoveFirst(term);
                currLineSize[term] -= 1;
                WriteDataRegister(term, '\b');
                termstats[term].tty_out += 1;
                specialEchoAdd(term, ' ');
                specialEchoAdd(term, '\b');
            } else {
                // Beep if there is nothing to backspace
                WriteDataRegister(term, '\a');
                termstats[term].tty_out += 1;
            }
        // Any other character
        } else {
            // Check if input buffer not full and add to input
            if(inputCount[term] != BUFFERSIZE) {
                inputAdd(term, c);
                currLineSize[term] += 1; 
                // Put that character into the echo buffer if not full
                if(echoCount[term] != BUFFERSIZE) {
                    WriteDataRegister(term, c);
                    termstats[term].tty_out += 1;
                } 
            } else {
                // Beep if input buffer is full
                WriteDataRegister(term, '\a');
                termstats[term].tty_out += 1;
            }
        }
        // Finally, signal that we just wrote
        CondSignal(writing[term]);

    // If not in a cycle, just add to buffers for later writing
    } else {
        // Return or new line handling
        if(c == '\r' || c == '\n') {
            inputAdd(term, '\n');
            currLineSize[term] = 0;
            echoAdd(term, '\r');
            specialEchoAdd(term, '\n');
        // Backspace handling
        } else if(c == '\b' || c == '\177') {
            if(inputCount[term] > 0 && currLineSize[term] != 0) {
                inputRemoveFirst(term);
                currLineSize[term] -= 1;
                echoAdd(term, '\b');
                specialEchoAdd(term, ' ');
                specialEchoAdd(term, '\b');
            } else {
                // Beep if there is nothing to backspace
                echoAdd(term, '\a');
            }
        // Any other character
        } else {
            // Check if input buffer not full and add to input
            if(inputCount[term] != BUFFERSIZE) {
                inputAdd(term, c);
                currLineSize[term] += 1; 
                // Put that character into the echo buffer if not full
                if(echoCount[term] != BUFFERSIZE) {
                    echoAdd(term, c);
                } 
            } else {
                // Beep if input buffer is full
                echoAdd(term, '\a');
            }
        }
    }
}

/**
 * When the transmission of a character to a terminal completes, the terminal 
 * hardware signals a transmit interrupt. This function writes to the data
 * register of the terminal term.
**/
extern
void TransmitInterrupt(int term) {
    Declare_Monitor_Entry_Procedure();
    // Make sure driver was initialized
    if(driverInitialized == 0) {
        printf("Driver must be initialized first!\n");
        return;
    }
    // Make sure terminal was already initialized
    if(termInitialized[term] == 0) {
        printf("Terminal %d not yet initialized!\n", term);
        return;
    }
    
    inCycle[term] = 1;
    // Now write what we can in the following order:
    // First, the special character echo buffer
    if(specialEchoCount[term] > 0) {
        WriteDataRegister(term, specialEchoRemove(term));
        termstats[term].tty_out += 1;
    // Next, the normal echo buffer
    } else if(echoCount[term] > 0) {
        WriteDataRegister(term, echoRemove(term));
        termstats[term].tty_out += 1;
    // Next, the special character output buffer
    } else if(specialOutputCount[term] > 0) {
        WriteDataRegister(term, specialOutputRemove(term));
        termstats[term].tty_out += 1;
    // Finally, the normal output buffer
    } else if(outputCount[term] > 0) {
        char c = outputRemove(term);
        if(c == '\n') {
            specialOutputAdd(term, '\n');
            WriteDataRegister(term, '\r');
            termstats[term].tty_out += 1;
        } else {
            WriteDataRegister(term, c);
            termstats[term].tty_out += 1;
        }
    // All buffers were empty, indicate that we have left the cycle and signal
    } else {
        inCycle[term] = 0;
        CondSignal(writing[term]);
    }
}

/**
 * Writes to terminal number term, buflen characters from the buffer that starts 
 * at address buf, using an stored output buffer for similarity to other
 * processes.
**/
extern
int WriteTerminal(int term, char *buf, int buflen) {
    Declare_Monitor_Entry_Procedure();
    // Make sure driver was initialized
    if(driverInitialized == 0) {
        printf("Driver must be initialized first!\n");
        return(-1);
    }
    // Make sure terminal was already initialized
    if(termInitialized[term] == 0) {
        printf("Terminal %d not yet initialized!\n", term);
        return(-1);
    }
    // Make sure buflen is > -1
    if (buflen < 0) {
        printf("Buflen must be greater than or equal to 0!\n");
        return(-1);
    }
    // Make sure buf is not null
    if (buf == NULL) {
        printf("Buffer cannot be null! Input a valid buffer!\n");
        return(-1);
    }
    // Return immediately if buflen is 0
    if(buflen == 0)
        return(0);

    int charsPlaced = 0;
    // Loop until exhausted the buffer
    while(charsPlaced != buflen) {
        // Wait until output buffer not full
        while(outputCount[term] == BUFFERSIZE) {
            CondWait(writing[term]);
        }
        // Not in a cycle so we can write now
        if(inCycle[term] == 0) {
            inCycle[term] = 1;
            if(buf[charsPlaced] == '\n') {
                WriteDataRegister(term, '\r');
                termstats[term].tty_out += 1;
                specialOutputAdd(term, '\n');
                charsPlaced += 1;
                currLineSize[term] = 0;
            } else {
                WriteDataRegister(term, buf[charsPlaced]);
                termstats[term].tty_out += 1;
                charsPlaced += 1;
                currLineSize[term] += 1;
            }
        // In a cycle so just add to buffers
        } else {
            outputAdd(term, buf[charsPlaced]);
            charsPlaced += 1;
        }
    }
    termstats[term].user_in += charsPlaced;
    return(charsPlaced);
}

/**
 * Reads characters from terminal term, copying them to the input buf
 * until reaching a new line or until buflen characters have been copied.
 * Returns the number of characters copied to buf.
**/
extern
int ReadTerminal(int term, char *buf, int buflen) {
    Declare_Monitor_Entry_Procedure();
    // Make sure driver was initialized
    if(driverInitialized == 0) {
        printf("Driver must be initialized first!\n");
        return(-1);
    }
    // Make sure terminal was already initialized
    if(termInitialized[term] == 0) {
        printf("Terminal %d not yet initialized!\n", term);
        return(-1);
    }
    // Make sure buflen is > -1
    if (buflen < 0) {
        printf("Buflen must be greater than or equal to 0!\n");
        return(-1);
    }
    // Make sure buf is not null
    if (buf == NULL) {
        printf("Buffer cannot be null! Input a valid buffer!\n");
        return(-1);
    }
    int count;
    // Make sure buflen is non-zero
    if(buflen == 0)
        return(0);
    while(count < buflen) {
        // Make sure input buffer non-empty, wait until its not
        while(inputCount[term] == 0)
            CondWait(writing[term]);
        // Copy to buf and add to count of num copied
        buf[count] = inputRemove(term);
        count += 1;
        // Finish if copied newline
        if(buf[count - 1] == '\n') {
            break;
        }  
    }
    termstats[term].user_out += count;
    return(count);
}
/**
 * Initializes hardware for the specified terminal term
 * 
 * Output: 0 if successfully initialized, -1 if not
**/
extern
int InitTerminal(int term) {
    Declare_Monitor_Entry_Procedure();
    // Make sure driver was initialized
    if(driverInitialized == 0) {
        printf("Driver must be initialized first!\n");
        return(-1);
    }
    // Make sure its a valid terminal number
    if(term >= NUM_TERMINALS || term < 0) {
        printf("Invalid terminal number, choose between 0 and %d\n", NUM_TERMINALS);
    }
    // Make sure terminal wasn't initialized
    if(termInitialized[term] == 1) {
        printf("Terminal %d already initialized!\n", term);
        return(-1);
    }
    // Init stats to 0
    termstats[term].tty_in = 0;
    termstats[term].tty_out = 0;
    termstats[term].user_in = 0;
    termstats[term].user_out = 0;
    // Indicate that the terminal has been initialized
    termInitialized[term] = 1;
    return(InitHardware(term));
}

/**
 * Returns a consistent snapshot of the I/O statistics for all terminals all at
 * once. Copies the statistics from internal memory to the inputted
 * stats struct.
**/
extern
int TerminalDriverStatistics(struct termstat *stats) {
    Declare_Monitor_Entry_Procedure();
    // Make sure driver was initialized
    if(driverInitialized == 0) {
        printf("Driver must be initialized first!");
        return(-1);
    }
    // Make sure stats is not null
    if (stats == NULL) {
        printf("Stats cannot be null! Input a valid stats struct!\n");
        return(-1);
    }
    int i;
    for(i = 0; i < NUM_TERMINALS; i++) {
        stats[i].tty_in = termstats[i].tty_in;
        stats[i].tty_out = termstats[i].tty_out;
        stats[i].user_in = termstats[i].user_in;
        stats[i].user_out = termstats[i].user_out;
    }
    return(0);
}

/**
 * Initializes all buffers and their statistics as well as conditional variables
 * Should be called before anything else
 * 
 * Return: 0 on successful initializatin of the driver
**/
extern
int InitTerminalDriver() {
    // Make sure driver isn't initialized twice
    if(driverInitialized == 1) {
        printf("Driver already initialized!\n");
        return(-1);
    }  
    int i;
    // Set up each terminal's buffers
    for(i = 0; i < NUM_TERMINALS; i++) {
        // Init input buffer vars to 0
        inputIn[i] = 0;
        inputOut[i] = 0;
        inputCount[i] = 0;

        // Init curr line size vars to 0
        currLineSize[i] = 0;

        // Init echo buffer vars to 0
        echoIn[i] = 0;
        echoOut[i] = 0;
        echoCount[i] = 0;

        // Init output buffer vars to 0
        outputIn[i] = 0;
        outputOut[i] = 0;
        outputCount[i] = 0;

        // Init special buffer vars to 0
        specialOutputIn[i] = 0;
        specialOutputOut[i] = 0;
        specialOutputCount[i] = 0;

        specialEchoIn[i] = 0;
        specialEchoOut[i] = 0;
        specialEchoCount[i] = 0;

        // Create reading and writing conditionals
        writing[i] = CondCreate();
        reading[i] = CondCreate();

        // Set each terminal as unitialized
        termInitialized[i] = 0;

        // Indicate that each terminal is not in a cycle
        inCycle[i] = 0;

        // Init stats
        termstats[i].tty_in = -1;
        termstats[i].tty_out = -1;
        termstats[i].user_in = -1;
        termstats[i].user_out = -1;
    }
    // Indicate that the driver has been initialized
    driverInitialized = 1;
    printf("Driver successfully initialized.\n");
    return(0);
}