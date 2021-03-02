#include <threads.h>
#include <terminals.h>
#include <hardware.h>
#include <stdio.h>
#include <string.h>

#define BUFFERSIZE 100

// Indicates if we are in a transmit interrupt cycle
static int inCycle;

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
 * Removes from the input buffer of terminal term, returning the removed 
 * character
**/
char inputRemove(int term) {
    char c = inputBuffer[term][inputOut[term]];
    inputCount[term] -= 1;
    inputOut[term] = (inputOut[term] + 1) % BUFFERSIZE;
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
    // Make sure terminal was already initialized
    if(termInitialized[term] == 0) {
        printf("Terminal %d not yet initialized!\n", term);
        return;
    }
    
    // Read the character
    char c = ReadDataRegister(term);
    
    // If not in a cycle, write to the data register 
    if(inCycle == 0) {
        inCycle = 1;
        // Return or new line handling
        if(c == '\r' || c == '\n') {
            inputAdd(term, '\n');
            currLineSize[term] = 0;
            WriteDataRegister(term, '\r');
            specialEchoAdd(term, '\n');
        // Backspace handling
        } else if(c == '\b' || c == '\177') {
            if(inputCount[term] > 0 && currLineSize[term] != 0) {
                inputRemove(term);
                currLineSize[term] -= 1;
                WriteDataRegister(term, '\b');
                specialEchoAdd(term, ' ');
                specialEchoAdd(term, '\b');
            } else {
                // Beep if there is nothing to backspace
                WriteDataRegister(term, '\a');
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
                } 
            } else {
                // Beep if input buffer is full
                WriteDataRegister(term, '\a');
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
                inputRemove(term);
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
    // Make sure terminal was already initialized
    if(termInitialized[term] == 0) {
        printf("Terminal %d not yet initialized!\n", term);
        return;
    }
    
    inCycle = 1;
    // If echo buffer has more to empty after transmission, do so
    if(specialEchoCount[term] > 0) {
        WriteDataRegister(term, specialEchoRemove(term));
    } else if(echoCount[term] > 0) {
        WriteDataRegister(term, echoRemove(term));
    } else if(specialOutputCount[term] > 0) {
        WriteDataRegister(term, specialOutputRemove(term));
    } else if(outputCount[term] > 0) {
        WriteDataRegister(term, outputRemove(term));
    } else {
        inCycle = 0;
        CondSignal(writing[term]);
    }
}

/**
 * This call should write to terminal number term, buflen characters from the buffer that starts at
address buf. The characters must be transmitted by your terminal device driver to the terminal one
at a time by calling WriteDataRegister() for each character.

Your driver must block the calling thread until the transmission of the last character of the buffer
is completed (including receiving a TransmitInterrupt following the last character); this call
should not return until then. 
**/
extern
int WriteTerminal(int term, char *buf, int buflen) {
    Declare_Monitor_Entry_Procedure();
    // Make sure terminal was already initialized
    if(termInitialized[term] == 0) {
        printf("Terminal %d not yet initialized!\n", term);
        return(-1);
    }
    // Return immediately if buflen is 0
    if(buflen == 0)
        return(0);

    int charsPlaced = 0;
    while(charsPlaced != buflen) {
        while(outputCount[term] == BUFFERSIZE) {
            CondWait(writing[term]);
        }
        if(buf[charsPlaced] == '\n') {
            outputAdd(term, '\r');
            outputAdd(term, '\n');
            charsPlaced += 1;
            currLineSize[term] = 0;
        } else {
            outputAdd(term, buf[charsPlaced]);
            charsPlaced += 1;
            currLineSize[term] += 1;
        }
    }


    // Do the first write data register
    if(inCycle == 0) {
        inCycle = 1;
        WriteDataRegister(term, outputRemove(term));
    }
    return(buflen);
}

/**
 * This call should copy characters typed from terminal number term, placing each into the buffer
beginning at address buf. As you copy characters into this buffer, continue until either buflen
characters have been copied into buf or a newline (’\n’) has been copied into buf (whichever
occurs first), but note that, as described in Section 7.2, only characters from input lines that have been
terminated by a newline character in the input buffer can be returned. Your driver should block the
calling thread until this call can be completed.
**/
extern
int ReadTerminal(int term, char *buf, int buflen) {
    Declare_Monitor_Entry_Procedure();
    // Make sure terminal was already initialized
    if(termInitialized[term] == 0) {
        printf("Terminal %d not yet initialized!\n", term);
        return(-1);
    }

    int i;
    int count;
    // Make sure buflen is non-zero
    if(buflen == 0)
        return(0);
    for(i = 0; i < buflen; i++) {
        // Make sure input buffer non-empty
        if(inputCount[term] == 0)
            break;
        // Copy to buf and add to count of num copied
        buf[i] = inputRemove(term);
        count += 1;
        // Finish if copied buflen chars or copied newline
        if(count == buflen || buf[i] == '\n')
            break;
    }
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
    if(termInitialized[term] == 1) {
        printf("Terminal %d already initialized!\n", term);
        return(-1);
    }
    termInitialized[term] = 1;
    return(InitHardware(term));
}

extern
int TerminalDriverStatistics(struct termstat *stats) {
    Declare_Monitor_Entry_Procedure();
    (void)stats;
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
    }
    driverInitialized = 1;
    printf("Driver successfully initialized.\n");
    return(0);
}