INITSEG = 0x9000
entry _start
_start:
! 打印出now we are in my setup
! 首先读入光标位置
    mov     ah, #0x03
    xor     bh, bh
    int     0x10
! 显示setup的字符串
    mov     cx, #28             ! 要显示字符串的长度，注意还有3个换行 + 回车
    mov     bx, #0x0007          
    mov     bp, #msg2           ! es:bp是显示字符串的地址
! 修改es的值为cs的值
    mov     ax, cs
    mov     es, ax              ! es = cs
    mov     ax, #0x1301         ! ah = 13，显示字符串; al = 01，目标字符仅包含字符，属性在bl中，光标停在字符串结尾处
    int     0x10
    
    mov     ax, cs
    mov     es, ax
! 初始化ss:sp
    mov     ax, #INITSEG
    mov     ss, ax
    mov     sp, #0xFF00

! 获取硬件参数，放在内存0x90000
    mov     ax, #INITSEG
    mov     ds, ax              ! 数据段的基址0x9000
    ! 读入光标位置
    mov     ah, #0x03
    xor     bh, bh
    int     0x10
    ! 将光标位置写入0x90000
    mov     [0], dx             ! dh = 行号, dl = 列号

    ! 读入内存大小，超过1M则为扩展
    mov     ah, #0x88
    int     0x15
    mov     [2], ax             ! [2]是因为[0]和[1]被光标信息占用

    ! 读取第1个磁盘参数表，共16个Byte
    ! 其首地址在int 0x41的中断向量位置
    ! 中断向量表的起始地址是0x000，共1KB大，每个表项占4Byte
    ! 所以第1个磁盘参数表的首地址: 0x41 * 4 = 0x104，此处4Byte由段地址和偏移地址组成
    mov     ax, #0x0000         ! 中断向量表起始地址
    mov     ds, ax              ! 赋给段寄存器ds
    lds     si, [4*0x41]        ! Load pointer using DS，取出第一个表项的4Byte(32Bit)，高位存入ds，地位存入si
    mov     ax, #INITSEG
    mov     es, ax              ! es = 0x9000
    mov     di, #0x0004         ! 光标和内存共占用4Byte
    mov     cx, #0x10           ! counter = 16
    ! 重复16次
    rep                         ! 先执行cx--，并判断cx是否为0，不为0就执行后面一条语句
    movsb                       ! 按字节，把ds:si搬到es:di，并自动让si和di指向下一个字节（movsw就是按字搬运）

! 打印前的准备
    mov     ax, cs              
    mov     es, ax              ! es = cs = setup所在的代码段
    mov     ax, #INITSEG
    mov     ds, ax              ! ds = #INITSEG，数据段，指向参数所在的地方

! 读光标位置，显示提示语，并将数值入栈
    mov     ah, #0x03
    xor     bh, bh
    int     0x10

    mov     cx, #18             ! 16 + 2个字符
    mov     bx, #0x0007
    mov     bp, #msg_cursor     ! "cursor position:" [es:bp]，bp是base pointer基址指针寄存器
    mov     ax, #0x1301
    int     0x10

    mov     dx, [0]             ! 光标位置存到dx中
    call    print_hex           ! 打印光标位置

! 显示内存大小
    ! 读入光标位置
    mov     ah, #0x03
    xor     bh, bh
    int     0x10

    mov     cx, #14             ! 12 + 2
    mov     bx, #0x0007
    mov     bp, #msg_memory     ! "memory size:" [es:bp]
    mov     ax, #0x1301
    int     0x10
    
    mov     dx, [2]
    call    print_hex

! 在显示内存后补上KB
    mov     ah, #0x03
    xor     bh, bh
    int     0x10

    mov     cx, #2
    mov     bx, #0x0007
    mov     bp, #msg_kb
    mov     ax, #0x1301
    int     0x10

! 柱面cylinder
    mov     ah, #0x03
    xor     bh, bh
    int     0x10

    mov     cx, #7
    mov     bx, #0x0007
    mov     bp, #msg_cycles
    mov     ax, #0x1301
    int     0x10
    mov     dx, [4]
    call    print_hex

! 磁头headers
    mov     ah, #0x03
    xor     bh, bh
    int     0x10

    mov     cx, #8
    mov     bx, #0x0007
    mov     bp, #msg_headers
    mov     ax, #0x1301
    int     0x10
    mov     dx, [6]
    call    print_hex

! 扇区sectors
    mov     ah, #0x03
    xor     bh, bh
    int     0x10

    mov     cx, #10
    mov     bx, #0x0007
    mov     bp, #msg_sectors
    mov     ax, #0x1301
    int     0x10
    mov     dx, [18]            ! 在磁盘基本参数表中扇区偏移是0x0e，这里前4个字节是光标和内存，因此扇区在[18]
    call    print_hex

! 设置一个无限循环
inf_loop:
    jmp inf_loop

! 显示硬件参数的"函数"，使用的是dx中的数据
print_hex: 
    ! 以%04x进行显示
    mov     cx, #4

print_digit: 
    ! 从高位到低位显示4位16进制数
    rol     dx, #4          ! rol 把目的地址中的数据循环左移4次
    mov     ax, #0x0e0f     ! 此时al为00001111
    and     al, dl          ! 将dl和00001111按位与，即把dl的低4位存到了al的低4位中
    add     al, #0x30       ! al += 48，因为ascii码中数字从48开始
    cmp     al, #0x3a       ! 如果是数字，则0x30 <= al <= 0x39 < 0x3a
    jl      outp            ! Jump if Lower
    add     al, #0x07       ! 是字母，就再加上0x07，因为ascii码中字母A是0x41 (65)

outp: 
    int     0x10            ! 调用显卡BIOS输出
    loop    print_digit     ! loop执行时也会先cx--并判断cx是否为0，这里cx = 4就会执行4次
    ret

! 打印回车换行
print_nl:
    ! CR 回车
    mov     ax, #0x0e0d
    int     0x10
    ! LF 换行
    mov     ax, #0x0e0a
    int     0x10
    ret

! 要显示的字符串
msg2:
    .byte   13, 10          ! 换行 + 回车
    .ascii  "NOW we are in my SETUP"
    .byte   13, 10, 13, 10

msg_cursor: 
    .byte   13, 10
    .ascii  "cursor position:"

msg_memory: 
    .byte   13, 10
    .ascii  "memory size:"

msg_kb: 
    .ascii  "KB"

msg_cycles: 
    .byte   13, 10
    .ascii  "cyls:"

msg_headers: 
    .byte   13, 10
    .ascii  "heads:"

msg_sectors: 
    .byte   13, 10
    .ascii  "sectors:"

! 以下的语句从地址510 (0x1FC) 开始
.org 510
boot_flag:
    .word   0xAA55          ! 启动盘具有有效引导扇区的标志，必须位于引导扇区的最后2个字节
