#include <threads.h>
#include <terminals.h>
#include <hardware.h>

void WriteDataRegister(int term, char c) {
    (void)term;
    (void)c;
}

char ReadDataRegister(int term) {
    (void)term;
    return('0');
}

int InitHardware(int term) {
    (void)term;
    return(0);
}

void ReceiveInterrupt(int term) {
    (void)term;
}

void TransmitInterrupt(int term) {
    (void)term;
}

int WriteTerminal(int term, char *buf, int buflen) {
    (void)term;
    (void)buf;
    (void)buflen;
    return(0);
}

int ReadTerminal(int term, char *buf, int buflen) {
    (void)term;
    (void)buf;
    (void)buflen;
    return(0);
}

int InitTerminal(int term) {
    (void)term;
    return(0);
}

int TerminalDriverStatistics(struct termstat *stats) {
    (void)stats;
    return(0);
}

int InitTerminalDriver() {
    return(0);
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    return(0);
}