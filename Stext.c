#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>

#define PORT 8756  // Define the port number for the Stext server
#define BUFFER_SIZE 1024  // Define the buffer size for data transfer

// Function prototypes
void handle_smain(int smain_socket);  // Function to handle incoming Smain connections
void save_txt_file(int smain_socket, const char *destination_path, const char *filename, const char *extra_content);  // Function to save a received text file
void remove_file(const char *filepath);  // Function to remove a specified file
void create_directory_if_not_exists(const char *path);  // Function to create a directory if it does not exist
void send_file(int client_socket, const char *filename);  // Function to send a file to the client
void send_txt_list(int smain_socket);  // Function to send a list of text files to the client

int main() {
    int server_socket, smain_socket;  // Define socket descriptors for server and Smain
    struct sockaddr_in server_addr, smain_addr;  // Define structures for server and Smain addresses
    socklen_t addr_len = sizeof(smain_addr);  // Define the size of the Smain address structure

    server_socket = socket(AF_INET, SOCK_STREAM, 0);  // Create a TCP socket using IPv4
    if (server_socket == 0) {  // Check if socket creation failed
        perror("Socket failed");  // Print an error message if socket creation fails
        exit(EXIT_FAILURE);  // Exit the program with a failure status
    }

    server_addr.sin_family = AF_INET;  // Set the address family to IPv4
    server_addr.sin_addr.s_addr = INADDR_ANY;  // Bind the socket to any available network interface
    server_addr.sin_port = htons(PORT);  // Set the server port number

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {  // Bind the server socket to the specified IP address and port
        perror("Bind failed");  // Print an error message if binding fails
        exit(EXIT_FAILURE);  // Exit the program with a failure status
    }

    if (listen(server_socket, 3) < 0) {  // Listen for incoming connections on the server socket with a maximum of 3 pending connections
        perror("Listen failed");  // Print an error message if listening fails
        exit(EXIT_FAILURE);  // Exit the program with a failure status
    }

    printf("Stext server is listening on port %d...\n", PORT);  // Print a message indicating that the server is listening

    while (1) {  // Enter an infinite loop to accept and handle incoming connections
        smain_socket = accept(server_socket, (struct sockaddr *)&smain_addr, &addr_len);  // Accept an incoming connection from Smain
        if (smain_socket < 0) {  // Check if the accept function failed
            perror("Accept failed");  // Print an error message if accepting fails
            exit(EXIT_FAILURE);  // Exit the program with a failure status
        }

        if (fork() == 0) {  // Create a child process to handle the Smain connection
            close(server_socket);  // Close the server socket in the child process
            handle_smain(smain_socket);  // Handle the Smain connection
            exit(0);  // Exit the child process when done
        } else {  // The parent process continues to accept new connections
            close(smain_socket);  // Close the Smain socket in the parent process
        }
    }

    return 0;  // Return 0 to indicate successful execution
}

void handle_smain(int smain_socket) {
    char buffer[BUFFER_SIZE];  // Buffer to store received data
    int bytes_read;  // Variable to store the number of bytes read

    while ((bytes_read = recv(smain_socket, buffer, BUFFER_SIZE, 0)) > 0) {  // Continuously read data from the Smain socket
        buffer[bytes_read] = '\0';  // Null-terminate the received string
        printf("Received from Smain: %s\n", buffer);  // Print the received data

        if (strncmp(buffer, "rmfile", 6) == 0) {  // Check if the command is "rmfile"
            char filepath[150];  // Buffer to store the file path
            sscanf(buffer, "rmfile %s", filepath);  // Extract the file path from the command

            const char *home_dir = getenv("HOME");  // Get the home directory path from the environment variable
            if (home_dir == NULL) {  // Check if the home directory is not found
                perror("Unable to get the home directory");  // Print an error message if home directory is not found
                return;  // Return to avoid further execution
            }

            char smain_path[BUFFER_SIZE];  // Buffer to store the expected smain path
            snprintf(smain_path, sizeof(smain_path), "%s/smain", home_dir);  // Construct the expected smain path based on the home directory

            if (strncmp(filepath, smain_path, strlen(smain_path)) == 0) {  // Check if the file path starts with the expected smain path
                char full_path[BUFFER_SIZE];  // Buffer to store the full path to the file
                snprintf(full_path, sizeof(full_path), "%s/stext%s", home_dir, filepath + strlen(smain_path));  // Replace ~/smain with ~/stext in the file path

                printf("Attempting to remove file: %s\n", full_path);  // Print a message indicating the file being removed
                remove_file(full_path);  // Call the function to remove the file
            } else {
                printf("Error: Invalid file path. It must start with %s/smain.\n", home_dir);  // Print an error message if the file path is invalid
            }
        } else if (strncmp(buffer, "text.tar", 8) == 0 || strncmp(buffer, "dtar", 4) == 0) {  // Check if the command is "text.tar" or "dtar"
            system("tar -cvf text.tar $(find ~/stext -name '*.txt')");  // Create a tar archive of all text files in the ~/stext directory
            send_file(smain_socket, "text.tar");  // Send the tar file to the Smain client
        } else if (strncmp(buffer, "list_txt", 8) == 0) {  // Check if the command is "list_txt"
            send_txt_list(smain_socket);  // Call the function to send a list of text files to the client
        } else {
            char filename[50], destination_path[150];  // Buffers to store the filename and destination path
            sscanf(buffer, "%s %s", filename, destination_path);  // Extract the filename and destination path from the command

            if (strncmp(destination_path, "~/smain", 7) == 0) {  // Check if the destination path starts with "~/smain"
                char full_destination[BUFFER_SIZE];  // Buffer to store the full destination path
                snprintf(full_destination, sizeof(full_destination), "%s/stext%s", getenv("HOME"), destination_path + 7);  // Replace ~/smain with ~/stext in the destination path

                char *last_slash = strrchr(full_destination, '/');  // Find the last occurrence of the '/' character
                char last_part[BUFFER_SIZE] = "";  // Buffer to store the part of the path after the last slash

                if (last_slash != NULL) {  // Check if a slash was found in the path
                    strncpy(last_part, last_slash + 1, BUFFER_SIZE - 1);  // Store the part after the last slash
                    *last_slash = '\0';  // Terminate the string at the last slash to get the directory path
                }

                create_directory_if_not_exists(full_destination);  // Create the destination directory if it doesn't exist

                printf("Saving .txt file to Stext in %s...\n", full_destination);  // Print a message indicating where the file will be saved
                save_txt_file(smain_socket, full_destination, filename, last_part);  // Call the function to save the text file
            } else {
                printf("Error: Invalid destination path. It must start with ~/smain.\n");  // Print an error message if the destination path is invalid
            }
        }

        send(smain_socket, "File processed", strlen("File processed"), 0);  // Send a confirmation message to the Smain client
    }

    close(smain_socket);  // Close the Smain socket when done
}

