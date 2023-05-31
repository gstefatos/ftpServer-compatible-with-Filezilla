#include "commands.h"
#include <arpa/inet.h>
#include <ctype.h>
#include <dirent.h>
#include <grp.h>
#include <netinet/in.h>
#include <pthread.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "dbFunc.h"
#include "colors.h"

int handle_USER_Command(int client_socket, char* bufferIn,Session* state);
int handle_PASS_Command(int client_socket, char* bufferIn,Session* state);
int handle_LIST_Command(int client_socket, char* bufferIn,Session* state);
int handle_PWD_Command(int client_socket, char* bufferIn,Session* state);
int handle_CWD_Command(int client_socket, char* bufferIn,Session* state);
int handle_PORT_Command(int client_socket, char* bufferIn,Session* state);
int handle_DELE_Command(int client_socket, char* bufferIn,Session* state);
int handle_RETR_Command(int client_socket, char* bufferIn,Session* state);
int handle_STOR_Command(int client_socket, char* bufferIn,Session* state);
int handle_MKD_Command(int client_socket, char* bufferIn,Session* state);
int handle_TYPE_Command(int client_socket, char* bufferIn,Session* state);

struct COMMANDS mComands[] = 
{
  {"USER",handle_USER_Command,NULL},
  {"PASS",handle_PASS_Command,NULL},
  {"PORT",handle_PORT_Command,NULL},
  {"LIST",handle_LIST_Command,NULL},
  {"PWD",handle_PWD_Command,NULL},
  {"CWD",handle_CWD_Command,NULL},
  {"DELE",handle_DELE_Command,NULL},
  {"RETR",handle_RETR_Command,NULL},
  {"STOR",handle_STOR_Command,NULL},
  {"MKD",handle_MKD_Command,NULL},
  {"TYPE",handle_TYPE_Command}
};
extern sqlite3 *mDb;
#define BUFFER_SIZE 2048
int port  = 0;

void handle_client(void *client_fdIn);
void handleCommands(int client_fdIn, char *commandIn, char *bufferIn);
int create_data_connection(unsigned char ip[4], int port);
void file_mode_string(mode_t mode, char *str) {
  static const char *rwx[] = {"---", "--x", "-w-", "-wx",
                              "r--", "r-x", "rw-", "rwx"};

  str[0] = S_ISDIR(mode) ? 'd' : '-';
  strcpy(&str[1], rwx[(mode >> 6) & 7]);
  strcpy(&str[4], rwx[(mode >> 3) & 7]);
  strcpy(&str[7], rwx[mode & 7]);

  if (mode & S_ISUID)
    str[3] = (mode & S_IXUSR) ? 's' : 'S';
  if (mode & S_ISGID)
    str[6] = (mode & S_IXGRP) ? 's' : 'S';

  str[10] = '\0';
}
void removeSpaces(char *str) {
  char *i = str;
  char *j = str;
  while (*j != '\0') {
    if (!isspace(*j)) {
      *i = *j;
      i++;
    }
    j++;
  }
  *i = '\0';
}

