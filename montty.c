#include <threads.h>
#include <terminals.h>
#include <hardware.h>
#include <stdio.h>
#include <string.h>



/**
Provided:
hardware.h

void WriteDataRegister(int term, char c) 
This hardware operation places the character c in the output data register of the terminal identified
by the terminal number term. On any error, in this project, this function prints an error message on
stderr and terminates the program.

char ReadDataRegister(int term)
This hardware operation reads (and returns) the current contents of the input data register of the
terminal identified by term. On any error, in this project, this function prints an error message on
stderr and terminates the program.

int InitHardware(int term)
This hardware operation initializes the terminal identified by term. It must be called once and only
once before calling any of the other hardware procedures on that terminal. Returns 0 on success
or -1 on any error.

**/

#define BUFFERSIZE 100

// Indicates if we are in a transmit interrupt cycle
static int inCycle;

// Input buffer
static char inputBuffer[NUM_TERMINALS][BUFFERSIZE];
static int inputIn[NUM_TERMINALS];
static int inputOut[NUM_TERMINALS];
static int inputCount[NUM_TERMINALS];

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

// Writing/reading conditionals
cond_id_t writing[NUM_TERMINALS];
cond_id_t reading[NUM_TERMINALS];

void echoAdd(int term, char c) {
    echoBuffer[term][echoIn[term]] = c;
    echoCount[term] += 1;
    echoIn[term] = (echoIn[term] + 1) % BUFFERSIZE;
}

char echoRemove(int term) {
    char c = echoBuffer[term][echoOut[term]];
    echoCount[term] -= 1;
    echoOut[term] = (echoOut[term] + 1) % BUFFERSIZE;
    return c;
}

void inputAdd(int term, char c) {
    inputBuffer[term][inputIn[term]] = c;
    inputCount[term] += 1;
    inputIn[term] = (inputIn[term] + 1) % BUFFERSIZE;
}

char inputRemove(int term) {
    char c = inputBuffer[term][inputOut[term]];
    inputCount[term] -= 1;
    inputOut[term] = (inputOut[term] + 1) % BUFFERSIZE;
    return c;
}

void outputAdd(int term, char c) {
    outputBuffer[term][outputIn[term]] = c;
    outputCount[term] += 1;
    outputIn[term] = (outputIn[term] + 1) % BUFFERSIZE;
}

char outputRemove(int term) {
    char c = outputBuffer[term][outputOut[term]];
    outputCount[term] -= 1;
    outputOut[term] = (outputOut[term] + 1) % BUFFERSIZE;
    return c;
}


/**
 * When the receipt of a new character from a keyboard completes,
the terminal hardware signals a receive interrupt

Handle that interrupt here, pretty sure this is just signaling conditional variables
**/
extern
void ReceiveInterrupt(int term) {
    Declare_Monitor_Entry_Procedure();
    // Read the character
    char c = ReadDataRegister(term);
    // Return or new line handling
    if(c == '\r' || c == '\n') {
        inputAdd(term, '\n');
        echoAdd(term, '\r');
        echoAdd(term, '\n');
    // Backspace handling
    } else if(c == '\b' || c == '\177') {
        if(inputCount[term] > 0) {
            inputRemove(term);
            echoAdd(term, '\b');
            echoAdd(term, ' ');
            echoAdd(term, '\b');
        }
    // Any other character
    } else {
        if(inputCount[term] != BUFFERSIZE) {
            // Put that character into the input buffer
            inputAdd(term, c);
            // Put that character into the echo buffer
            echoAdd(term, c);
        }
    }
    
    // Do the first write data register
    if(inCycle == 0) {
        inCycle = 1;
        WriteDataRegister(term, echoRemove(term));
    }
    CondSignal(writing[term]);

}

/**
 * when the transmission of a character to a terminal completes, the terminal hardware
signals a transmit interrupt.

We know we just transmitted one, transmit the next
**/
extern
void TransmitInterrupt(int term) {
    Declare_Monitor_Entry_Procedure();
    inCycle = 1;
    //printf("Transmit\n");
    // If echo buffer has more to empty after transmission, do so
    if(echoCount[term] > 0) {
        WriteDataRegister(term, echoRemove(term));
    } else if(outputCount[term] > 0) {
        WriteDataRegister(term, outputRemove(term));
    } else {
        inCycle = 0;
    }
    
    CondSignal(writing[term]);
    //printf("End transmit\n");
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
    int i;
    if(buflen == 0)
        return(0);

    // Put that character into the output buffer
    for(i = 0; i < buflen; i++) {
        outputAdd(term, buf[i]);
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
 * Initializes hardware for the specified terminal
 * 
 * Input: the number terminal to be initialized
 * Output: 0 if successfully initialized, -1 if not
**/
extern
int InitTerminal(int term) {
    Declare_Monitor_Entry_Procedure();
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
    int i;
    // Set up each terminal's buffers
    for(i = 0; i < NUM_TERMINALS; i++) {
        // Init input buffer vars to 0
        inputIn[i] = 0;
        inputOut[i] = 0;
        inputCount[i] = 0;

        // Init echo buffer vars to 0
        echoIn[i] = 0;
        echoOut[i] = 0;
        echoCount[i] = 0;

        // Init output buffer vars to 0
        outputIn[i] = 0;
        outputOut[i] = 0;
        outputCount[i] = 0;

        // Create reading and writing conditionals
        writing[i] = CondCreate();
        reading[i] = CondCreate();
    }
    return(0);
}
