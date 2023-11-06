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

// fault bits (shown as "status):
// bit 0: Cell 1 redundant voltage measurement circuit A/D fault
// bit 1: Cell temperature measurement circuit A/D fault
// bit 2: RLEC board temperature measurement circuit A/D fault
// bit 3: High impedance cell voltage connection fault
// bit 4: Cell primary voltage measurement circuit A/D fault
// bit 5: Module voltage measurement circuit A/D fault
// bit 6: Zero capacitor voltage fault (Not in use)
// bit 7: Cell 1 redundant measurement differs from primary




// initial broadcast to "wake up" all RLECs.
// Balance target voltage is set here, so it is given as input parameter (in mV).

void broadcast(int volts) {
    int nbytes;
    volts=volts*100/244;

    // initial broadcast setup 0x7e1
    frame.can_id = 0x7e1;
    frame.can_dlc = 8;
    frame.data[0] = 1;	  // system state (1=ok)
    frame.data[1] = 12;	  // number of cells
    frame.data[2] = 12;	  // number of temp sensors
    frame.data[3] = 1;	  // slave balancing enable
    frame.data[4] = 1;	  // hybrid balancing enable
    frame.data[5] = 0;	  // unused status
    frame.data[6] = 0x05; // current min voltage in pack
    frame.data[7] = 0x00; //     (probably unused)

    nbytes = write(sock, &frame, sizeof(frame)); 
    if(nbytes != sizeof(frame)) {
        printf("Send Error frame[0]!\r\n");
    }

    // initial broadcast setup 0x7e2
    frame.can_id = 0x7e2;
    frame.can_dlc = 8;
    frame.data[0] = 0;   // hybrid balancing upper limit
    frame.data[1] = 10;
    frame.data[2] = 0;   // hybird balancing lower limit
    frame.data[3] = 10;
    frame.data[4] = 0;   // charging flag
    frame.data[5] = 0;   // charge state
    frame.data[6] = 1;   // charge enable
    frame.data[7] = 0;   // charge enable off cold

    nbytes = write(sock, &frame, sizeof(frame)); 
    if(nbytes != sizeof(frame)) {
        printf("Send Error frame[0]!\r\n");
    }

    // initial broadcast setup 0x7e3
    frame.can_id = 0x7e3;
    frame.can_dlc = 8;
    frame.data[0] = volts>>8;  //balance target upper
    frame.data[1] = volts&0xff;
    frame.data[2] = volts>>8;  //balance target lower
    frame.data[3] = volts&0xff;
    frame.data[4] = 0;    // balancing differential limit
    frame.data[5] = 9;
    frame.data[6] = 0;    // balancing differential voltage
    frame.data[7] = 3;

    nbytes = write(sock, &frame, sizeof(frame)); 
    if(nbytes != sizeof(frame)) {
        printf("Send Error frame[0]!\r\n");
    }

    // initial broadcast setup 0x7e4
    frame.can_id = 0x7e4;
    frame.can_dlc = 8;
    frame.data[0] = 0;    // pre-balance delta
    frame.data[1] = 9;
    frame.data[2] = 0x46; // temp limit
    frame.data[3] = 0x2d; // cell voltage limit
    frame.data[4] = 10;   // max resistance temp adj
    frame.data[5] = 2;    // temp adjusted hysteresis
    frame.data[6] = 0;    // temp adjsuted resistance time
    frame.data[7] = 0x4b;

    nbytes = write(sock, &frame, sizeof(frame)); 
    if(nbytes != sizeof(frame)) {
        printf("Send Error frame[0]!\r\n");
    }

    // initial broadcast setup 0x7e5
    frame.can_id = 0x7e5;
    frame.can_dlc = 8;
    frame.data[0] = 5;
    frame.data[1] = 2;
    frame.data[2] = 0x04;  //3.0V  disable balancing
    frame.data[3] = 0xce;
    frame.data[4] = volts>>8;  //balance target
    frame.data[5] = volts&0xff;
    frame.data[6] = 0x07;  // current max voltage in pack
    frame.data[7] = 0x53;

    nbytes = write(sock, &frame, sizeof(frame)); 
    if(nbytes != sizeof(frame)) {
        printf("Send Error frame[0]!\r\n");
    }

}


