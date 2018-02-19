#include "platform.H"
#include <stdint.h>
#include <time.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <assert.h>

typedef unsigned long ul;
typedef unsigned int ui;
typedef uint32_t u32;
typedef uint32_t u16;
typedef uint64_t u64;
typedef uint8_t u8;


//============== orange pi zero
#define OPI_GPIO_BASE_BP	(0x01C20000)
#define OPI_GPIO_OFFSET (0x0800)

// use the gpio numbers listed in the "H2+" column of wiringOP "gpio readall" output
// ALL GPIOS MUST BE FROM THE SAME BANK!!!
// bank number is gpio number / 32
u32 OPI_GPIOTCK=14;
u32 OPI_GPIOTMS=16;
u32 OPI_GPIOTDI=13;
u32 OPI_GPIOTDO=2;
u32 OPI_GPIOMASK = -1;

volatile u32* opiGpio=NULL;
u64 ioDelayCycles=0;



void doDelay(u64 cycles) {
	for(u64 i=0;i<cycles;i++) asm volatile("");
}
u64 tsToNs(const struct timespec& ts) {
	return u64(ts.tv_sec)*1000000000+ts.tv_nsec;
}
//returns time in ns
u64 measureTime(u64 cycles) {
	struct timespec t,t2;
	clock_gettime(CLOCK_MONOTONIC,&t);
	for(int i=0;i<50;i++) {
		doDelay(cycles); doDelay(cycles); doDelay(cycles); doDelay(cycles);
		doDelay(cycles); doDelay(cycles); doDelay(cycles); doDelay(cycles);
		doDelay(cycles); doDelay(cycles); doDelay(cycles); doDelay(cycles);
		doDelay(cycles); doDelay(cycles); doDelay(cycles); doDelay(cycles);
		doDelay(cycles); doDelay(cycles); doDelay(cycles); doDelay(cycles);
		doDelay(cycles); doDelay(cycles); doDelay(cycles); doDelay(cycles);
		doDelay(cycles); doDelay(cycles); doDelay(cycles); doDelay(cycles);
		doDelay(cycles); doDelay(cycles); doDelay(cycles); doDelay(cycles);
	}
	clock_gettime(CLOCK_MONOTONIC,&t2);
	return (tsToNs(t2)-tsToNs(t))/(32*50);
}
u64 measureTime2(u64 cycles) {
	u64 min=measureTime(cycles);
	u64 tmp;
	for(int i=0;i<10;i++)
		if((tmp=measureTime(cycles))<min) min=tmp;
	return min;
}
//desiredDelay is in ns
//returns cycles to be passed to doDelay()
u64 calibrateDelay(u64 desiredDelay=1000) {
	fprintf(stderr,"calibrating delay loop... ");
	fflush(stderr);
	
	u64 d=2;
	while(true) {
		if(measureTime2(d)>desiredDelay) break;
		d*=2;
	}
	fprintf(stderr,"d=%llu\n",(unsigned long long)d);
	//binary search
	u64 min=d/2,max=d;
	for(int i=0;i<32;i++) {
		u64 mid=(min+max)/2;
		if(measureTime2(mid)>desiredDelay) {
			//too long
			max=mid;
		} else {
			min=mid;
		}
	}
	fprintf(stderr,"done; %llu ns = %llu cycles\n",
		(unsigned long long)measureTime(max),(unsigned long long)max);
	return max;
}

int platform_init() {
	int halfperiod=500; //ns
	int fd;
	if((fd = open("/dev/mem", O_RDWR | O_SYNC)) < 0) {
		fprintf(stderr, "ERROR: could not open \"/dev/mem\"...\n" ); return 1;
	}
	
	u8* hwreg = (u8*)mmap( NULL, 4096, ( PROT_READ | PROT_WRITE ), MAP_SHARED, fd, OPI_GPIO_BASE_BP);
	
	int bank = OPI_GPIOTCK >> 5;
	// make sure all gpios are on the same bank
	assert(bank == (OPI_GPIOTDI>>5));
	assert(bank == (OPI_GPIOTDO>>5));
	assert(bank == (OPI_GPIOTMS>>5));
	
	OPI_GPIOTCK -= bank;
	OPI_GPIOTDI -= bank;
	OPI_GPIOTDO -= bank;
	OPI_GPIOTMS -= bank;
	
	OPI_GPIOMASK = (1<<OPI_GPIOTCK)|(1<<OPI_GPIOTDI)|(1<<OPI_GPIOTDO)|(1<<OPI_GPIOTMS);
	
	opiGpio=(volatile u32*)(hwreg + OPI_GPIO_OFFSET + bank*36 + 0x10);
	int mask1 = (*opiGpio) & (~OPI_GPIOMASK);
	*opiGpio = mask1 | (1<<OPI_GPIOTCK);
	
	auto setGpioMode = [&](int pin, char mode) {
		volatile u32* opiGpioMode = (volatile u32*)(hwreg + OPI_GPIO_OFFSET + bank*36 + ((pin>>3)<<2));
		int offset = ((pin - ((pin >> 3) << 3)) << 2);
		
		(*opiGpioMode) &= ~(7 << offset);
		if(mode=='o') {
			(*opiGpioMode) |= (1 << offset);
		}
	};
	setGpioMode(OPI_GPIOTDO, 'i');
	setGpioMode(OPI_GPIOTCK, 'o');
	setGpioMode(OPI_GPIOTDI, 'o');
	setGpioMode(OPI_GPIOTMS, 'o');
	
	
	ioDelayCycles=calibrateDelay(halfperiod);
	return 0;
}
int platform_writeJtag(uint8_t* data, int len, volatile int* progress) {
	u32 mask1 = ((*opiGpio) & (~OPI_GPIOMASK));
	for(int i=0;i<len;i++) {
		uint8_t cmd=data[i];
		u32 tms=(cmd&(1))?1:0;
		u32 tdi=(cmd&(1<<1))?1:0;
		u32 tdo=(cmd&(1<<2))?1:0;
		u32 tdiEnable=(cmd&(1<<3))?1:0;
		u32 tdoEnable=(cmd&(1<<4))?1:0;
		
		//put data on the line and set tck low
		*opiGpio=mask1 | (tms<<OPI_GPIOTMS) | (tdi<<OPI_GPIOTDI) | (1<<OPI_GPIOTCK);
		doDelay(ioDelayCycles);
		*opiGpio=mask1 | (tms<<OPI_GPIOTMS) | (tdi<<OPI_GPIOTDI);
		
		doDelay(ioDelayCycles);
		//assert tck and sample tdo, while holding other pins constant
		*opiGpio = mask1 | (tms<<OPI_GPIOTMS) | (tdi<<OPI_GPIOTDI) | (1<<OPI_GPIOTCK);
		doDelay(ioDelayCycles);
		bool receivedTdo=((*opiGpio)&(1<<OPI_GPIOTDO))!=0;
		
		if(tdoEnable && receivedTdo!=tdo) {
			// error; tdo is not what was expected
			return i;
		}
		if(progress != NULL) *progress = i;
	}
	return 0;
}

