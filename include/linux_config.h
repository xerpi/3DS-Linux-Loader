// Brahma
#define PA_ARM11_CODE_ADDR (0x1FFFFFF0)

// 3DS memory layout
#define VRAM_BASE     (0x18000000)
#define VRAM_SIZE     (0x00600000)
#define AXI_WRAM_BASE (0x1FF80000)
#define AXI_WRAM_SIZE (0x00080000)
#define FCRAM_BASE    (0x20000000)
#define FCRAM_SIZE    (0x08000000)

// Payload settings
#define ARM11_EXCVEC_ADDR     (0x1FFF4000)
#define ARM11_STAGE0_ADDR     (0x1FFF4B40)
#define ARM11_EXCVEC_ADDR_VA  (0xFFFF0000)
#define ARM11_STAGE0_ADDR_VA  (0xFFFF0B40)
#define ARM11_STAGE1_ADDR     (0x1FF80000)
#define ARM11_STAGE1_ADDR     (0x1FF80000)

// Linux settings
#define ZIMAGE_ADDR    (0x20408000)
#define ZRELADDR       (0x20008000)
#define PARAMS_ADDR    (0x20000100)
#define INITRD_ADDR    (0x20000000)
#define MACHINE_NUMBER (0xFFFFFFFF)

#define LINUXIMAGE_FILENAME "/zImage"
#define DTB_FILENAME        "/nintendo3ds_ctr.dtb"

// Linux shared stuff
#define SHARED_CHAR (0x27FFFFFF)

