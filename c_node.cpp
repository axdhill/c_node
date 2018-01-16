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


template <typename T> int sgn(T val) {
    return (T(0) < val) - (val < T(0));
}


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


    uint8_t id;
    int32_t adc[8];
    float volt[8];
    uint8_t i;
    uint8_t ch_num;
    int32_t iTemp;
    uint8_t buf[3];
    if (!bcm2835_init())
        return;
    bcm2835_spi_begin();
    bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_LSBFIRST );      // The default
    bcm2835_spi_setDataMode(BCM2835_SPI_MODE1);                   // The default
    bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_1024); // The default
    bcm2835_gpio_fsel(SPICS, BCM2835_GPIO_FSEL_OUTP);//
    bcm2835_gpio_write(SPICS, HIGH);
    bcm2835_gpio_fsel(DRDY, BCM2835_GPIO_FSEL_INPT);
    bcm2835_gpio_set_pud(DRDY, BCM2835_GPIO_PUD_UP);

    id = ADS1256_ReadChipID();
    printf("\r\n");
    printf("ID=\r\n");
    if (id != 3)
    {
        printf("Error, ASD1256 Chip ID = 0x%d\r\n", (int)id);
    }
    else
    {
        printf("Ok, ASD1256 Chip ID = 0x%d\r\n", (int)id);
    }
    ADS1256_CfgADC(ADS1256_GAIN_1, ADS1256_7500SPS);
    ADS1256_StartScan(0);
    ch_num = 8;

    int sendbuf_n;
    ADS1256_VAR_T* g_tADS1256 = get_state();



    timespec deadline;
    deadline.tv_sec = 0;
    deadline.tv_nsec = 2000000;    
    while(!terminateProgram) {

        for( int i = 0; i < 3; i++) {
            while((DRDY_IS_LOW() == 0));

            ADS1256_SetChannel(i);	/*Switch channel mode */
    		bsp_DelayUS(25);
            adc[i] = ADS1256_ReadData();
            volt[i] = (adc[i] * 100.) / 167.;
        }



        bzero(buffer,256);
        sprintf(buffer,"%0.8f %0.8f %0.8f",volt[0]/1000000.,volt[1]/1000000.,volt[2]/1000000.);
        //printf("\t%s\n",buffer);
        j++;
        //t_now = std::chrono::high_resolution_clock::now();

        sendbuf_n = sendto(fd, buffer, strlen(buffer), 0, (struct sockaddr *)&servaddr, sizeof(servaddr));

        //printf("\t%i\n", sendbuf_n);

        clock_nanosleep(CLOCK_REALTIME,0,&deadline,NULL);

    }
    bcm2835_spi_end();
    bcm2835_close();

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


void acquire(serialib* ser1, serialib* ser2) {
	printf("ACQUIRE:::\n");
    char buf[256];
    int errorX,errorY;

    float KdX = 0.0;
    float KdY = 0.0;
    float DARK_LEVEL = 0.15;
    float value1,value2,value3,v1norm,v2norm,v1_v,v2_v,v1norm_old,v2norm_old,v3_old;
    value3 = 0;
	std::string errXstr;
    std::string errYstr;
    std::vector<float> v;
    std::string bufstr;
    int d = 0;
    float r = 20.;
    float th = 0.;
    int i = 0;
    while(value3 < DARK_LEVEL) {

        pos_buffer_mutex.lock();
        memcpy(buf, pos_buffer,256);
        //printf("pos_buffer:%s\n",pos_buffer);
        pos_buffer_mutex.unlock();
        //printf("2\n");

        bufstr = std::string(buf);
        //printf("3\n");
        //printf("%s\n",bufstr.c_str());
        if(bufstr == "") {
            std::this_thread::sleep_for (std::chrono::milliseconds(50));
            continue;
        }

        std::istringstream processbuf(bufstr);
        //printf("processbuf:%s\n",processbuf.str().c_str());

        std::copy(std::istream_iterator<float>(processbuf),
        std::istream_iterator<float>(),
        std::back_inserter(v));
        //printf("5\n");
        printf("%0.8f,%0.8f,%0.8f\n",v[0],v[1],v[2]);
        value3 = v[2];
        v.clear();

        value3 = float(value3);

        i++;
	    th = fmod((th+30.0/(pow(r,(1/3.)))),360);
        r += 0.25;


        errorX = round(r*cos(th*3.14159/180.));
        errorY = round(r*sin(th*3.14159/180.));
        
        errXstr = std::to_string(errorX);
        errYstr = std::to_string(errorY);

        if(errorX > 0) {
            errXstr = "+"+errXstr;
        } else {
            errXstr = errXstr;
        }
        if(errorY > 0) {
            errYstr = "+"+errYstr;
        } else {
            errYstr = errYstr;
        }

        if(errorX != 0) {
            ser2->WriteString((errXstr+";").c_str());
        }
        if(errorY != 0) {
            ser1->WriteString((errYstr+";").c_str());
        }
        std::this_thread::sleep_for (std::chrono::milliseconds(20));
    }

    return;

}

