#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/select.h>
#include <linux/can.h>
#include <linux/can/raw.h>

unsigned short int volt[16][12];
unsigned short int maxvolt[16];
unsigned short int minvolt[16];
unsigned short int modvolt[16];
unsigned short int calcvolt[16];

signed char temp[16][12];
signed char maxtemp[16];
signed char mintemp[16];
signed char modtemp[16];

unsigned short int balance[16];
unsigned short int status[16];

int sock;
struct can_frame frame;

// ********************************************************************************
int main(int argc, char **argv)
{
    int ret;
    int id=-1;
    int nbytes;
    struct sockaddr_can addr;
    struct ifreq ifr;
    fd_set fdset;
    struct timeval timeout;

    memset(&frame, 0, sizeof(struct can_frame));

    system("sudo ip link set can0 type can bitrate 500000 >/dev/null 2>&1");
    system("sudo ifconfig can0 up >/dev/null 2>&1");

    //1.Create socket
    sock = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (sock < 0) {
        perror("socket PF_CAN failed");
        return 1;
    }

    //2.Specify can0 device
    strcpy(ifr.ifr_name, "can0");
    ret = ioctl(sock, SIOCGIFINDEX, &ifr);
    if (ret < 0) {
        perror("ioctl failed");
        return 1;
    }

    //3.Bind the socket to can0
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    ret = bind(sock, (struct sockaddr *)&addr, sizeof(addr));
    if (ret < 0) {
        perror("bind failed");
        return 1;
    }

    //4.Define receive rules
    struct can_filter rfilter[1];
    rfilter[0].can_id = 0;
    rfilter[0].can_mask = 0xf;
    setsockopt(sock, SOL_CAN_RAW, CAN_RAW_FILTER, &rfilter, sizeof(rfilter));

    if (argc!=2) {
	printf("usage: rlecid <id>\n");
	return 10;
    }
    id=atoi(argv[1]);

    printf("Initiating Security handshake\n");

    //packet 0x7e0
    frame.can_id = 0x7e0;
    frame.can_dlc = 8;
    frame.data[0] = 0x0d;
    frame.data[1] = 0x01;
    frame.data[2] = 0;
    frame.data[3] = 0;
    frame.data[4] = 0;
    frame.data[5] = 0;
    frame.data[6] = 0;
    frame.data[7] = 0;
    nbytes = write(sock, &frame, sizeof(frame)); 
    if(nbytes != sizeof(frame)) {
        printf("Send Error frame[0]!\r\n");
    }

    //5.Receive data
    do {
          FD_ZERO(&fdset); /* clear the set */
          FD_SET(sock, &fdset); /* add our file descriptor to the set */

          timeout.tv_sec = 0;
          timeout.tv_usec = 200000;

          ret = select(sock + 1, &fdset, NULL, NULL, &timeout);
          if(ret == -1)
              perror("select"); /* an error accured */
          else if(ret != 0) {
	     nbytes = read(sock, &frame, sizeof(frame));

	     if ((nbytes==sizeof(frame)) && (frame.data[0]==0x0d) && (frame.data[1]==1) && (frame.data[2]==0xAA)) {
		printf("Got challenge: 0x%02x%02x\n",frame.data[3],frame.data[4]);

    		//packet 0x7e0
    		frame.can_id = 0x7e0;
    		frame.can_dlc = 8;
    		frame.data[0] = 0xd;
    		frame.data[1] = 0x2;
    		frame.data[2] = (((~frame.data[4])&0xf)<<4) | (((~frame.data[3])>>4)&0xf);
    		frame.data[3] = (((~frame.data[3])&0xf)<<4) | (((~frame.data[4])>>4)&0xf);
    		frame.data[4] = 0;
    		frame.data[5] = 0;
    		frame.data[6] = 0;
    		frame.data[7] = 0;
    		nbytes = write(sock, &frame, sizeof(frame));
    		if(nbytes != sizeof(frame)) {
        	    printf("Send Error frame[0]!\r\n");
    		}
		printf("Send response: 0x%02x%02x\n",frame.data[2],frame.data[3]);

	     }
	     if ((nbytes==sizeof(frame)) && (frame.data[0]==0x0d) && (frame.data[1]==2) && (frame.data[2]==0xAA)) {
		printf("Passed security check!\n");
		printf("Changing RLEC id to %02d\n", id);

    		//packet 0x7e0
    		frame.can_id = 0x7e0;
    		frame.can_dlc = 8;
    		frame.data[0] = 0x4;
    		frame.data[1] = 0x15;
    		frame.data[2] = id;
    		frame.data[3] = 0;
    		frame.data[4] = 0;
    		frame.data[5] = 0;
    		frame.data[6] = 0;
    		frame.data[7] = 0;
    		nbytes = write(sock, &frame, sizeof(frame));
    		if(nbytes != sizeof(frame)) {
        	    printf("Send Error frame[0]!\r\n");
    		}

	     }
	     if ((nbytes==sizeof(frame)) && (frame.data[0]==4) && (frame.data[1]==0x15) && (frame.data[2]==0xAA)) {
		printf("Programming successfull!\n");
	     }

          }
      } while (ret!=0);

    // close socket
    close(sock);

    return 0;

}
