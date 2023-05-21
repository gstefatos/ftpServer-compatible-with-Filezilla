
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sqlite3.h>
#include <sodium.h>

int connectToDatabase();
int registerClient(char* usernameIn,char* passwordIn);
int clientAuthentication(char* usernameIn,char* passwordIn);
int checkUsername(char* usernameIn);
int encryptData(char* password,char* encryptedPasswordIn);
int decryptData(char* encrypted_password,char* password);
enum DB_RETURNS
{
    NOT_FOUND = -3,
    FAILED_DB_CREATION= -2,
    BAD_QUERY = -1,
    SUCCESS = 0,
};


