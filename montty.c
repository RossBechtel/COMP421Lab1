#include <threads.h>
#include <terminals.h>
#include <hardware.h>



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

void ReceiveInterrupt(int term) {
    (void)term;
}

void TransmitInterrupt(int term) {
    (void)term;
}

int WriteTerminal(int term, char *buf, int buflen) {
    if(buflen == 0)
        return(0);
    (void)term;
    (void)buf;
    return(buflen);
}

int ReadTerminal(int term, char *buf, int buflen) {
    (void)term;
    (void)buf;
    (void)buflen;
    return(0);
}
/**
 * This procedure should be called once and only once before any other call to the terminal device driver procedures defined above are called for terminal term.
 * Dont create threads in here
**/
int InitTerminal(int term) {
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
