/*
 *
 * CSEE 4840 Lab 2 for 2019
 *
 * Name/UNI: Please Changeto Yourname (pcy2301)
 */
#include "fbputchar.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "usbkeyboard.h"
#include <pthread.h>
#include <stdlib.h>

/* Update SERVER_HOST to be the IP address of
 * the chat server you are connecting to
 */
/* arthur.cs.columbia.edu */
#define SERVER_HOST "128.59.19.114"
#define SERVER_PORT 42000

#define BUFFER_SIZE 128

/*
 * References:
 *
 * https://web.archive.org/web/20130307100215/http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html
 *
 * http://www.thegeekstuff.com/2011/12/c-socket-programming/
 * 
 */

int sockfd; /* Socket file descriptor */
void insert(char *, int *, const char);
void delete(char *, int *);

struct libusb_device_handle *keyboard;
uint8_t endpoint_address;
uint8_t prev_keycode[6] = { 0 };

pthread_t network_thread;
void *network_thread_f(void *);

int main()
{
    int err, col;

    struct sockaddr_in serv_addr;

    struct usb_keyboard_packet packet;
    int transferred;
    char keystate[12];

    if ((err = fbopen()) != 0) {
        fprintf(stderr, "Error: Could not open framebuffer: %d\n", err);
        exit(1);
    }

    /* Draw rows of asterisks across the top and bottom of the screen */
    // fbclear();
    printf("cleared\n");

    for (col = 0 ; col < 64 ; col++) {
        fbputchar('*', 0, col);
        fbputchar('*', 11, col);
        fbputchar('*', 23, col);
    }

    fbputs("Hello CSEE 4840 World!", 4, 10);

    /* Open the keyboard */
    if ( (keyboard = openkeyboard(&endpoint_address)) == NULL ) {
        fprintf(stderr, "Did not find a keyboard\n");
        exit(1);
    }

    /* Create a TCP communications socket */
    if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
        fprintf(stderr, "Error: Could not create socket\n");
        exit(1);
    }

    /* Get the server address */
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);
    if ( inet_pton(AF_INET, SERVER_HOST, &serv_addr.sin_addr) <= 0) {
        fprintf(stderr, "Error: Could not convert host IP \"%s\"\n", SERVER_HOST);
        exit(1);
    }

    /* Connect the socket to the server */
    if ( connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        fprintf(stderr, "Error: connect() failed.  Is the server running?\n");
        exit(1);
    }

    /* Start the network thread */
    pthread_create(&network_thread, NULL, network_thread_f, NULL);

    /* Look for and handle keypresses */
    /* 
     * Backspace: 2a
     * leftarrow: 50
     * rightarrow: 4f
     * a: 04
     * z: 1d
     */
    char *editor = malloc(BUFFER_SIZE);  // don't forget to free this
    editor[0] = '\0';
    printf("%s\n", editor);
    int cursor = 0;
    uint8_t key;
    for (;;) {
        libusb_interrupt_transfer(keyboard, endpoint_address,
                (unsigned char *) &packet, sizeof(packet),
                &transferred, 0);
        if (transferred == sizeof(packet)) {
            sprintf(keystate, "%02x %02x %02x %02x %02x", packet.modifiers, packet.keycode[0],
                    packet.keycode[1], packet.keycode[2], packet.keycode[3]);
            printf("%s\n", keystate);
            printf("prev %02x %02x %02x %02x\n", prev_keycode[0],prev_keycode[1], prev_keycode[2], prev_keycode[3]);
            fbputs(keystate, 6, 0);
            printf("%s\n", editor);

            // compares state with previous buffer
            key = 0;
            for (int i = 0; i < 6; i++) {   // for each entry in keycode
                int unique = 1;
                for (int j = 0; j < 6; j++) {
                    if (packet.keycode[i] == prev_keycode[j]) {
                        unique = 0;
                        break;
                    }
                }
                if (unique) {
                    key = packet.keycode[i];
                    break;
                }
            }
            printf("keystate: %02x\n", key);

            // letter press
            if (key >= 0x04 && key <= 0x1d){
                // shift pressed
                if (packet.modifiers & 0x22) {
                    insert(editor, &cursor, 'A' + key - 0x04);
                }
                else {
                    insert(editor, &cursor, 'a' + key - 0x04);
                }
            }
            // TODO: number
            // TODO: pervent editor overflow
            // space
            else if (key == 0x2c) {
              insert(editor, &cursor, 0x20);
            }
            // backspace
            else if (key == 0x2a) {
              delete(editor, &cursor);
            }
            else if (key == 0x28) {  // Enter (Return) key
                 // If the user enters "/clear", clear the screen
                if (strcmp(editor, "clear") == 0) {
                    fbclear();  // Clear the entire VGA screen
                    memset(editor, 0, BUFFER_SIZE);  // Clear the input box
                    cursor = 0;
                }
                else if (strlen(editor) > 0) {
                    write(sockfd, editor, strlen(editor));  // Send a message to the server
            
                    // Display sent messages at the top of the screen
                    fbputchunk(editor, 0, 0, BUFFER_SIZE);
            
                    // Clear the input box (bottom area)
                    memset(editor, 0, BUFFER_SIZE);
                    fbclearln(12);
                    fbclearln(13);
                    cursor = 0;
                }
            }

            // write line
            fbclearln(12);
            fbclearln(13);
            fbputchunk(editor, 12, 0, 128);
            printf("%d\n", cursor);
            printf("%s\n", editor);
            if (packet.keycode[0] == 0x29) { /* ESC pressed? */
                break;
            }
            // updates previous keycode
            for (int i = 0; i < 6; i++) {
                prev_keycode[i] = packet.keycode[i];
            }
        }

    }
    free(editor);

    /* Terminate the network thread */
    pthread_cancel(network_thread);

    /* Wait for the network thread to finish */
    pthread_join(network_thread, NULL);

    return 0;
}

