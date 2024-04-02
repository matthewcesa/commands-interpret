#ifndef ANALYS_H
#define ANALYS_H


char *getEntreS();
void analys ();
int pwd();
int cd(char *chemin);
int exit_call(int val);
int creerJobEnArrierePlan();
int lancerBackground();
int kill_command(int signal, int pid);
void getArg();
void last_call();
void background_cmd();
void getRed(char *buf);
int redirections(char *c, char *fic);
void redirectionsInt(char *c, char *fic);
void redirInterne();
struct job numJobToJob(int process);
void verifieFin(int job);
void verifSIGTERM();
void pipeline(char * buf);
void subProcessus(char  * buf);
int bg_cmd();
int foreground_cmd();



#endif