int main(int argc, char *argv[]) {
  int server_fd, client_fd;
  struct sockaddr_in server_addr, client_addr;
  if(argc !=2)
  {
    printf("Please enter Port number\n");
    return;
  }
  if(argv[1]!= NULL)
  port = atoi(argv[1]);
  connectToDatabase();
  // registerClient("makis","makis");
  // clientAuthentication("eleni","eleni");
  // Create a socket
  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    perror("socket");
    exit(EXIT_FAILURE);
  }

  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(port);

  // Bind the socket
  if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) <
      0) {
    perror("bind");
    exit(EXIT_FAILURE);
  }

  // Listen for connections
  if (listen(server_fd, 1) < 0) {
    perror("listen");
    exit(EXIT_FAILURE);
  }

  socklen_t addr_len = sizeof(client_addr);

  while (1) {
    // Accept a connection
    printf(YELLOW);
    printf("Waiting for incoming connection\n");
    printf(RESET);
    if ((client_fd = accept(server_fd, (struct sockaddr *)&client_addr,
                            &addr_len)) < 0) {
      perror("accept");
      exit(EXIT_FAILURE);
    }
    printf(RESET);
    printf(GREEN);
    printf("Connection accepted from %s:%d\n", inet_ntoa(client_addr.sin_addr),
           ntohs(client_addr.sin_port));
           printf(RESET);
    pthread_t clientThread;
    pthread_create(&clientThread, NULL, handle_client, (void *)client_fd);
    // handle_client(client_fd);

    // Close the connection
    // close(client_fd);
  }

  close(server_fd);
  return 0;
}
int isValidCommand(char *commandIn) {
  for (int i = 0; i < NUMBER_OF_COMMANDS; i++)
    if (strncmp(commandIn, mCmd[i], strlen(mCmd[i])) == 0)
      return 1;
  return -1;
}
int create_data_connection(unsigned char ip[4], int port) {
  int data_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (data_socket < 0) {
    perror("socket");
    return -1;
  }

  struct sockaddr_in client_addr;
  client_addr.sin_family = AF_INET;
  client_addr.sin_port = htons(port);
  client_addr.sin_addr.s_addr =
      htonl((ip[0] << 24) | (ip[1] << 16) | (ip[2] << 8) | ip[3]);

  if (connect(data_socket, (struct sockaddr *)&client_addr,
              sizeof(client_addr)) < 0) {
    perror("connect");
    close(data_socket);
    return -1;
  }

  return data_socket;
}

void handle_client(void *client_fdIn) {
  int client_fd = (int)client_fdIn;
  printf("Client %d",client_fd);
  char buffer[BUFFER_SIZE];
  memset(buffer,0,sizeof(buffer));
  int read_bytes;
  int logged_in = 0;
  int data_socket;
  char mCwd[BUFFER_SIZE];
  getcwd(mCwd, sizeof(mCwd));

  // Send welcome message
  snprintf(buffer, BUFFER_SIZE, "220 Welcome to the simple FTP server.\r\n");
  send(client_fd, buffer, strlen(buffer), 0);

  Session state;
  memset(&state,0,sizeof(Session));
  while ((read_bytes = recv(client_fd, buffer, BUFFER_SIZE, 0)) > 0) {
    // Ensure the buffer is null-terminated
    buffer[read_bytes] = '\0';
    char cmd[5];
    sscanf(buffer, "%4s", cmd);
    if (isValidCommand(cmd) != -1)
      {
        for(int i=0;i<11;++i)
        {
           if (strcmp(cmd, mComands[i].name) == 0) {
            mComands[i].commandFunc(client_fd,buffer,&state);
            break;
        }
        }
      }
      // handleCommands(client_fd, cmd, buffer);
    else {
      snprintf(buffer, BUFFER_SIZE, "500 Unknown command.\r\n");
      send(client_fd, buffer, strlen(buffer), 0);
    }
  }
}

int handle_USER_Command(int client_socket,char* bufferIn,Session* state)
{
char response[256];
memset(response,0,sizeof(response));
strncpy(state->username,bufferIn+5,strlen(bufferIn+4));
state->username[strlen(state->username)] = '\0';
removeSpaces(state->username);
printf("USER Command:%s\n",state->username);
if (checkUsername(state->username) == NOT_FOUND) {
  snprintf(response, strlen("530 Not logged in. \r\n"), "530 Not logged in. \r\n");
  send(client_socket, response, strlen(response), 0);
  return -1;
}
snprintf(response, strlen("331 User name okay, need password.\r\n"), "331 User name okay, need password.\r\n");
send(client_socket, response, strlen(response), 0);
return 0;
}
int handle_PASS_Command(int client_socket,char* bufferIn,Session* state)
{
  if(state == NULL)printf("null re\n");
    strncpy(state->password,bufferIn+4,strlen(bufferIn+4));
    removeSpaces(state->password);
    printf("Username is :%s and Password is:%s\n",state->username,state->password);
    if(clientAuthentication(state->username,state->password) == 0)snprintf(bufferIn, BUFFER_SIZE, "230 User logged in, proceed.\r\n");
    else snprintf(bufferIn, BUFFER_SIZE, "530 Not logged in.\r\n");
    send(client_socket, bufferIn, strlen(bufferIn), 0);
    return 0;
}