// ********************************************************************************
// takes target balance voltage as arguments, in mV
int main(int argc, char **argv)
{
    int ret,volts=4100,balancecells=0;
    int r;
    int nbytes;
    struct sockaddr_can addr;
    struct ifreq ifr;
    fd_set fdset;
    struct timeval timeout;
    int packmaxvolt=0, packminvolt=5000, packmaxtemp=-200, packmintemp=200, packstatus=0,totalvolts;
    FILE *fh;
    time_t now = time(NULL);
    struct tm tm = *localtime(&now);

    if (argc==2) volts=atoi(argv[1]);

    memset(&frame, 0, sizeof(struct can_frame));

    // rasp can setup, could be moved elsewhere..
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
    rfilter[0].can_mask = 0x600 ;
    setsockopt(sock, SOL_CAN_RAW, CAN_RAW_FILTER, &rfilter, sizeof(rfilter));

    // First send broadcast to every RLEC
    broadcast(volts);

    // Loop all rlec's
    // Send a few packets and wait for response
    for (r=0;r<16;r++) {
        //packet 0x406
        frame.can_id = 0x406+r*32;
        frame.can_dlc = 8;
        frame.data[0] = 0;  // 1=balance override
        frame.data[1] = 0;  // override mask 9-12
        frame.data[2] = 0;  // override mask 1-8
        frame.data[3] = 0;  // output 9-12
        frame.data[4] = 0;  // output 1-8
        frame.data[5] = 0;  // cell vol override ctl
        frame.data[6] = 0;  // cell volt override mask 9-12
        frame.data[7] = 0;  // cell volt override mask 1-8

        nbytes = write(sock, &frame, sizeof(frame));
        if(nbytes != sizeof(frame)) {
            printf("Send Error frame[0]!\r\n");
        }

        //packet 0x40a
        frame.can_id = 0x40a+r*32;
        frame.can_dlc = 8;
        frame.data[0] = 0;  // input override mask
        frame.data[1] = 0;
        frame.data[2] = 0;  // cell1 diag oerride
        frame.data[3] = 0;
        frame.data[4] = 0;  // zero cap override
        frame.data[5] = 0;
        frame.data[6] = 0;  // module volt overide
        frame.data[7] = 0;
        nbytes = write(sock, &frame, sizeof(frame));
        if(nbytes != sizeof(frame)) {
            printf("Send Error frame[0]!\r\n");
        }

        //packet 0x40b
        frame.can_id = 0x40b+r*32;
        frame.can_dlc = 8;
        frame.data[0] = 0;  // cell temp overrides 1-8
        frame.data[1] = 0;
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

        //packet 0x40c
        frame.can_id = 0x40c+r*32;
        frame.can_dlc = 8;
        frame.data[0] = 0;  // cell temp overrides 9-12
        frame.data[1] = 0;
        frame.data[2] = 0;
        frame.data[3] = 0;
        frame.data[4] = 0;  // rlec temp override
        frame.data[5] = 0;  // heater override
        frame.data[6] = 12; // number of cells
        frame.data[7] = 12; // number of cell temps
        nbytes = write(sock, &frame, sizeof(frame));
        if(nbytes != sizeof(frame)) {
            printf("Send Error frame[0]!\r\n");
        }

        //5.Receive data
        do {
          FD_ZERO(&fdset); /* clear the set */
          FD_SET(sock, &fdset); /* add our file descriptor to the set */

          timeout.tv_sec = 0;
          timeout.tv_usec = 20000;

          ret = select(sock + 1, &fdset, NULL, NULL, &timeout);
          if(ret == -1)
              perror("select"); /* an error accured */
          else if(ret != 0) {
              nbytes = read(sock, &frame, sizeof(frame));

	      switch(frame.can_id - r*32) {
	        case 0x001:
  	        case 0x002:
 	        case 0x003:
		  volt[r][0+((frame.can_id&3)-1)*4]=(frame.data[0]*256 + frame.data[1])*244/100;
		  volt[r][1+((frame.can_id&3)-1)*4]=(frame.data[2]*256 + frame.data[3])*244/100;
		  volt[r][2+((frame.can_id&3)-1)*4]=(frame.data[4]*256 + frame.data[5])*244/100;
		  volt[r][3+((frame.can_id&3)-1)*4]=(frame.data[6]*256 + frame.data[7])*244/100;
		  break;

	        case 0x004:
		  maxvolt[r]=(frame.data[0]*256 + frame.data[1])*244/100;
		  minvolt[r]=(frame.data[2]*256 + frame.data[3])*244/100;
		  modtemp[r]=frame.data[4];
		  balance[r]=frame.data[5]*256 + frame.data[6];
		  balancecells+=(balance[r]&1)+((balance[r]&2)>>1)+((balance[r]&4)>>2)+((balance[r]&8)>>3)+
				((balance[r]&16)>>4)+((balance[r]&32)>>5)+((balance[r]&64)>>6)+((balance[r]&128)>>7)+
				((balance[r]&256)>>8)+((balance[r]&512)>>9)+((balance[r]&1024)>>10)+((balance[r]&2048)>>11);

		  status[r]=frame.data[7];
		  break;

	        case 0x00b:
		  modvolt[r]=(frame.data[6]*256 + frame.data[7])*122/10;
		  break;

	        case 0x00c:
		  memcpy(&temp[r][0],&frame.data[0],8);
		  break;

	        case 0x00d:
		  memcpy(&temp[r][8],&frame.data[0],4);
		  maxtemp[r]=frame.data[4];
		  mintemp[r]=frame.data[5];
		  break;
	      }
          }
        } while (ret!=0);
    }

    // Figure out whole pack min and max
    // Also global status
    packmaxvolt=0;
    packminvolt=5000;
    packmaxtemp=-200;
    packmintemp=200;
    packstatus=0;
    totalvolts=0;

    for (r=0;r<16;r++) if (minvolt[r] && maxvolt[r]) {
	int i;

	calcvolt[r]=0;
	for (i=0;i<12;i++) calcvolt[r]+=volt[r][i];
	if ((calcvolt[r]>totalvolts)) totalvolts=calcvolt[r];

	if (mintemp[r]<packmintemp) packmintemp=mintemp[r];
	if (maxtemp[r]>packmaxtemp) packmaxtemp=maxtemp[r];
	if (minvolt[r]<packminvolt) packminvolt=minvolt[r];
	if (maxvolt[r]>packmaxvolt) packmaxvolt=maxvolt[r];

	packstatus|=status[r];
    }

    // print result
    fh=stdout;
    if (fh) {
        fprintf(fh,"Balance target %d mV, balanced cells: %d\n",volts,balancecells);
	for (r=0;r<16;r++) {

	  if (minvolt[r] && maxvolt[r]) fprintf(fh,
		"Module %02d    Reported: %5.2f V   Calculated: %5.2f V     %2d C    Status 0x%x\n"
		"  T:  %4d  %4d  %4d  %4d   %4d  %4d  %4d  %4d   %4d  %4d  %4d  %4d  (%4d-%4d)\n"
		"  V: %c%4d %c%4d %c%4d %c%4d  %c%4d %c%4d %c%4d %c%4d  %c%4d %c%4d %c%4d %c%4d  (%4d-%4d = %4d mV)\n",

		r, (double)modvolt[r]/1000.0, (double)calcvolt[r]/1000.0, modtemp[r], status[r],
		temp[r][0], temp[r][1], temp[r][2], temp[r][3], temp[r][4], temp[r][5], temp[r][6], temp[r][7], temp[r][8], temp[r][9], temp[r][10], temp[r][11], mintemp[r], maxtemp[r],

		(balance[r]&1)?'>':' ',	volt[r][0],
		(balance[r]&2)?'>':' ', volt[r][1],
		(balance[r]&4)?'>':' ', volt[r][2],
		(balance[r]&8)?'>':' ', volt[r][3],
		(balance[r]&16)?'>':' ',volt[r][4],
		(balance[r]&32)?'>':' ',volt[r][5],
		(balance[r]&64)?'>':' ',volt[r][6],
		(balance[r]&128)?'>':' ',volt[r][7],
		(balance[r]&256)?'>':' ',volt[r][8],
		(balance[r]&512)?'>':' ',volt[r][9],
		(balance[r]&1024)?'>':' ',volt[r][10],
		(balance[r]&2048)?'>':' ',volt[r][11],
		maxvolt[r], minvolt[r], maxvolt[r]-minvolt[r]);

      }

      fprintf(fh,"%02d.%02d %02d:%02d: ", tm.tm_mday, tm.tm_mon + 1, tm.tm_hour, tm.tm_min);
      fprintf(fh,"Cell (%d-%d)=%d mV,  Temp (%d-%d),  Pack %5.2f V,  Status 0x%x\n",packminvolt,packmaxvolt,packmaxvolt-packminvolt,packmintemp,packmaxtemp,totalvolts/1000.0, packstatus);
      if (fh!=stdout) fclose(fh);
    }


    // close socket
    close(sock);

    // fix packstatus to <127
    if (packstatus&0xff) return 64; else return (packstatus>>8);
}
