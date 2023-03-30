SETUPLEN=2
SETUPSEG=0x07e0

entry _start
_start:
! 首先读入光标位置
    mov     ah, #0x03
    xor     bh, bh
    int     0x10
! 显示字符串
    mov     cx, #39                 ! 要显示字符串的长度，注意还有3个换行 + 回车
    mov     bx, #0x0007          
    mov     bp, #msg1               ! es:bp是显示字符串的地址
! 增加对es的处理
    mov     ax, #0x07c0
    mov     es, ax                  ! es = 0x07c0
    mov     ax, #0x1301
    int     0x10

! 在bootsect.s中载入setup.s
load_setup: 
    mov     dx, #0x0000             ! 设置驱动器和磁头(diver 0, head 0): dl = 0是软盘，dh = 0是0号磁头
    mov     cx, #0x0002             ! 设置扇区号和磁道(sector 2, track 0): ch = 0是0磁道，cl = 02是2号扇区
    mov     bx, #0x0200             ! 设置读入的内存地址: BOOTSEG + address = 512，即偏移512Byte
    mov     ax, #0x0200 + SETUPLEN  ! 设置读入扇区个数为2个
    int     0x13                    ! 调用磁盘BIOS中断函数，读入2个setup.s扇区
    jnc     ok_load_setup           ! 读入成功 Jump if No Carry，就跳转到成功
    ! 0x13函数返回值带carry了，出错了
    mov     dx, #0x0000             ! 将软驱复位
    mov     ax, #0x0000
    int     0x13                    ! 调用磁盘BIOS进行复位
    jmp     load_setup              ! 重新循环再次尝试读取
ok_load_setup: 
    jmpi    0, SETUPSEG             ! 读取成功，跳转到cs:ip = 0x7ce0，因此设置setupseg = 0x07ce

! 设置一个无限循环
inf_loop:
    jmp inf_loop

! 要显示的字符串
msg1:
    .byte   13, 10           ! 换行 + 回车
    .ascii  "Hello OS world, my name is XLR..."
    .byte   13, 10, 13, 10

! 以下的语句从地址510 (0x1FC) 开始
.org 510
boot_flag:
    .word   0xAA55          ! 启动盘具有有效引导扇区的标志，必须位于引导扇区的最后2个字节