int handle_PWD_Command(int client_socket,char* bufferIn,Session* state)
{
  char cwd[BUFFER_SIZE];
  getcwd(cwd, sizeof(cwd));
  strcpy(state->current_working_dir,cwd);
  printf("Current WOrking Dir:%s\n",state->current_working_dir);
  snprintf(bufferIn, BUFFER_SIZE, "257 \"%s\" is the current directory.\r\n",
            cwd);
  send(client_socket, bufferIn, strlen(bufferIn), 0);
  return 0;
}

int handle_CWD_Command(int client_socket,char* bufferIn,Session* state)
{
  char tmpBuffer[BUFFER_SIZE];
  char tmp[BUFFER_SIZE];
  memset(tmpBuffer, 0, BUFFER_SIZE);
  memset(state->current_working_dir, 0, sizeof(state->current_working_dir));
  strncpy(state->current_working_dir, bufferIn + 4, strlen(bufferIn + 4));
  state->current_working_dir[strlen(state->current_working_dir)] = '\0';
  char *cr = strchr(state->current_working_dir, '\r');
  if (cr != NULL)
    *cr = '\0';
  if (chdir(state->current_working_dir) == -1)
    perror("chdir");
    printf("CWD %s\n",state->current_working_dir);
    snprintf(tmpBuffer, BUFFER_SIZE,"250 Directory successfully changed to %s.\r\n", state->current_working_dir);
  send(client_socket, tmpBuffer, strlen(tmpBuffer), 0);
}

int handle_DELE_Command(int client_socket,char* bufferIn,Session* state)
{
  printf("Deletion Command\n");
  char fileToDelete[BUFFER_SIZE];
  char finalFile[BUFFER_SIZE];
  strncpy(fileToDelete, bufferIn + 4, strlen(bufferIn + 4));
  char *cr = strchr(fileToDelete, '\r');
  if (cr != NULL)
    *cr = '\0';
  sprintf(finalFile, "%s%s%s", state->current_working_dir, "/", fileToDelete);
  removeSpaces(finalFile);
  printf("File to Delete %s\n", finalFile);
  int res = remove(finalFile);
  printf("Result %d\n", res);
  sprintf(bufferIn, "250 File deleted successfully.\r\n");
  send(client_socket, bufferIn, strlen(bufferIn), 0);
}

int handle_MKD_Command(int client_socket, char* bufferIn,Session* state)
{
  char finalDir[BUFFER_SIZE];
  char *response = "550 Failed to create directory.\r\n";
  sprintf(finalDir, "%s%s%s", state->current_working_dir, "/", bufferIn + 4);
  removeSpaces(finalDir);
  if (mkdir(finalDir, 0777) == 0)
    response = "250 Directory created successfully.\r\n";
  else
    response = "550 Failed to create directory.\r\n";
  send(client_socket, response, strlen(response), 0);
}

int handle_LIST_Command(int client_socket,char* bufferIn,Session* state)
{
 DIR *dir;
    struct dirent *entry;
    char entry_buffer[BUFFER_SIZE];
    dir = opendir(state->current_working_dir);
    if (dir == NULL) {
      perror("opendir");
      snprintf(bufferIn, BUFFER_SIZE, "550 Failed to open directory.\r\n");
      send(client_socket, bufferIn, strlen(bufferIn), 0);
      close(state->data_socket);
    } else {
      snprintf(bufferIn, BUFFER_SIZE,
               "150 Opening ASCII mode data connection for file list.\r\n");
      send(client_socket, bufferIn, strlen(bufferIn), 0);

      while ((entry = readdir(dir)) != NULL) {
        struct stat file_stat;
        char filepath[BUFFER_SIZE];
        snprintf(filepath, BUFFER_SIZE, "%s/%s", state->current_working_dir, entry->d_name);
        if (stat(filepath, &file_stat) == 0) {
          char file_mode[11];
          file_mode_string(file_stat.st_mode, file_mode);
          struct passwd *user_info = getpwuid(file_stat.st_uid);
          struct group *group_info = getgrgid(file_stat.st_gid);
          char time_buffer[80];
          strftime(time_buffer, sizeof(time_buffer), "%b %d %H:%M",
                   localtime(&(file_stat.st_mtime)));

          snprintf(entry_buffer, BUFFER_SIZE, "%s %ld %s %s %lld %s %s\r\n",
                   file_mode, (long)file_stat.st_nlink, user_info->pw_name,
                   group_info->gr_name, (long long)file_stat.st_size,
                   time_buffer, entry->d_name);

          send(state->data_socket, entry_buffer, strlen(entry_buffer), 0);
        }
      }
      snprintf(bufferIn, BUFFER_SIZE, "226 Transfer complete.\r\n");
      send(client_socket, bufferIn, strlen(bufferIn), 0);
      closedir(dir);
      close(state->data_socket);
    }
}


