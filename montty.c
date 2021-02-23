#include <threads.h>
#include <terminals.h>
#include <hardware.h>
#include <stdio.h>



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

static int inCycle;

char inputBuffer[100];
int inputStart = 0;
int inputEnd = 0;

char echoBuffer[100];
int echoStart = 0;
int echoEnd = 0;
int echoCount = 0;

static cond_id_t writing;


/**
 * When the receipt of a new character from a keyboard completes,
the terminal hardware signals a receive interrupt

Handle that interrupt here, pretty sure this is just signaling conditional variables
**/
void ReceiveInterrupt(int term) {
    Declare_Monitor_Entry_Procedure();
    printf("Rec\n");
    // Read the character
    char c = ReadDataRegister(term);
    // Put that character into the input buffer
    inputBuffer[inputStart] = c;
    inputStart += 1;
    // Put that character into the echo buffer
    echoBuffer[echoStart] = c;
    echoCount += 1;

    // Make sure no one else is writing
    //CondWait(writing);
    // Do the first write data register
    if(inCycle == 0) {
        inCycle = 1;
        WriteDataRegister(term, echoBuffer[echoStart]);
        echoCount -= 1;
        echoStart += 1;
    }
    CondSignal(writing);

}

/**
 * when the transmission of a character to a terminal completes, the terminal hardware
signals a transmit interrupt.

We know we just transmitted one, transmit the next
**/
void TransmitInterrupt(int term) {
    Declare_Monitor_Entry_Procedure();
    inCycle = 1;
    printf("Transmit\n");
    // If echo buffer has more to empty after transmission, do so
    if(echoCount > 0) {
        printf("%d\n", echoCount);
        WriteDataRegister(term, echoBuffer[echoStart]);
        echoCount -= 1;
        printf("%d\n", echoCount);
        echoStart += 1;
    } else {
        inCycle = 0;
    }
    
    CondSignal(writing);
    printf("End transmit\n");
}

/**
 * This call should write to terminal number term, buflen characters from the buffer that starts at
address buf. The characters must be transmitted by your terminal device driver to the terminal one
at a time by calling WriteDataRegister() for each character.

Your driver must block the calling thread until the transmission of the last character of the buffer
is completed (including receiving a TransmitInterrupt following the last character); this call
should not return until then. 
**/
int WriteTerminal(int term, char *buf, int buflen) {
    Declare_Monitor_Entry_Procedure();
    int i;
    if(buflen == 0)
        return(0);
    for(i = 0; i < buflen; i++) {
        WriteDataRegister(term, buf[i]);
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
int ReadTerminal(int term, char *buf, int buflen) {
    (void)term;
    (void)buf;
    (void)buflen;
    return(0);
}
/**
 * This procedure should be called once and only once before any other call to the terminal device driver procedures defined above are called for terminal term.
 * Dont create threads in 
**/
int InitTerminal(int term) {
    writing = CondCreate();
    Declare_Monitor_Entry_Procedure();
    
    return(InitHardware(term));
}

int TerminalDriverStatistics(struct termstat *stats) {
    (void)stats;
    return(0);
}

int InitTerminalDriver() {
    return(0);
}
