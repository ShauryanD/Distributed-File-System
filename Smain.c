//==============================================================================================================================
//Required Libraries
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
//==============================================================================================================================
//Defining
#define PORT 8702 //This is the port number for the Smain server
#define SPDF_PORT 8755 //This is the port number for the Spdf server 
#define STEXT_PORT 8756 //This is the port number for the Stext server 
#define BUFFER_SIZE 1024 //This is the buffer size for data transfer
//==============================================================================================================================
//Functions
void prcclient(int client_socket);  //This is the function to handle client requests
void save_file(int client_socket, const char *destination_path, const char *filename); //This is the function for saving a file received from the client
void send_to_spdf(const char *filename, const char *destination_path);//This function is going to send a file to the SPDF
void send_to_stext(const char *filename, const char *destination_path);//This function is going to send a file to the Stext service
void create_directory_if_not_exists(const char *path);//This function is going to create a directory if it does not already exist. 
void remove_local_file(const char *filepath);//This function is going to remove a local file from the filesystem. 
void forward_delete_request(const char *service, const char *filename);//This function is going to forward a delete request to a specified service 
void create_tar_file(const char *filetype, int client_socket);//This function is going to create a tar
void send_file(int client_socket, const char *filename);//This function is going to send a file to a client over a socket connection. 
void display_pathname(int client_socket, const char *pathname);//This is going to function to display or send a pathname to the client. 
void receive_and_send_file(const char *filename, int source_socket, int dest_socket);//This function is going to receive a file from one socket and send it to another socket. 
//==============================================================================================================================
int main() {
    int server_socket, client_socket; // Socket descriptors
    struct sockaddr_in server_addr, client_addr; // Address structures
    socklen_t addr_len = sizeof(client_addr);// Address length

    server_socket = socket(AF_INET, SOCK_STREAM, 0);// Creating the socket
    if (server_socket == 0) {
        perror("Socket failed"); // Checking for socket error
        exit(EXIT_FAILURE); //Failure
    }

    server_addr.sin_family = AF_INET; // This is to IPv4
    server_addr.sin_addr.s_addr = INADDR_ANY; // Binding to all interfaces
    server_addr.sin_port = htons(PORT); // Set port

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed"); // Checking for bind error
        exit(EXIT_FAILURE); //Failure
    }

    if (listen(server_socket, 3) < 0) {
        perror("Listen failed"); // Checking for listen error
        exit(EXIT_FAILURE); //Failure
    }

    printf("Smain server is listening on port %d...\n", PORT); // Listening message

    while (1) {
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_len); // Accepting connection
        if (client_socket < 0) {
            perror("Accept failed"); // Checking for accept error
            exit(EXIT_FAILURE); //Failure
        }

        if (fork() == 0) {  // This child process is going to handle the client
            close(server_socket); // Closing the server socket in child
            prcclient(client_socket); // Handling client
            exit(0); // Exiting child process
        } else {  // Parent process continues to accept new clients
            close(client_socket);// Closing the client socket in parent
        }
    }

    return 0;
}
//==============================================================================================================================
void prcclient(int client_socket) { // This function is going to handle client communication
    char buffer[BUFFER_SIZE]; // This buffer is going to store incoming data
    int bytes_read; // This variable is going to track the number of bytes read

    while ((bytes_read = recv(client_socket, buffer, BUFFER_SIZE, 0)) > 0) { // Receiving the data from client
        buffer[bytes_read] = '\0'; // Null-terminating the received data
        printf("Received from client: %s\n", buffer);

        char command[10], filename[50], destination_path[150]; // Variables to hold parsed command, filename, and destination path
        sscanf(buffer, "%s %s %s", command, filename, destination_path);

        if (strcmp(command, "ufile") == 0) { // Checking if the command is "ufile"
            if (strstr(filename, ".c")) { // Checking if the file is a .c file
                // Handling .c file upload to Smain
                if (strncmp(destination_path, "~/smain", 7) == 0) {// Checking if destination is within "~/smain"
                    char full_destination[BUFFER_SIZE]; // Buffer to store the full destination path
                    snprintf(full_destination, sizeof(full_destination), "%s%s", getenv("HOME"), destination_path + 1);// Constructing the full path
                    create_directory_if_not_exists(full_destination); // Ensuring the directory exists
                    save_file(client_socket, full_destination, filename); // Saving the .c file to the destination
                }
            } else if (strstr(filename, ".pdf")) { // Checking if the file is a .pdf file
                // Forwarding .pdf file to Spdf
                send_to_spdf(filename, destination_path);
            } else if (strstr(filename, ".txt")) {// Checking if the file is a .txt file
                // Forwarding .txt file to Stext
                send_to_stext(filename, destination_path);
            } else {
                send(client_socket, "Invalid File Extension! ", strlen("Invalid File Extension! "), 0);
            }
        } else if (strcmp(command, "rmfile") == 0) { // Checking if the command is "rmfile"
            const char *extension = strrchr(filename, '.'); // Getting the file extension

            if (extension) { // If a file extension exists
                if (strcmp(extension, ".c") == 0) {// If it's a .c file
                    remove_local_file(filename);// Remove the local .c file
                } else if (strcmp(extension, ".pdf") == 0) {// If it's a .pdf file
                    forward_delete_request("Spdf", filename);// Forwarding delete request to Spdf service
                } else if (strcmp(extension, ".txt") == 0) {// If it's a .txt file
                    forward_delete_request("Stext", filename);// Forwarding the delete request to Stext service
                } else {
                    printf("Unsupported file type: %s\n", extension);
                }
            } else {
                printf("Invalid filename: %s\n", filename);
            }
        } else if (strcmp(command, "dtar") == 0) { // Checking if the command is "dtar"
            create_tar_file(filename, client_socket);// Creating a tar file and send it to the client
        } else if (strcmp(command, "display") == 0) {// Checking if the command is "display"
            display_pathname(client_socket, filename);// Displaying the pathname to the client
        }

        send(client_socket, "Command processed", strlen("Command processed"), 0); // Sending confirmation to the client
    }

    close(client_socket);// Closing the connection with the client
}
//==============================================================================================================================
void save_file(int client_socket, const char *destination_path, const char *filename) {// This function is going to save a file from client socket
    char full_path[150];
    snprintf(full_path, sizeof(full_path), "%s/%s", destination_path, filename);// Constructing the full file path by combining destination_path and filename

    int file_fd = open(full_path, O_WRONLY | O_CREAT, 0666);// Opening the file for writing, creating it if it doesn't exist, set permissions to 0666
    if (file_fd < 0) { // Checking if the file was successfully opened
        perror("File open error");
        return;// Exit the function
    }

    char buffer[BUFFER_SIZE]; // Buffer to store data received from the client
    int bytes_read;// Variable to store the number of bytes
    int total_bytes = 0;// Variable to keep track of the total bytes 

    while ((bytes_read = recv(client_socket, buffer, BUFFER_SIZE, 0)) > 0) { // Loop to receive data from the client
        write(file_fd, buffer, bytes_read); // Writing the received data to the file
        total_bytes += bytes_read;// Updating the total bytes written
    }

    printf("File saved to %s\n", full_path);  // Confirmation message only
    close(file_fd); // Closing the file descriptor
}
//==============================================================================================================================
void send_to_spdf(const char *filename, const char *destination_path) { // Function to send a file to Spdf
    int sock; // Variable to store the socket descriptor
    struct sockaddr_in spdf_addr; // Structure to hold the Spdf server

    sock = socket(AF_INET, SOCK_STREAM, 0); // Creating a socket for communication using TCP
    if (sock < 0) {// Checking if the socket was successfully created
        perror("Socket creation error");
        return;
    }

    spdf_addr.sin_family = AF_INET; // Setting the address family to IPv4
    spdf_addr.sin_port = htons(SPDF_PORT);// Setting the port number for Spdf server
    if (inet_pton(AF_INET, "127.0.0.1", &spdf_addr.sin_addr) <= 0) {// Converting IP address to binary format
        printf("\nInvalid address/ Address not supported \n");
        return;
    }

    if (connect(sock, (struct sockaddr *)&spdf_addr, sizeof(spdf_addr)) < 0) { // Connecting to the Spdf server
        perror("Connection Failed");
        return; // Exit the function
    }

    char command[BUFFER_SIZE]; // Buffer which is going to hold the command to be sent
    snprintf(command, sizeof(command), "%s %s", filename, destination_path); // Printing an error message if connection failed
    send(sock, command, strlen(command), 0); // Sending the command to the Spdf server

    FILE *file = fopen(filename, "rb"); // Opening the file to be sent in binary read mode
    if (!file) { // Checking if the file was successfully opened
        perror("File open error");
        close(sock); // Closing the socket before exiting
        return;
    }

    char buffer[BUFFER_SIZE]; // Buffer to hold file data read from the file
    size_t bytes_read; // Variable to store the number of bytes

    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) { // Reading the data from the file into the buffer
        if (send(sock, buffer, bytes_read, 0) == -1) {// Sending the data to the Spdf server
            perror("File send error");
            break; // Exiting the loop  
        }
    }

    fclose(file);// Closing the file after sending all data
    close(sock);// Closing the socket after finishing communication
}
//==============================================================================================================================
void send_to_stext(const char *filename, const char *destination_path) { // This Function to send a file to Stext
    int sock; // Variable to store the socket descriptor
    struct sockaddr_in stext_addr; // Structure to hold the Stext

    sock = socket(AF_INET, SOCK_STREAM, 0);// Creating a socket for communication using TCP
    if (sock < 0) { // Check if the socket was successfully created
        perror("Socket creation error");
        return; // Exit the function 
    }

    stext_addr.sin_family = AF_INET; // Set the address family to IPv4
    stext_addr.sin_port = htons(STEXT_PORT); // Set the port number for Stext server
    if (inet_pton(AF_INET, "127.0.0.1", &stext_addr.sin_addr) <= 0) {
        printf("\nInvalid address/ Address not supported \n");
        return;// Exit the function
    }

    if (connect(sock, (struct sockaddr *)&stext_addr, sizeof(stext_addr)) < 0) { // Connect to the Stext server using the address information
        perror("Connection Failed");
        return; // Exit the function
    }

    char command[BUFFER_SIZE]; // Buffer to hold the command to be sent
    snprintf(command, sizeof(command), "%s %s", filename, destination_path); // Format the command with filename and destination path
    send(sock, command, strlen(command), 0);// Send the command to the Stext server

    FILE *file = fopen(filename, "rb");// Open the file to be sent in binary read mode
    if (!file) { // Check if the file was successfully opened
        perror("File open error");
        close(sock);// Close the socket before exiting
        return; // Exit the function
    }

    char buffer[BUFFER_SIZE];  // Buffer to hold file data read from the file
    size_t bytes_read;  // Variable to store the number of bytes

    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {// Read data from the file into the buffer
        if (send(sock, buffer, bytes_read, 0) == -1) {  // Send the data to the Stext server
            perror("File send error");
            break; // Exit the loop
        }
    }

    fclose(file); // Close the file after sending all data
    close(sock); // Close the socket after finishing communication
}