void feedback() {
    signal (SIGINT, CtrlHandler);
    signal (SIGQUIT, CtrlHandler);
    float KpX = 0.0;
    float KpY = 0.0;

    std::ifstream configFile;
    configFile.open("./pidconfig.txt");
    if(!configFile) {
        printf("Unable to open config file\n");
        exit(0);
    }
    int j = 0;
    configFile >> KpX;
    configFile >> KpY;

    configFile.close();
    printf("||||| K: %f %f\n",KpX,KpY);

    printf("feedback loop\n");

    serialib ser1;
    serialib ser2;

    int ret;
    char ser_buf[128];
    ret = ser1.Open(SER1_PORT,57600);
    // Open serial link at 115200 bauds
    if (ret != 1) {                                                           // If an error occured...
        printf ("Error while opening port. Permission problem ?\n");        // ... display a message ...
        return;                                                         // ... quit the application
    }
    printf ("Serial port opened successfully !\n");

    ret = ser2.Open(SER2_PORT,57600);                                        // Open serial link at 115200 bauds
    if (ret != 1) {                                                           // If an error occured...
        printf ("Error while opening port. Permission problem ?\n");        // ... display a message ...
        return;                                                         // ... quit the application
    }
    printf ("Serial port opened successfully !\n");

    ser1.WriteString("C33H1024c;");
    ser2.WriteString("C33H1024c;");

    char buf[256];
    int errorX,errorY;


    float KdX = 0.0;
    float KdY = 0.0;
    float DARK_LEVEL = 0.1;
    float value1,value2,value3,v1norm,v2norm,v1_v,v2_v,v1norm_old,v2norm_old,v3_old;
    std::string errXstr;
    std::string errYstr;
    std::vector<float> v;
    printf("Loop running...\n");
    std::string bufstr;
    int d = 0;

    float Ix = 0.0;
    float Iy = 0.0;

    timespec deadline;
    deadline.tv_sec = 0;
    deadline.tv_nsec = 20000000; 
    while(!terminateProgram) {
        //printf("1\n");

        pos_buffer_mutex.lock();
        memcpy(buf, pos_buffer,256);
        //printf("pos_buffer:%s\n",pos_buffer);
        pos_buffer_mutex.unlock();
        //printf("2\n");

        bufstr = std::string(buf);
        //printf("3\n");
        //printf("%s\n",bufstr.c_str());
        if(bufstr == "") {
            clock_nanosleep(CLOCK_REALTIME,0,&deadline,NULL);
            continue;
        }

        std::istringstream processbuf(bufstr);
        //printf("processbuf:%s\n",processbuf.str().c_str());

        std::copy(std::istream_iterator<float>(processbuf),
        std::istream_iterator<float>(),
        std::back_inserter(v));
        //printf("5\n");
//        printf("%0.8f,%0.8f,%0.8f\n",(v[0])/v[2],(v[1])/v[2],v[2]);

        value1 = v[0];
        value2 = v[1];
        value3 = v[2];
        v.clear();
        value1 = float(value1)-2.385;
        value2 = float(value2)-2.385;
        value3 = float(value3);

        v1norm = value1/value3;
        v2norm = value2/value3;

        // printf("%0.8f,%0.8f,%0.8f\n",v1norm,v2norm,v[2]);

        v1norm = sgn(v1norm)*pow(fabs(v1norm),6.0/5.);
        v2norm = sgn(v2norm)*pow(fabs(v2norm),6.0/5.);
        printf("%0.8f,%0.8f,%0.8f\n",v1norm,v2norm,v[2]);

        v1_v = (v1norm - v1norm_old);
        v2_v = (v2norm - v2norm_old);

        errorX = int(round(v1norm * KpX  ));
        errorY = int(round(v2norm * KpY ));

        v1norm_old = v1norm;
        v2norm_old = v2norm;
        if(value3 < DARK_LEVEL) {
            printf("LOST SIGNAL\n");
            clock_nanosleep(CLOCK_REALTIME,0,&deadline,NULL);

            //acquire(&ser1, &ser2);
            continue;
        }



        // Ix += v1norm/100.0;
        // Iy += v2norm/100.0;

        errXstr = std::to_string(errorX);
        errYstr = std::to_string(errorY);
	    printf("%i %i\n", errorX,errorY);
        if(errorX > 0) {
            errXstr = "+"+errXstr;
        } else {
            errXstr = errXstr;
        }
        if(errorY > 0) {
            errYstr = "+"+errYstr;
        } else {
            errYstr = errYstr;
        }

        if(errorX != 0) {
            ser2.WriteString((errXstr+";").c_str());
        }
        if(errorY != 0) {
            ser1.WriteString((errYstr+";").c_str());
        }

        v1norm_old = v1norm;
        v2norm_old = v2norm;
        v3_old = value3;



        clock_nanosleep(CLOCK_REALTIME,0,&deadline,NULL);

    }
    // ser1.close();
    // ser2.close();
    return;
}

int main(int argc, char** argv) {
    signal (SIGINT, CtrlHandler);
    signal (SIGQUIT, CtrlHandler);

    memcpy(UDP_IP, argv[1],256);
    portno = atoi(argv[2]);
    portno_tx = atoi(argv[3]);


    printf("%s\t%i\t%i\n",UDP_IP,portno,portno_tx);

    //std::thread t_transmit_qc(transmit_qc);
    std::thread t_receive(receive_data);
    std::thread t_feedback(feedback);

    // Initialize variables

    //t_transmit_qc.join();
    t_receive.join();
    t_feedback.join();

    return 0;
}
