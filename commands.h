#ifndef COMMANDS_H
#define COMMANDS_H
#define NUMBER_OF_COMMANDS 14
typedef enum 
{
    PWD=0,
    CWD,
    LIST,
    RETR,
    DELE,
    MKD,
    RMD,
    PORT,
    QUIT
}Commands;
extern const char* mCmd[NUMBER_OF_COMMANDS];

typedef struct 
{
    char  username[256];
    char  password[256];
    int data_socket;
    char current_working_dir[2048];
}Session;
struct COMMANDS
{
    char *name;
    int (*commandFunc)(int ,char*,Session*);
};

#endif