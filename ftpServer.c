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

#define PORT 10009
#define BUFFER_SIZE 2048

void handle_client(void *client_fdIn);
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

int main() {
  int server_fd, client_fd;
  struct sockaddr_in server_addr, client_addr;

  // Create a socket
  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    perror("socket");
    exit(EXIT_FAILURE);
  }

  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(PORT);

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
    if ((client_fd = accept(server_fd, (struct sockaddr *)&client_addr,
                            &addr_len)) < 0) {
      perror("accept");
      exit(EXIT_FAILURE);
    }

    printf("Connection accepted from %s:%d\n", inet_ntoa(client_addr.sin_addr),
           ntohs(client_addr.sin_port));
    pthread_t clientThread;
    pthread_create(&clientThread, NULL, handle_client, (void *)client_fd);
    // handle_client(client_fd);

    // Close the connection
    // close(client_fd);
  }

  close(server_fd);
  return 0;
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
  char buffer[BUFFER_SIZE];
  int read_bytes;
  int logged_in = 0;
  int data_socket;
  char mCwd[BUFFER_SIZE];
  getcwd(mCwd, sizeof(mCwd));

  // Send welcome message
  snprintf(buffer, BUFFER_SIZE, "220 Welcome to the simple FTP server.\r\n");
  send(client_fd, buffer, strlen(buffer), 0);

  while ((read_bytes = recv(client_fd, buffer, BUFFER_SIZE, 0)) > 0) {
    // Ensure the buffer is null-terminated
    buffer[read_bytes] = '\0';
    printf("Command %s\n", buffer);
    char cmd[5];
    sscanf(buffer, "%4s", cmd);

    if (strcmp(cmd, "USER") == 0) {
      snprintf(buffer, BUFFER_SIZE, "331 User name okay, need password.\r\n");
      send(client_fd, buffer, strlen(buffer), 0);
    } else if (strcasecmp(cmd, "PASS") == 0) {
      logged_in = 1;
      snprintf(buffer, BUFFER_SIZE, "230 User logged in, proceed.\r\n");
      send(client_fd, buffer, strlen(buffer), 0);
    } else if (strcasecmp(cmd, "QUIT") == 0) {
      snprintf(buffer, BUFFER_SIZE, "221 Goodbye.\r\n");
      send(client_fd, buffer, strlen(buffer), 0);
      break;
    } else if (strcmp(cmd, "PWD") == 0) {
      if (logged_in) {
        char cwd[BUFFER_SIZE];
        getcwd(cwd, sizeof(cwd));
        snprintf(buffer, BUFFER_SIZE,
                 "257 \"%s\" is the current directory.\r\n", cwd);
        send(client_fd, buffer, strlen(buffer), 0);
      }
    } else if (strcmp(cmd, "CWD") == 0) {
      char tmpBuffer[BUFFER_SIZE];
      char tmp[BUFFER_SIZE];
      memset(tmpBuffer, 0, BUFFER_SIZE);
      memset(mCwd, 0, sizeof(mCwd));
      strncpy(mCwd, buffer + 4, strlen(buffer + 4));
      mCwd[strlen(mCwd)] = '\0';
      char *cr = strchr(mCwd, '\r');
      if (cr != NULL)
        *cr = '\0';
      if (chdir(mCwd) == -1)
        perror("chdir");
      snprintf(tmpBuffer, BUFFER_SIZE,
               "250 Directory successfully changed to %s.\r\n", mCwd);
      send(client_fd, tmpBuffer, strlen(tmpBuffer), 0);
    } else if (strcmp(cmd, "TYPE") == 0) {
      char type;
      sscanf(buffer, "TYPE %c", &type);

      if (type == 'I') {
        snprintf(buffer, BUFFER_SIZE, "200 Type set to I.\r\n");
        send(client_fd, buffer, strlen(buffer), 0);
      } else {
        snprintf(buffer, BUFFER_SIZE,
                 "504 Command not implemented for that parameter.\r\n");
        send(client_fd, buffer, strlen(buffer), 0);
      }
    } else if (strcmp(cmd, "PORT") == 0) {
      char ip[4];
      unsigned char p1, p2;
      sscanf(buffer, "PORT %hhu,%hhu,%hhu,%hhu,%hhu,%hhu", &ip[0], &ip[1],
             &ip[2], &ip[3], &p1, &p2);
      int data_port = p1 * 256 + p2;
      data_socket = create_data_connection(ip, data_port);
      if (data_socket < 0) {
        snprintf(buffer, BUFFER_SIZE, "425 Can't open data connection.\r\n");
        send(client_fd, buffer, strlen(buffer), 0);
      } else {
        snprintf(buffer, BUFFER_SIZE, "200 PORT command successful.\r\n");
        send(client_fd, buffer, strlen(buffer), 0);
      }
    } else if (strcmp(cmd, "LIST") == 0) {
      DIR *dir;
      struct dirent *entry;
      char entry_buffer[BUFFER_SIZE];
      dir = opendir(mCwd);
      if (dir == NULL) {
        perror("opendir");
        snprintf(buffer, BUFFER_SIZE, "550 Failed to open directory.\r\n");
        send(client_fd, buffer, strlen(buffer), 0);
        close(data_socket);
      } else {
        snprintf(buffer, BUFFER_SIZE,
                 "150 Opening ASCII mode data connection for file list.\r\n");
        send(client_fd, buffer, strlen(buffer), 0);

        while ((entry = readdir(dir)) != NULL) {
          struct stat file_stat;
          char filepath[BUFFER_SIZE];
          snprintf(filepath, BUFFER_SIZE, "%s/%s", mCwd, entry->d_name);
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

            send(data_socket, entry_buffer, strlen(entry_buffer), 0);
          }
        }
        snprintf(buffer, BUFFER_SIZE, "226 Transfer complete.\r\n");
        send(client_fd, buffer, strlen(buffer), 0);
        closedir(dir);
        close(data_socket);
      }
    } else if (strcmp(cmd, "DELE") == 0) {
      printf("Deletion Command\n");
      char fileToDelete[BUFFER_SIZE];
      char finalFile[BUFFER_SIZE];
      strncpy(fileToDelete, buffer + 4, strlen(buffer + 4));
      char *cr = strchr(fileToDelete, '\r');
      if (cr != NULL)
        *cr = '\0';
      sprintf(finalFile, "%s%s%s", mCwd, "/", fileToDelete);
      removeSpaces(finalFile);
      printf("File to Delete %s\n", finalFile);
      int res = remove(finalFile);
      printf("Result %d\n", res);
      sprintf(buffer, "250 File deleted successfully.\r\n");
      send(client_fd, buffer, strlen(buffer), 0);
    } else if (strcmp(cmd, "RETR") == 0) {
      printf("Download Command\n");
      char fileName[BUFFER_SIZE];
      char filePath[BUFFER_SIZE];
      memset(fileName,0,BUFFER_SIZE);
      memset(filePath,0,BUFFER_SIZE);
      strncpy(fileName, buffer + 4, strlen(buffer + 4));
      sprintf(filePath, "%s%s%s", mCwd, "/", fileName);
      removeSpaces(filePath);
      FILE *fp = fopen(filePath, "rb");
      if (fp == NULL)
        perror("Fopen");
      printf("FilePath:%s\n", filePath);
      // Read and send file data
      char fileBuffer[BUFFER_SIZE];
      size_t bytes_read;
      while ((bytes_read = fread(fileBuffer, 1, BUFFER_SIZE, fp)) > 0) {
        int sended = send(data_socket,fileBuffer,bytes_read,0);
        printf("Bytes to read %d\n",sended);
      }
      fclose(fp);
      const char* response = "226 Transfer complete.\r\n";
      send(client_fd, response, strlen(response), 0);
      printf("Upload Finished\n");
      close(data_socket);
    } else if (strcmp(cmd, "MKD") == 0) {
      char finalDir[BUFFER_SIZE];
      char *response = "550 Failed to create directory.\r\n";
      sprintf(finalDir, "%s%s%s", mCwd, "/", buffer + 4);
      removeSpaces(finalDir);
      if (mkdir(finalDir, 0777) == 0)
        response = "250 Directory created successfully.\r\n";
      else
        response = "550 Failed to create directory.\r\n";
      send(client_fd, response, strlen(response), 0);
    } else {
      snprintf(buffer, BUFFER_SIZE, "500 Unknown command.\r\n");
      send(client_fd, buffer, strlen(buffer), 0);
    }
  }
}
