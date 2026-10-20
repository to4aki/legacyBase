#include <stdint.h>


#define VGA_WIDTH   320
#define VGA_HEIGHT  200
#define VGA_MEMORY  0xA0000


#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21

#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1


#define PIT_CHANNEL0 0x40
#define PIT_COMMAND  0x43
#define PIT_BASE_HZ  1193182


#define PS2_DATA 0x60


#define KEY_BUFFER_SIZE 64


#define BOX_WIDTH   32
#define BOX_HEIGHT  24


struct idt_entry
{
    uint16_t offset_low;
    uint16_t selector;
    uint8_t zero;
    uint8_t type_attr;
    uint16_t offset_high;
} __attribute__((packed));


struct idt_ptr
{
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));


extern void irq0_entry(void);
extern void irq1_entry(void);


static struct idt_entry idt[256];
static struct idt_ptr idtr;


static volatile uint32_t timer_ticks = 0;


static volatile uint8_t key_buffer[KEY_BUFFER_SIZE];

static volatile uint32_t key_write_pos = 0;
static volatile uint32_t key_read_pos = 0;


static volatile uint8_t *vga =
    (volatile uint8_t *)VGA_MEMORY;


static int box_x = 40;
static int box_y = 30;

static int old_box_x = 40;
static int old_box_y = 30;

static int box_dx = 2;
static int box_dy = 1;


/*
 * ------------------------------------------------------------
 * I/O
 * ------------------------------------------------------------
 */

static inline void outb(
    uint16_t port,
    uint8_t value)
{
    __asm__ volatile (
        "outb %0, %1"
        :
        : "a"(value),
          "Nd"(port)
    );
}


static inline uint8_t inb(
    uint16_t port)
{
    uint8_t value;

    __asm__ volatile (
        "inb %1, %0"
        : "=a"(value)
        : "Nd"(port)
    );

    return value;
}


static void io_wait(void)
{
    outb(0x80, 0);
}


/*
 * ------------------------------------------------------------
 * IDT
 * ------------------------------------------------------------
 */

static void idt_set_gate(
    uint8_t vector,
    uint32_t handler)
{
    idt[vector].offset_low =
        (uint16_t)(handler & 0xFFFF);

    idt[vector].selector =
        0x08;

    idt[vector].zero =
        0;

    idt[vector].type_attr =
        0x8E;

    idt[vector].offset_high =
        (uint16_t)((handler >> 16) & 0xFFFF);
}


static void idt_init(void)
{
    uint32_t i;


    for (i = 0; i < 256; i++)
    {
        idt[i].offset_low = 0;
        idt[i].selector = 0;
        idt[i].zero = 0;
        idt[i].type_attr = 0;
        idt[i].offset_high = 0;
    }


    idt_set_gate(
        0x20,
        (uint32_t)irq0_entry
    );


    idt_set_gate(
        0x21,
        (uint32_t)irq1_entry
    );


    idtr.limit =
        (uint16_t)(sizeof(idt) - 1);

    idtr.base =
        (uint32_t)&idt;


    __asm__ volatile (
        "lidt %0"
        :
        : "m"(idtr)
    );
}


/*
 * ------------------------------------------------------------
 * PIC
 * ------------------------------------------------------------
 */

static void pic_init(void)
{
    outb(PIC1_COMMAND, 0x11);
    io_wait();

    outb(PIC2_COMMAND, 0x11);
    io_wait();


    outb(PIC1_DATA, 0x20);
    io_wait();

    outb(PIC2_DATA, 0x28);
    io_wait();


    outb(PIC1_DATA, 0x04);
    io_wait();

    outb(PIC2_DATA, 0x02);
    io_wait();


    outb(PIC1_DATA, 0x01);
    io_wait();

    outb(PIC2_DATA, 0x01);
    io_wait();


    /*
     * IRQ0 = timer
     * IRQ1 = keyboard
     *
     * 両方enable
     */

    outb(PIC1_DATA, 0xFC);

    outb(PIC2_DATA, 0xFF);
}


/*
 * ------------------------------------------------------------
 * PIT
 * ------------------------------------------------------------
 */

static void pit_init(
    uint32_t hz)
{
    uint32_t divisor;


    divisor =
        PIT_BASE_HZ / hz;


    outb(
        PIT_COMMAND,
        0x36
    );


    outb(
        PIT_CHANNEL0,
        (uint8_t)(divisor & 0xFF)
    );


    outb(
        PIT_CHANNEL0,
        (uint8_t)((divisor >> 8) & 0xFF)
    );
}


void timer_irq_handler(void)
{
    timer_ticks++;


    outb(
        PIC1_COMMAND,
        0x20
    );
}


/*
 * ------------------------------------------------------------
 * Keyboard IRQ + Ring Buffer
 * ------------------------------------------------------------
 */

void keyboard_irq_handler(void)
{
    uint8_t code;
    uint32_t next;


    code =
        inb(PS2_DATA);


    next =
        (key_write_pos + 1)
        % KEY_BUFFER_SIZE;


    if (next != key_read_pos)
    {
        key_buffer[key_write_pos] =
            code;

        key_write_pos =
            next;
    }


    outb(
        PIC1_COMMAND,
        0x20
    );
}


