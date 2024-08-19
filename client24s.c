#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/stat.h>

#define PORT 8702  // Define the port number for the client-server communication
#define BUFFER_SIZE 1024  // Define the buffer size for data transfer

// Function prototypes
void send_command(int socket, const char *command, const char *filename, const char *destination_path);  // Function to send a command to the server along with a file
void send_delete_request(int socket, const char *filename);  // Function to send a delete request to the server
void send_dtar_command(int socket, const char *command);  // Function to send a tar creation command to the server
void send_display_command(int socket, const char *pathname);  // Function to send a display request to the server

int main() {
    int client_socket;  // Define the socket descriptor for the client
    struct sockaddr_in server_addr;  // Define the structure for server address
    char command[BUFFER_SIZE];  // Buffer to store the user's command

    client_socket = socket(AF_INET, SOCK_STREAM, 0);  // Create a TCP socket using IPv4
    if (client_socket < 0) {  // Check if socket creation failed
        perror("Socket creation error");  // Print an error message if socket creation fails
        return -1;  // Return -1 to indicate an error
    }

    server_addr.sin_family = AF_INET;  // Set the address family to IPv4
    server_addr.sin_port = htons(PORT);  // Set the server port number
    server_addr.sin_addr.s_addr = INADDR_ANY;  // Bind the socket to any available network interface

    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {  // Attempt to connect to the server
        perror("Connection failed");  // Print an error message if connection fails
        return -1;  // Return -1 to indicate an error
    }

    printf("Connected to Smain server on port %d...\n", PORT);  // Print a success message indicating connection to the server

    while (1) {  // Enter an infinite loop to process user commands
        printf("client24s$ ");  // Prompt the user for input
        fgets(command, BUFFER_SIZE, stdin);  // Read the user's input
        command[strcspn(command, "\n")] = '\0';  // Remove the newline character from the input

        if (strcmp(command, "exit") == 0) {  // Check if the user wants to exit
            printf("Exiting...\n");  // Print a message indicating that the program is exiting
            break;  // Break the loop to exit the program
        }

        char command_name[10], filename[50], destination_path[100];  // Buffers to store the parsed command name, filename, and destination path
        if (sscanf(command, "%s %s %s", command_name, filename, destination_path) == 2) {  // Parse the command into command_name and filename
            if (strcmp(command_name, "rmfile") == 0) {  // Check if the command is "rmfile"
                send_delete_request(client_socket, filename);  // Send a delete request to the server
            } else if (strcmp(command_name, "dtar") == 0) {  // Check if the command is "dtar"
                send_dtar_command(client_socket, filename);  // Send a tar creation command to the server
            } else if (strcmp(command_name, "display") == 0) {  // Check if the command is "display"
                send_display_command(client_socket, filename);  // Send a display request to the server
            } else {
                printf("Invalid command!\n");  // Print an error message if the command is invalid
            }
        } else if (sscanf(command, "%s %s %s", command_name, filename, destination_path) == 3) {  // Parse the command into command_name, filename, and destination_path
            if (strcmp(command_name, "ufile") == 0) {  // Check if the command is "ufile"
                struct stat buffer;  // Define a structure to hold file status information
                if (stat(filename, &buffer) == 0) {  // Check if the file exists
                    send_command(client_socket, command_name, filename, destination_path);  // Send the file and command to the server
                } else {
                    printf("Error: File '%s' not found.\n", filename);  // Print an error message if the file does not exist
                    continue;  // Skip to the next iteration to prompt for a new command
                }
            } else {
                printf("Invalid command!\n");  // Print an error message if the command is invalid
            }
        } else {
            printf("Invalid command format!\n");  // Print an error message if the command format is invalid
        }

        char response[BUFFER_SIZE];  // Buffer to store the server's response
        int bytes_read = recv(client_socket, response, BUFFER_SIZE, 0);  // Receive the server's response
        if (bytes_read > 0) {  // Check if any data was received
            response[bytes_read] = '\0';  // Null-terminate the response string
            printf("Server response: %s\n", response);  // Print the server's response
        }
    }

    close(client_socket);  // Close the client socket when done
    return 0;  // Return 0 to indicate successful execution
}

void send_command(int socket, const char *command, const char *filename, const char *destination_path) {
    char full_command[BUFFER_SIZE];  // Buffer to store the full command string
    snprintf(full_command, sizeof(full_command), "%s %s %s", command, filename, destination_path);  // Construct the full command string
    send(socket, full_command, strlen(full_command), 0);  // Send the command to the server
    printf("Command sent: %s\n", full_command);  // Print a confirmation message with the sent command

    // Open the file for reading
    FILE *file = fopen(filename, "rb");  // Open the specified file in binary read mode
    if (!file) {  // Check if the file failed to open
        perror("File open error");  // Print an error message if file opening fails
        return;  // Return to avoid further execution if the file cannot be opened
    }

    char buffer[BUFFER_SIZE];  // Buffer to store the data being sent
    size_t bytes_read;  // Variable to store the number of bytes read

    // Read the file and send its content
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {  // Read data from the file
        if (send(socket, buffer, bytes_read, 0) == -1) {  // Send the data to the server
            perror("File send error");  // Print an error message if sending fails
            break;  // Break the loop to stop sending data
        }
    }

    fclose(file);  // Close the file after sending is complete
}

void send_delete_request(int socket, const char *filename) {
    char command[BUFFER_SIZE];  // Buffer to store the command string
    snprintf(command, sizeof(command), "rmfile %s", filename);  // Construct the delete command string
    send(socket, command, strlen(command), 0);  // Send the delete request to the server
    printf("Delete request sent: %s\n", command);  // Print a confirmation message with the sent command
}

void send_dtar_command(int socket, const char *filetype) {
    char command[BUFFER_SIZE];  // Buffer to store the command string
    snprintf(command, sizeof(command), "dtar %s", filetype);  // Construct the tar command string
    send(socket, command, strlen(command), 0);  // Send the tar command to the server
    printf("Dtar command sent: %s\n", command);  // Print a confirmation message with the sent command
}

void send_display_command(int socket, const char *pathname) {
    char command[BUFFER_SIZE];  // Buffer to store the command string
    snprintf(command, sizeof(command), "display %s", pathname);  // Construct the display command string
    send(socket, command, strlen(command), 0);  // Send the display request to the server
    printf("Display command sent: %s\n", command);  // Print a confirmation message with the sent command
}