void create_directory_if_not_exists(const char *path) {
    char temp[BUFFER_SIZE];  // Temporary buffer to hold the modified path
    snprintf(temp, sizeof(temp), "%s", path);  // Copy the path into the temporary buffer

    // Recursively create the directory structure
    for (char *p = temp + 1; *p; p++) {  // Iterate through each character in the path string
        if (*p == '/') {  // Check if the current character is a directory separator
            *p = '\0';  // Temporarily terminate the string to create the intermediate directory
            mkdir(temp, 0777);  // Create the directory with full permissions
            *p = '/';  // Restore the directory separator
        }
    }
    mkdir(temp, 0777);  // Create the final directory specified by the full path
}

void save_txt_file(int smain_socket, const char *destination_path, const char *filename, const char *extra_content) {
    char full_path[BUFFER_SIZE];  // Buffer to store the full path of the file
    snprintf(full_path, sizeof(full_path), "%s/%s", destination_path, filename);  // Construct the full path to the file

    int file_fd = open(full_path, O_WRONLY | O_CREAT, 0666);  // Open the file for writing, create if it doesn't exist
    if (file_fd < 0) {  // Check if the file failed to open
        perror("File open error");  // Print an error message if file opening fails
        return;  // Return to avoid further execution if the file cannot be opened
    }

    // Write the extra content (last part of the path) to the file
    if (extra_content && strlen(extra_content) > 0) {  // Check if there is any extra content to write
        write(file_fd, extra_content, strlen(extra_content));  // Write the extra content to the file
    }

    char buffer[BUFFER_SIZE];  // Buffer to store the data being transferred
    int bytes_read;  // Variable to store the number of bytes read

    // Receive the file content from Smain and write it to the file
    while ((bytes_read = recv(smain_socket, buffer, BUFFER_SIZE, 0)) > 0) {  // Read data from the socket
        write(file_fd, buffer, bytes_read);  // Write the received data to the file
    }

    close(file_fd);  // Close the file after writing is complete
    printf("File saved to %s\n", full_path);  // Print a confirmation message with the file path
}

void remove_file(const char *filepath) {
    if (remove(filepath) == 0) {  // Attempt to remove the file
        printf("File %s removed successfully.\n", filepath);  // Print a success message if the file is removed
    } else {  // If the file removal fails
        perror("Error removing file");  // Print an error message indicating failure
    }
}

void send_file(int client_socket, const char *filename) {
    int fd = open(filename, O_RDONLY);  // Open the file for reading
    if (fd == -1) {  // Check if the file failed to open
        perror("Failed to open file");  // Print an error message if file opening fails
        send(client_socket, "File transfer failed", 20, 0);  // Send a failure message to the client
        return;  // Return to avoid further execution if the file cannot be opened
    }

    char buffer[BUFFER_SIZE];  // Buffer to store the data being sent
    int bytes_read;  // Variable to store the number of bytes read
    while ((bytes_read = read(fd, buffer, BUFFER_SIZE)) > 0) {  // Read data from the file
        send(client_socket, buffer, bytes_read, 0);  // Send the data to the client
    }
    close(fd);  // Close the file after sending is complete
}

void send_txt_list(int smain_socket) {
    char file_list[BUFFER_SIZE] = "";  // Buffer to store the list of text files
    DIR *dir;  // Pointer to the directory stream
    struct dirent *entry;  // Pointer to directory entry

    // Expand the ~ to the actual home directory path
    char path[BUFFER_SIZE];  // Buffer to store the expanded path
    snprintf(path, sizeof(path), "%s/stext", getenv("HOME"));  // Construct the path to the Stext directory

    // Open the directory containing the text files
    if ((dir = opendir(path)) != NULL) {  // Check if the directory can be opened
        while ((entry = readdir(dir)) != NULL) {  // Iterate through the directory entries
            if (strstr(entry->d_name, ".txt")) {  // Check if the file has a .txt extension
                strcat(file_list, entry->d_name);  // Add the text filename to the list
                strcat(file_list, "\n");  // Add a newline after the filename
            }
        }
        closedir(dir);  // Close the directory when done
    } else {  // If the directory fails to open
        perror("Failed to open directory");  // Print an error message
        snprintf(file_list, sizeof(file_list), "Failed to open directory: %s\n", strerror(errno));  // Add the error message to the list
        send(smain_socket, file_list, strlen(file_list), 0);  // Send the error message to the client
        return;  // Return to avoid further execution
    }

    send(smain_socket, file_list, strlen(file_list), 0);  // Send the list of text files to the client
}