void *network_thread_f(void *ignored)
{
    char recvBuf[BUFFER_SIZE];
    int n;
    // /* Receive data */
    // while ( (n = read(sockfd, &recvBuf, BUFFER_SIZE - 1)) > 0 ) {
    //     recvBuf[n] = '\0';
    //     printf("%s", recvBuf);
    //     fbputs(recvBuf, 8, 0);
    // }
    int current_row = 0;

    while ((n = read(sockfd, recvBuf, BUFFER_SIZE - 1)) > 0) {
        recvBuf[n] = '\0';  
        printf("Received: %s\n", recvBuf);

        // Display server messages, each message is wrapped
        fbputchunk(recvBuf, current_row, 0, BUFFER_SIZE);
        current_row++;

        // If the message exceeds 8 lines, scroll the screen
        // if (current_row >= 8) {
        //     fbscroll();
        //     current_row = 7;
        // }
    }

    return NULL;
}

// void fbscroll() {
//     memmove(framebuffer, framebuffer + fb_finfo.line_length * FONT_HEIGHT, 
//             fb_finfo.smem_len - fb_finfo.line_length * FONT_HEIGHT);
    
//     fbclearln(7);  // Clear the last row
// }

/*
 * Inserts the character `text` to `buf`, at position specified by `cursor`
 * Cursor is the index of the buf array
 */
void insert(char *buf, int* cursor, const char text) {
    int len = strlen(buf);

    // don't forget \0
    if (len + 1 >= BUFFER_SIZE) {
        printf("Buffer full\n");
        return;
    }

    memmove(buf + *cursor + 1, buf + *cursor, len - *cursor + 1);
    buf[*cursor] = text;
    (*cursor)++;
}
void delete(char *buf, int* cursor) {
  int len = strlen(buf);

  // don't forget \0
  if (*cursor == 0) {
      printf("Empty\n");
      return;
  }

  memmove(buf + *cursor - 1, buf + *cursor, len - *cursor + 1);
  (*cursor)--;
}