//==============================================================================================================================
void create_directory_if_not_exists(const char *path) { // Function to create directories
    char temp[BUFFER_SIZE]; // Buffer to hold a copy of the path
    snprintf(temp, sizeof(temp), "%s", path);// Copy the path

    for (char *p = temp + 1; *p; p++) {// Iterate
        if (*p == '/') { // Checking if the current character is a '/'
            *p = '\0'; // Temporarily terminate the string at the '/'
            mkdir(temp, 0777); // Create the directory using the modified path
            *p = '/'; // Restore the '/' character to continue the path processing
        }
    }
    mkdir(temp, 0777); // Create the final directory specified by the path
}

void remove_local_file(const char *filepath) { // Function to remove a local file
    if (remove(filepath) == 0) { // Attempt to remove the file specified by filepath
        printf("File %s removed successfully.\n", filepath); // Print success message if the file was removed
    } else {
        perror("Error removing file"); // Print an error message with details of the failure
    }
}
//==============================================================================================================================
void forward_delete_request(const char *service, const char *filename) {// Function to forward a delete request to a specified service
    int sock; // Socket file descriptor
    struct sockaddr_in server_addr; // Structure to hold the server address
    int target_port = (strcmp(service, "Spdf") == 0) ? SPDF_PORT : STEXT_PORT; // Determine port based on service name

    sock = socket(AF_INET, SOCK_STREAM, 0); // Create a socket for IPv4 and TCP
    if (sock < 0) { // Check if socket creation failed
        perror("Socket creation error");
        return; // Exit the function
    }

    server_addr.sin_family = AF_INET; // Set address family to IPv4
    server_addr.sin_port = htons(target_port); // Set port number for the server
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) { // Convert IP address to network format
        perror("Invalid address");
        close(sock); // Close the socket
        return; //Exit the function
    }

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) { // Connect to the server
        perror("Connection failed");
        close(sock); // Close the socket
        return;// Exit the function
    }

    char command[BUFFER_SIZE];// Buffer to hold the command to be sent
    snprintf(command, sizeof(command), "rmfile %s", filename);// Format the delete command with filename
    if (send(sock, command, strlen(command), 0) < 0) {// Send the command to the server
        perror("Send error");
        close(sock);// Close the socket
        return;// Exit the function
    }

    char response[BUFFER_SIZE];// Buffer to hold the command to be sent
    int bytes_read = recv(sock, response, BUFFER_SIZE - 1, 0);  // Leaving the space for null terminator
    if (bytes_read < 0) {// Send the command to the server
        perror("Receive error");
        close(sock);// Close the socket
        return;// Exit the function
    }

    response[bytes_read] = '\0';  // Null terminate the response
    printf("Response from server: %s\n", response);

    close(sock);// Closing the socket
}
//==============================================================================================================================
void create_tar_file(const char *filetype, int client_socket) { // Function to create a tar file based on file type and send it to a client socket
    char tar_command[150];// Buffer to hold the tar command string
    if (strcmp(filetype, ".c") == 0) {// Check if the file type is ".c"
       
        snprintf(tar_command, sizeof(tar_command), "tar -cf cfiles.tar ~/smain/*.c");
        system(tar_command);// Execute the tar command to create the tar file
        send_file(client_socket, "cfiles.tar"); // Send the created tar file to the client socket
    } else { // If the file type is not ".c"
        int port = (strcmp(filetype, ".pdf") == 0) ? SPDF_PORT : STEXT_PORT;
        int sock = socket(AF_INET, SOCK_STREAM, 0);// Create a socket for IPv4 and TCP
        struct sockaddr_in server_addr;// Structure to hold the server address information
        server_addr.sin_family = AF_INET;// Set the address family to IPv4
        server_addr.sin_port = htons(port);// Set the port number in network byte order
        if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
            perror("Invalid address");
            close(sock);// Close the socket
            return;// Exit the function
        }

        if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) { // Attempt to connect to the server
            perror("Connection failed");
            close(sock); // Close the socket
            return;// Exit the function
        }

        char command[BUFFER_SIZE]; // Buffer to hold the command to be sent to the server
        snprintf(command, sizeof(command), "dtar %s", filetype);
        send(sock, command, strlen(command), 0);// Send the command to the server

        char tar_filename[50];
        snprintf(tar_filename, sizeof(tar_filename), "%s.tar", (strcmp(filetype, ".pdf") == 0) ? "pdf" : "text");
        receive_and_send_file(tar_filename, sock, client_socket);

        close(sock);  // Closing the socket after communication is complete
    }
}
//==============================================================================================================================
void send_file(int client_socket, const char *filename) {// Function to send a file to a client via the given socket
    int fd = open(filename, O_RDONLY);// Open the file specified by 'filename' in read-only mode
    if (fd == -1) {// Check if the file was not successfully opened
        perror("Failed to open file");
        send(client_socket, "File transfer failed", 20, 0);// Send a failure message to the client socket
        return;
    }

    char buffer[BUFFER_SIZE];// Buffer to hold data read from the file
    int bytes_read;// Variable to store the number of bytes read from the file
    while ((bytes_read = read(fd, buffer, BUFFER_SIZE)) > 0) { // Read data from the file into the buffer
        send(client_socket, buffer, bytes_read, 0);// Send the read data to the client socket
    }
    close(fd);// Closing the file descriptor once all data has been sent
}
//==============================================================================================================================
void receive_and_send_file(const char *filename, int source_socket, int dest_socket) {
    // Receive file content and directly send to client
    char buffer[BUFFER_SIZE];
    int bytes_read;
    while ((bytes_read = recv(source_socket, buffer, BUFFER_SIZE, 0)) > 0) {
        send(dest_socket, buffer, bytes_read, 0);
    }
}