static int keyboard_available(void)
{
    return
        key_read_pos !=
        key_write_pos;
}


static uint8_t keyboard_get(void)
{
    uint8_t code;


    code =
        key_buffer[key_read_pos];


    key_read_pos =
        (key_read_pos + 1)
        % KEY_BUFFER_SIZE;


    return code;
}


/*
 * ------------------------------------------------------------
 * VGA
 * ------------------------------------------------------------
 */

static void vga_clear(
    uint8_t color)
{
    uint32_t i;


    for (i = 0;
         i < VGA_WIDTH * VGA_HEIGHT;
         i++)
    {
        vga[i] = color;
    }
}


static void vga_rect(
    int x,
    int y,
    int width,
    int height,
    uint8_t color)
{
    int px;
    int py;


    for (py = 0;
         py < height;
         py++)
    {
        volatile uint8_t *line;


        line =
            vga +
            (y + py) * VGA_WIDTH +
            x;


        for (px = 0;
             px < width;
             px++)
        {
            line[px] = color;
        }
    }
}


/*
 * ------------------------------------------------------------
 * Keyboard
 * ------------------------------------------------------------
 */

static void keyboard_process(
    uint8_t code)
{
    /*
     * break codeは無視
     */

    if ((code & 0x80) != 0)
    {
        return;
    }


    /*
     * A
     *
     * 左方向へ加速
     */

    if (code == 0x1E)
    {
        box_dx--;

        if (box_dx < -6)
        {
            box_dx = -6;
        }
    }


    /*
     * D
     *
     * 右方向へ加速
     */

    if (code == 0x20)
    {
        box_dx++;

        if (box_dx > 6)
        {
            box_dx = 6;
        }
    }


    /*
     * W
     *
     * 上方向へ加速
     */

    if (code == 0x11)
    {
        box_dy--;

        if (box_dy < -6)
        {
            box_dy = -6;
        }
    }


    /*
     * S
     *
     * 下方向へ加速
     */

    if (code == 0x1F)
    {
        box_dy++;

        if (box_dy > 6)
        {
            box_dy = 6;
        }
    }
}


/*
 * ------------------------------------------------------------
 * Application update
 * ------------------------------------------------------------
 */

static void app_update(void)
{
    /*
     * Keyboard ring bufferを全部処理
     */

    while (keyboard_available())
    {
        keyboard_process(
            keyboard_get()
        );
    }


    old_box_x = box_x;
    old_box_y = box_y;


    box_x += box_dx;
    box_y += box_dy;


    /*
     * 左
     */

    if (box_x < 0)
    {
        box_x = 0;

        box_dx =
            -box_dx;
    }


    /*
     * 右
     */

    if (box_x >
        VGA_WIDTH - BOX_WIDTH)
    {
        box_x =
            VGA_WIDTH - BOX_WIDTH;

        box_dx =
            -box_dx;
    }


    /*
     * 上
     */

    if (box_y < 0)
    {
        box_y = 0;

        box_dy =
            -box_dy;
    }


    /*
     * 下
     */

    if (box_y >
        VGA_HEIGHT - BOX_HEIGHT)
    {
        box_y =
            VGA_HEIGHT - BOX_HEIGHT;

        box_dy =
            -box_dy;
    }
}


/*
 * ------------------------------------------------------------
 * Application render
 * ------------------------------------------------------------
 */

static void app_render(void)
{
    /*
     * 前のBOXだけ消す
     *
     * 画面全体は消さない
     */

    vga_rect(
        old_box_x,
        old_box_y,
        BOX_WIDTH,
        BOX_HEIGHT,
        1
    );


    /*
     * 新しいBOX
     */

    vga_rect(
        box_x,
        box_y,
        BOX_WIDTH,
        BOX_HEIGHT,
        15
    );
}


/*
 * ------------------------------------------------------------
 * Application entry
 * ------------------------------------------------------------
 */

void app_main(void)
{
    uint32_t processed_tick;


    idt_init();

    pic_init();


    /*
     * PIT 100Hz
     */

    pit_init(100);


    processed_tick = 0;


    /*
     * 一度だけ全画面初期化
     */

    vga_clear(1);


    /*
     * 上ライン
     */

    vga_rect(
        0,
        0,
        VGA_WIDTH,
        4,
        3
    );


    /*
     * 下ライン
     */

    vga_rect(
        0,
        VGA_HEIGHT - 4,
        VGA_WIDTH,
        4,
        3
    );


    /*
     * 最初のBOX
     */

    vga_rect(
        box_x,
        box_y,
        BOX_WIDTH,
        BOX_HEIGHT,
        15
    );


    __asm__ volatile ("sti");


    for (;;)
    {
        uint32_t now;


        /*
         * Keyboard IRQで起床した場合は
         * タイマーを待たず入力を処理
         */

        while (keyboard_available())
        {
            keyboard_process(
                keyboard_get()
            );
        }


        now =
            timer_ticks;


        /*
         * Animation = 50Hz
         *
         * PIT       = 100Hz
         */

        while (processed_tick != now)
        {
            processed_tick++;


            if ((processed_tick & 1) == 0)
            {
                app_update();

                app_render();
            }
        }


        __asm__ volatile ("hlt");
    }
}