int handle_RETR_Command(int client_socket,char* bufferIn,Session* state)
{
  printf("Download Command\n");
  char fileName[BUFFER_SIZE];
  char filePath[BUFFER_SIZE];
  memset(fileName, 0, BUFFER_SIZE);
  memset(filePath, 0, BUFFER_SIZE);
  strncpy(fileName, bufferIn + 4, strlen(bufferIn + 4));
  sprintf(filePath, "%s%s%s", state->current_working_dir, "/", fileName);
  removeSpaces(filePath);
  FILE *fp = fopen(filePath, "rb");
  if (fp == NULL)
    perror("Fopen");
  printf("FilePath:%s\n", filePath);
  // Read and send file data
  char fileBuffer[BUFFER_SIZE];
  size_t bytes_read;
  while ((bytes_read = fread(fileBuffer, 1, BUFFER_SIZE, fp)) > 0) {
    int sended = send(state->data_socket, fileBuffer, bytes_read, 0);
    printf("Bytes to read %d\n", sended);
  }
  fclose(fp);
  const char *response = "226 Transfer complete.\r\n";
  send(client_socket, response, strlen(response), 0);
  printf("Upload Finished\n");
  close(state->data_socket);
}
int handle_STOR_Command(int client_socket,char* bufferIn,Session* state)
{
    printf("Upload Command\n");
    char fileName[BUFFER_SIZE];
    char filePath[BUFFER_SIZE];
    const char *response ;
    int read_bytes;
    memset(fileName,0, BUFFER_SIZE);  
    memset(filePath,0, BUFFER_SIZE);
    strncpy(fileName,bufferIn+4,strlen(bufferIn+4));
    sprintf(filePath, "%s%s%s",state->current_working_dir,"/",fileName);
    removeSpaces(filePath);
    printf("FilePath:%s\n",filePath);
    FILE * fp = fopen(filePath,"wb");
    if(fp == NULL)
    {
      printf("Unable to open the file\n");
      return;
    }
    response = "150 Ok to send data.\r\n";
    send(client_socket, response, strlen(response), 0);
    while((read_bytes = recv(state->data_socket, bufferIn, BUFFER_SIZE, 0)) > 0)fwrite(bufferIn,1,read_bytes,fp);
    response = "226 Transfer complete. \r\n";
    fclose(fp);
    send(client_socket, response, strlen(response), 0);
    close(state->data_socket);
}
int handle_TYPE_Command(int client_socket, char* bufferIn,Session* state)
{
  char type;
  sscanf(bufferIn, "TYPE %c", &type);
  if (type == 'I') {
    snprintf(bufferIn, BUFFER_SIZE, "200 Type set to I.\r\n");
    send(client_socket, bufferIn, strlen(bufferIn), 0);
  } else {
    snprintf(bufferIn, BUFFER_SIZE,
              "504 Command not implemented for that parameter.\r\n");
    send(client_socket, bufferIn, strlen(bufferIn), 0);
  }
}

int handle_PORT_Command(int client_socket, char* bufferIn,Session* state)
{
  char ip[4];
  unsigned char p1, p2;
  sscanf(bufferIn, "PORT %hhu,%hhu,%hhu,%hhu,%hhu,%hhu", &ip[0], &ip[1], &ip[2],
          &ip[3], &p1, &p2);
  int data_port = p1 * 256 + p2;
  state->data_socket = create_data_connection(ip, data_port);
  if (state->data_socket < 0) {
    snprintf(bufferIn, BUFFER_SIZE, "425 Can't open data connection.\r\n");
    send(client_socket, bufferIn, strlen(bufferIn), 0);
  } else {
    snprintf(bufferIn, BUFFER_SIZE, "200 PORT command successful.\r\n");
    send(client_socket, bufferIn, strlen(bufferIn), 0);
  }
  return state->data_socket;
}


