#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <assert.h>

extern "C" {
	void sunxi_digitalWrite(int pin, int value);
	uint32_t readl(uint32_t addr);
	void writel(uint32_t val, uint32_t addr);
}

struct simple_ili9341 {
	int pin_cs, pin_sda, pin_scl, pin_dc;
	int fd;
	uint8_t* gpio;
	void init() {
		digitalWrite(pin_cs, 1);
		digitalWrite(pin_scl, 0);
		pinMode(pin_cs, OUTPUT);
		pinMode(pin_sda, OUTPUT);
		pinMode(pin_scl, OUTPUT);
		pinMode(pin_dc, OUTPUT);
		fd = open ("/dev/mem", O_RDWR | O_SYNC | O_CLOEXEC);
		#define GPIO_BASE_BP	(0x01C20000)
		gpio = (uint8_t *)mmap(0, 4096, PROT_READ|PROT_WRITE, MAP_SHARED, fd, GPIO_BASE_BP);
	}
	void delay() {
	}
	void writeByte(uint8_t byte, int dc) {
		digitalWrite(pin_cs, 0);
		delay();
		
		digitalWrite(pin_dc, dc);
		for(int i=0;i<8;i++) {
			digitalWrite(pin_sda, byte>>7);
			delay();
			digitalWrite(pin_scl, 1);
			delay();
			digitalWrite(pin_scl, 0);
			byte <<= 1;
		}
		delay();
		digitalWrite(pin_cs, 1);
	}
	#define SUNXI_GPIO_BASE (0x01C20800)
	void writePixels(uint16_t* pixels, int count, int dc) {
		digitalWrite(pin_cs, 0);
		delay();
		
		digitalWrite(pin_dc, dc);
		
		int sda=wpiPinToGpio(pin_sda);
		int scl=wpiPinToGpio(pin_scl);
		
		int bank_sda = sda>>5;
		int bank_scl = scl>>5;
		assert(bank_sda == bank_scl);
		int index_sda = sda-bank_sda;
		int index_scl = scl-bank_scl;
		uint32_t phyaddr_sda = SUNXI_GPIO_BASE + (bank_sda * 36) + 0x10;
		//uint32_t phyaddr_scl = SUNXI_GPIO_BASE + (bank_scl * 36) + 0x10;
		volatile uint32_t* ptr = getBankAddr(phyaddr_sda);
		//volatile uint32_t* ptr_scl = getBankAddr(phyaddr_scl);
		int mask_sda = 1 << index_sda;
		int mask_scl = 1 << index_scl;
		int reg1 = (*ptr) & (~(mask_sda|mask_scl));
		//int regval_scl = (*ptr_scl) & (~mask_scl);
		
		for(int x=0;x<count;x++) {
			uint16_t px = pixels[x];
			for(int i=0;i<16;i++) {
				//sunxi_digitalWrite(sda, byte>>7);
				int bit = px&1;
				int regval = reg1 | (bit << index_sda);
				*ptr = regval;
				delay();
				
				//sunxi_digitalWrite(scl, 1);
				regval |= mask_scl;
				*ptr = regval;
				
				delay();
				
				//sunxi_digitalWrite(scl, 0);
				regval = reg1 | (bit << index_sda);
				*ptr = regval;
				
				px >>= 1;
			}
		}
		delay();
		digitalWrite(pin_cs, 1);
	}
	
	volatile uint32_t* getBankAddr(uint32_t addr) {
		int MAP_MASK=4096-1;
		uint32_t val = 0;
		uint32_t mmap_seek = (addr & MAP_MASK);
		return (volatile uint32_t*)(gpio + mmap_seek);
	}

	void reset() {
		delay();
		writeByte(0x11,0);
		writeByte(0x29,0);
		writeByte(0x3a,0);
		writeByte(0b01010101,1);
		writeByte(0x13,0);
		writeByte(0x36,0);
		writeByte(0x00,1);
	}
	
	// 16bpp, rgb565
	void writeFrame(uint16_t* data, int pixels) {
		writeByte(0x2c,0);
		writePixels(data, pixels, 1);
	}
};
