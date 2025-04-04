/*
 *
 * CSEE 4840 Lab 2 for 2019
 *
 * Name/UNI: Ming Gong (mg4264), Tianshuo Jin (tj2591, Zidong Xu (zx2507)
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
#define RECV_BUFFER_SIZE 192

/*
 * References:
 *
 * https://web.archive.org/web/20130307100215/http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html
 *
 * http://www.thegeekstuff.com/2011/12/c-socket-programming/
 * 
 */
int current_row = 0;
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
    // char keystate[12];

    if ((err = fbopen()) != 0) {
        fprintf(stderr, "Error: Could not open framebuffer: %d\n", err);
        exit(1);
    }

    // Initialize screen on program startup
    fbclear();
    for (col = 0 ; col < 64 ; col++) {
        fbputchar('-', 0, col);
        fbputchar('-', 21, col);
    }

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

    // editor buffer for input
    char *editor = malloc(BUFFER_SIZE);
    editor[0] = '\0';

    // cursor variable
    int cursor = 0;
    uint8_t key;
    for (;;) {
        libusb_interrupt_transfer(keyboard, endpoint_address,
                (unsigned char *) &packet, sizeof(packet),
                &transferred, 0);
        if (transferred == sizeof(packet)) {
         /* 
            sprintf(keystate, "%02x %02x %02x %02x %02x", packet.modifiers, packet.keycode[0],
                    packet.keycode[1], packet.keycode[2], packet.keycode[3]);
            printf("%s\n", keystate);
            printf("prev %02x %02x %02x %02x\n", prev_keycode[0],prev_keycode[1], prev_keycode[2], prev_keycode[3]);
            // fbputs(keystate, 6, 0);
        */

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
            // printf("keystate: %02x\n", key);

            switch (key) {
                // letter press
                case 0x04 ... 0x1d:
                    if (packet.modifiers & 0x22) {
                        insert(editor, &cursor, 'A' + key - 0x04);
                    }
                    else {
                        insert(editor, &cursor, 'a' + key - 0x04);
                    }
                    break;

                // number input
                case 0x1e ... 0x27:
                    if (packet.modifiers & 0x22) {
                        insert(editor, &cursor, "!@#$%^&*()"[key - 0x1e]);  // Shifted symbols
                    } else {
                        insert(editor, &cursor, "1234567890"[key - 0x1e]);        // Digits
                    }
                    break;

                // return key to send
                case 0x28:
                    if (strlen(editor) > 0) {
                        write(sockfd, editor, strlen(editor));  // Send a message to the server
                        // Reset editor
                        editor[0] = '\0';
                        cursor = 0;
                    }
                    break;
                
                case 0x29:
                    free(editor);
                    goto exit;

                // backspace delete
                case 0x2a:
                    delete(editor, &cursor);
                    break;

                // other symbols and space
                case 0x2c ... 0x31:
                    {
                        const char symbols[] = " -=[]\\";
                        const char shifted_symbols[] = " _+{}|";
                        insert(editor, &cursor, (packet.modifiers & 0x22) ? shifted_symbols[key - 0x2c] : symbols[key - 0x2c]);
                    }
                    break;

                case 0x33 ... 0x38:
                    {
                        const char symbols[] = ";\'`,./";
                        const char shifted_symbols[] = ":\"~<>?";
                        insert(editor, &cursor, (packet.modifiers & 0x22) ? shifted_symbols[key - 0x33] : symbols[key - 0x33]);
                    }
                    break;

                // arrow press for cursor movement
                case 0x4f:
                    if (cursor < strlen(editor)) 
                        cursor++;
                    break;

                case 0x50:
                    if (cursor > 0) 
                        cursor--; 
                    break;

                default:
                    break;
            }

            // write line from the editor
            fbclearln(22);
            fbclearln(23);
            fbputeditor(editor, &cursor, 22, 0, strlen(editor));
            // printf("%d\n", cursor);
            // printf("%s\n", editor);

            // updates previous keycode
            for (int i = 0; i < 6; i++) {
                prev_keycode[i] = packet.keycode[i];
            }
        }

    }

exit:

    /* Terminate the network thread */
    pthread_cancel(network_thread);

    /* Wait for the network thread to finish */
    pthread_join(network_thread, NULL);

    close(sockfd);

    return 0;
}

void *network_thread_f(void *ignored)
{
    char recvBuf[RECV_BUFFER_SIZE];
    int n;
    int lines;

    while ((n = read(sockfd, recvBuf, RECV_BUFFER_SIZE)) > 0) {
        recvBuf[n] = '\0';  
        printf("Received: %s\n", recvBuf);

        lines = n % 64 ? n / 64 + 1 : n / 64;

        fbscroll(1, 20, lines * (-1));
        // Display server messages, each message is wrapped
        fbputchunk(recvBuf, 21 - lines, 0, RECV_BUFFER_SIZE);
        printf("%s\n", recvBuf);
    }

    return NULL;
}

void insert(char *buf, int* cursor, const char text) {
    int len = strlen(buf);

    // Make sure the buffer does not overflow, leaving extra space for '\0' and '|')
    if (len + 1 >= BUFFER_SIZE) {
        printf("Buffer full\n");
        return;
    }

    // Shift the string right
    memmove(buf + *cursor + 1, buf + *cursor, len - *cursor + 1);

    buf[*cursor] = text;
    (*cursor)++;
}

void delete(char *buf, int* cursor) {
  int len = strlen(buf);
  if (*cursor == 0) {
      printf("Empty\n");
      return;
  }

  memmove(buf + *cursor - 1, buf + *cursor, len - *cursor + 1);
  (*cursor)--;
}