//==============================================================================================================================
void display_pathname(int client_socket, const char *pathname) {
    char buffer[BUFFER_SIZE];
    char combined_list[BUFFER_SIZE * 10] = ""; // To store the combined file list
    DIR *dir;
    struct dirent *entry;

    // Expand the ~ to the actual home directory path for smain, spdf, and stext
    char smain_path[BUFFER_SIZE];
    char spdf_path[BUFFER_SIZE];
    char stext_path[BUFFER_SIZE];

    if (pathname[0] == '~') {
        snprintf(smain_path, BUFFER_SIZE, "%s/smain%s", getenv("HOME"), pathname + strlen("~/smain"));
        snprintf(spdf_path, BUFFER_SIZE, "%s/spdf%s", getenv("HOME"), pathname + strlen("~/smain"));
        snprintf(stext_path, BUFFER_SIZE, "%s/stext%s", getenv("HOME"), pathname + strlen("~/smain"));
    } else {
        snprintf(smain_path, BUFFER_SIZE, "%s", pathname);
        snprintf(spdf_path, BUFFER_SIZE, "%s/spdf%s", getenv("HOME"), pathname + strlen("~/smain"));
        snprintf(stext_path, BUFFER_SIZE, "%s/stext%s", getenv("HOME"), pathname + strlen("~/smain"));
    }

    // Fetching .c files from the given pathname under ~/smain/
    if ((dir = opendir(smain_path)) != NULL) {
        while ((entry = readdir(dir)) != NULL) {
            if (strstr(entry->d_name, ".c")) {
                strcat(combined_list, entry->d_name);
                strcat(combined_list, "\n");
            }
        }
        closedir(dir);
    }

    // Fetching .pdf files from the corresponding directory under ~/spdf/
    if ((dir = opendir(spdf_path)) != NULL) {
        while ((entry = readdir(dir)) != NULL) {
            if (strstr(entry->d_name, ".pdf")) {
                strcat(combined_list, entry->d_name);
                strcat(combined_list, "\n");
            }
        }
        closedir(dir);
    }

    // Fetching .txt files from the corresponding directory under ~/stext/
    if ((dir = opendir(stext_path)) != NULL) {
        while ((entry = readdir(dir)) != NULL) {
            if (strstr(entry->d_name, ".txt")) {
                strcat(combined_list, entry->d_name);
                strcat(combined_list, "\n");
            }
        }
        closedir(dir);
    }

    // Sending the combined list to the client
    if (strlen(combined_list) > 0) {
        send(client_socket, combined_list, strlen(combined_list), 0);
    } else {
        send(client_socket, "No files found.\n", 16, 0);
    }
}
//==============================================================================================================================