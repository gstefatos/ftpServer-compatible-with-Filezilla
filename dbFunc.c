#include "dbFunc.h"
sqlite3 *mDb;
char username[2048];
int connectToDatabase() {
  char *err_msg = 0;
  printf("Eftase edw\n");
  int rc = sqlite3_open("serverDB.db", &mDb);

  if (rc != SQLITE_OK) {
    printf("Eftase edw2\n");
    fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(mDb));
    sqlite3_close(mDb);
    return FAILED_DB_CREATION;
  }
char *sql = "CREATE TABLE IF NOT EXISTS Clients(id INTEGER PRIMARY KEY, username TEXT, password TEXT);";


  rc = sqlite3_exec(mDb, sql, 0, 0, &err_msg);

  if (rc != SQLITE_OK) {

    fprintf(stderr, "SQL error: %s\n", err_msg);
    sqlite3_free(err_msg);
    sqlite3_close(mDb);

    return BAD_QUERY;
  }
  return SUCCESS;
}

int registerClient(char *usernameIn,char* passwordIn)
{
    char finalQuery[1024];
    char encryptedPassword[crypto_pwhash_scryptsalsa208sha256_STRBYTES];
    memset(finalQuery,0,sizeof(finalQuery));
    encryptData(passwordIn,encryptedPassword);
    sprintf(finalQuery, "INSERT INTO Clients(username, password) VALUES ('%s', '%s');",usernameIn,encryptedPassword);
    char *err_msg = 0;
    int rc = sqlite3_exec(mDb, finalQuery, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
    }
}

int clientAuthentication(char* usernameIn, char* passwordIn)
{
    char sql[256];
    sprintf(sql, "SELECT password FROM Clients WHERE username = '%s';", username);
    sqlite3_stmt *res;
    int rc = sqlite3_prepare_v2(mDb, sql, -1, &res, 0);    
    if (rc == SQLITE_OK) 
    {
        int step = sqlite3_step(res);
        if (step == SQLITE_ROW)
        {
            const char *encryptedPass = (const char *)sqlite3_column_text(res, 0);
            if(decryptData(encryptedPass,passwordIn) == 0)
                printf("Found a user with the username %s and the given password.\n", username);
            else return -1;
        } else printf("No user found with the username %s and the given password.\n", username);
    } 
    else fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(mDb));
    sqlite3_finalize(res);
    return SUCCESS;
}

int checkUsername(char* usernameIn)
{
    printf("checkUsername: %s\n",usernameIn);
    char sql[256];
    memset(sql,0,sizeof(sql));
    sprintf(sql,"SELECT * FROM Clients WHERE username = '%s';",usernameIn);
    sqlite3_stmt *res;
    int rc = sqlite3_prepare_v2(mDb, sql, -1, &res, 0);    
    if (rc == SQLITE_OK) {
        int step = sqlite3_step(res);
        if (step == SQLITE_ROW) {
            printf("Found a user with the username %s.\n", usernameIn);
            strcpy(username,usernameIn);
            sqlite3_finalize(res);
            return SUCCESS;
        } else {
            printf("No user found with the username %s.\n", usernameIn);
            return NOT_FOUND;
        }
    } else {
        fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(mDb));
    }
return SUCCESS;
}


int encryptData(char* password,char* encryptedPasswordIn){
    // Initialize the library
    if (sodium_init() < 0) {
        printf("Error initializing libsodium.\n");
        return 1;
    }

    // Generate a random salt
    unsigned char salt[crypto_pwhash_scryptsalsa208sha256_SALTBYTES];
    randombytes(salt, sizeof salt);

    // Encrypt the password
    char encrypted_password[crypto_pwhash_scryptsalsa208sha256_STRBYTES];
    if (crypto_pwhash_scryptsalsa208sha256_str(encrypted_password, password, strlen(password), crypto_pwhash_scryptsalsa208sha256_OPSLIMIT_INTERACTIVE, crypto_pwhash_scryptsalsa208sha256_MEMLIMIT_INTERACTIVE) != 0) {
        printf("Error encrypting password.\n");
        return 1;
    }
    memcpy(encryptedPasswordIn,encrypted_password,strlen(encrypted_password));
}

int decryptData(char* encrypted_password,char* password)
{

    // Verify the password
    if (crypto_pwhash_scryptsalsa208sha256_str_verify(encrypted_password, password, strlen(password)) != 0) {
        printf("Error verifying password.\n");
        return -1;
    }

    // Password is correct
    printf("Password is correct.\n");

    return 0;
}