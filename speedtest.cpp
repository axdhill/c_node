/*
@author Alex Hill 2017
*/


#include <chrono>
#include <cstdlib>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <thread>
#include <mutex>
#include <netdb.h>
#include <vector>
#include <string>
#include <sstream>
#include <iterator>
#include <fstream>
#include <sys/stat.h>
#include <fcntl.h>
#include <cmath>
#include "serialib.h"
#include "adc_lib.h"


#define  SER1_PORT   "/dev/ttyUSB1"
#define SER2_PORT "/dev/ttyUSB0"
std::mutex pos_buffer_mutex;//you can use std::lock_guard if you want to be exception safe
char pos_buffer[256];


int portno_tx;
int portno;
char UDP_IP[256];


bool terminateProgram = false;
void CtrlHandler(int signum) {
    terminateProgram = true;
}
void transmit_qc() {
    signal (SIGINT, CtrlHandler);
    signal (SIGQUIT, CtrlHandler);


    int fd;
    if ((fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        printf("cannot create socket");
        return;
    }
    struct hostent *hp;     /* host information */
    struct sockaddr_in servaddr;    /* server address */
    //char *my_messsage = "this is a test message";

    /* fill in the server's address and data */
    memset((char*)&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(portno_tx);
    int size = 8192;
    setsockopt(fd, SOL_SOCKET, SO_SNDBUF,  &size, sizeof(int));

    /* look up the address of the server given its name */
    hp = gethostbyname(UDP_IP);
    if (!hp) {
        fprintf(stderr, "could not obtain address of %s\n", UDP_IP);
        return;
    }
    char buffer[256];
    /* put the host's address into the server address structure */
    memcpy((void *)&servaddr.sin_addr, hp->h_addr_list[0], hp->h_length);
    int j = 0;
    printf("Tx Running\n");
    std::chrono::time_point<std::chrono::high_resolution_clock> t_now;
    long l_time;


    timespec deadline;
    deadline.tv_sec = 0;
    deadline.tv_nsec = 20000000;    
    uint8_t id;
    int32_t adc[8];
    float volt[8];
    uint8_t i;
    uint8_t ch_num;
    int32_t iTemp;
    uint8_t buf[3];

    int sendbuf_n;
    float a = 0;
    while(!terminateProgram) {
       	unsigned total_written = 0;
		//a = ((float)std::rand())/((float)RAND_MAX),
		float  b = ((float)std::rand())/((float)RAND_MAX), c = ((float)std::rand())/((float)RAND_MAX);
		// a=std::time(nullptr);
		sprintf(buffer,";%f,%f,%f;",a/10.,b,c);
		// sprintf(send_buf,"%f",a);
		a++;
        //t_now = std::chrono::high_resolution_clock::now();
        sendbuf_n = sendto(fd, buffer, strlen(buffer), 0, (struct sockaddr *)&servaddr, sizeof(servaddr));

        //printf("\t%i\n", sendbuf_n);

        clock_nanosleep(CLOCK_REALTIME,0,&deadline,NULL);
    }

    return;

}



void receive_data() {
    signal (SIGINT, CtrlHandler);
    signal (SIGQUIT, CtrlHandler);
    struct sockaddr_in myaddr;      /* our address */
    struct sockaddr_in remaddr;     /* remote address */
    socklen_t addrlen = sizeof(remaddr);            /* length of addresses */
    int recvlen;                    /* # bytes received */
    int fd;                         /* our socket */
    char buf[256];     /* receive buffer */

    /* create a UDP socket */

    if ((fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        perror("cannot create socket\n");
        return;
    }

    /* bind the socket to any valid IP address and a specific port */

    memset((char *)&myaddr, 0, sizeof(myaddr));
    myaddr.sin_family = AF_INET;
    myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    myaddr.sin_port = htons(portno);

    if (bind(fd, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0) {
        perror("bind failed");
        return;
    }



    printf("Rx running...\n");
    while(!terminateProgram) {
        //printf("waiting on port %d\n", portno);
        //while(true) {
        recvlen = recvfrom(fd, buf, 256, 0, (struct sockaddr *)&remaddr, &addrlen);
        // if(recvlen <= 0 ){
        //     break;
        // }
        //}
        buf[recvlen] = 0;
        //printf("received %d bytes\n", recvlen);
        //if (recvlen > 0) {

        //printf("received message: \"%s\"\n", buf);
        //}
        printf("%s\n",buf);

        pos_buffer_mutex.lock();
        bzero(pos_buffer,256);
        memcpy(pos_buffer, buf, 256);
        pos_buffer_mutex.unlock();

        //std::this_thread::sleep_for (std::chrono::milliseconds(5));
    }
    close(fd);
    printf("Killed vicon thread\n");
    return;
}


int main(int argc, char** argv) {
    signal (SIGINT, CtrlHandler);
    signal (SIGQUIT, CtrlHandler);

    memcpy(UDP_IP, argv[1],256);
    portno = atoi(argv[2]);
    portno_tx = atoi(argv[3]);


    printf("%s\t%i\t%i\n",UDP_IP,portno,portno_tx);

    std::thread t_transmit_qc(transmit_qc);
    std::thread t_receive(receive_data);
    // std::thread t_feedback(feedback);

    // Initialize variables

    //t_transmit_qc.join();
    t_receive.join();

    return 0;
}
