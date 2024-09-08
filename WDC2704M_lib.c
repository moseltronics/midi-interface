
char d_text[30];
//uint8_t cursor = 0xFF;      //0xFF = off
uint8_t cursor = 0x01;

#define display_ges_mask 0x1000FF81
#define display_data_mask 0xFF00
#define rs 28
#define e1 0

uint32_t swcheck;
uint8_t sw_flag = 0;            //1:re/li, 2:Taste gedrückt
uint32_t sw_old;

#define switch_in_mask 0x7C0000
#define switch_out_mask 0x0
#define sw_1 18             // li
#define sw_2 19             // re
#define sw_3 20             // up
#define sw_4 21             // dn
#define sw_5 22             // click
#define swm_1 0x40000       // = 2^18
#define swm_2 0x80000
#define swm_3 0x100000
#define swm_4 0x200000
#define swm_5 0x400000
#define delay_time  100000      //100 ms
#define rep_delay   150000      //150 ms
#define short_delay 100000      //100 ms

static void init_switches(void)
{
    gpio_init_mask (switch_in_mask);
    gpio_set_dir_in_masked (switch_in_mask);
    gpio_pull_up (sw_1);
    gpio_pull_up (sw_2);
    gpio_pull_up (sw_3);
    gpio_pull_up (sw_4);
    gpio_pull_up (sw_5);
    sw_flag = 0;
}

static void init_display_pins(void)
{
    gpio_init_mask (display_ges_mask);
    gpio_set_dir_out_masked (display_ges_mask);
    gpio_clr_mask (display_ges_mask);
    gpio_put (e1, true);
}

void write_byte(uint8_t idata)
{
    uint32_t pdata = (uint32_t)idata << 8;
    gpio_put_masked (display_data_mask, pdata);
    sleep_us(100);
    gpio_put (e1, false);
    sleep_us(50);
    gpio_put (e1, true);
    sleep_us(50);
}

void set_cursor(uint8_t l, uint8_t adr)
{
    gpio_put (rs, false);                   //instruction mode
    sleep_us(10);
    if (l==0) write_byte(0x80 + adr);       //OK
    else if (l==1) write_byte(0xC0 + adr);  //or 0x90 ?
    else if (l==2) write_byte(0x94 + adr);  //OK
    else if (l==3) write_byte(0xD4 + adr);  //or 0x98 ?

    sleep_us(100);
    write_byte(0x0E);                       //Cursor
}

void no_cursor()
{
    gpio_put (rs, false);                   //instruction mode
    sleep_us(100);
    write_byte (0x0C);                      //0x0C = Cursor off
    sleep_us(100);
}

void display_adr(uint8_t l, uint8_t adr)
{
    gpio_put (rs, false);                   //instruction mode
    sleep_us(10);
    if (l==0) write_byte(0x80 + adr);       //OK
    else if (l==1) write_byte(0xC0 + adr);  //or 0x90 ?
    else if (l==2) write_byte(0x94 + adr);  //OK
    else if (l==3) write_byte(0xD4 + adr);  //or 0x98 ?
    sleep_us(100);
}

void set_blink()
{
    gpio_put (rs, false);                   //instruction mode
    sleep_us(100);
    write_byte(0x0D);                       //blink
    sleep_us(100);
}

void write_line(uint8_t l){
    gpio_put (rs, false);               //instruction mode
    sleep_us(10);
    if (l==0) write_byte(0x80);         //OK
    else if (l==1) write_byte(0xC0);    //or 0x90 ?
    else if (l==2) write_byte(0x94);    //OK
    else if (l==3) write_byte(0xD4);    //or 0x98 ?

    gpio_put (rs, true);                //data mode
    sleep_us(10);
    uint8_t wbyte;
    for (uint8_t i=0; i<20; i++) {
        wbyte = d_text[i];
        if (wbyte==0) break;
        write_byte(wbyte);
    }

    //if (cursor != 0xFF) {
    //    gpio_put (rs, false);           //instruction mode
    //    sleep_us(10);
    //    write_byte(cursor+0xC0);     //address now 0x40
    //}
}

void write_pos(uint8_t l, uint8_t p, uint8_t n){
    gpio_put (rs, false);               //instruction mode
    sleep_us(10);
    if (l==0) {
        p += 0x80;
        write_byte(p);               //address now 0x00 + p
    }
    else if (l==1) {
        p += 0xC0;
        write_byte(p);               //address now 0x40 + p
    }
    else if (l==2) {
        p += 0x94;
        write_byte(p);               //address now 0x00 + p
    }
    else if (l==3) {
        p += 0xD4;
        write_byte(p);               //address now 0x40 + p
    }

    gpio_put (rs, true);                //data mode
    sleep_us(10);
    uint8_t wbyte = d_text[0];

    for (uint8_t i=0; i<n; i++) {
        wbyte = d_text[i];
        if (wbyte==0) break;
        write_byte(wbyte);
    }
}

void clear_space(uint8_t l, uint8_t p, uint8_t n){
    gpio_put (rs, false);               //instruction mode
    sleep_us(10);
    if (l==0) write_byte(0x80 + p);         //or 0x80 ?
    else if (l==1) write_byte(0xC0 + p);    //or 0x90 ?
    else if (l==2) write_byte(0x94 + p);    //or 0x88 ?
    else if (l==3) write_byte(0xD4 + p);    //or 0x98 ?

    gpio_put (rs, true);                //data mode
    sleep_us(10);

    for (uint8_t i=0; i<n; i++) {
        write_byte(0x20);
    }
}
