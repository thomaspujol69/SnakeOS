#include "kernel.h"
#include "utils.h"
#include <unistd.h>

uint32 vga_index;
static uint32 next_line_index = 1;
uint8 g_fore_color = WHITE, g_back_color = BLUE;
int digit_ascii_codes[10] = {0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39};

void wait_for_io(uint32 timer_count)
{
  while(1){
    asm volatile("nop");
    timer_count--;
    if(timer_count <= 0)
      break;
    }
}
/*
void sleep(uint32 timer_count)
{
  wait_for_io(timer_count);
}

void sleep(){
  int i,j=0;
  while (i<1000000){
    while (j<1000000){
      j++;
    }
    i++;
  }
}
*/

uint16 vga_entry(unsigned char ch, uint8 fore_color, uint8 back_color) 
{
  uint16 ax = 0;
  uint8 ah = 0, al = 0;

  ah = back_color;
  ah <<= 4;
  ah |= fore_color;
  ax = ah;
  ax <<= 8;
  al = ch;
  ax |= al;

  return ax;
}

void clear_vga_buffer(uint16 **buffer, uint8 fore_color, uint8 back_color)
{
  uint32 i;
  for(i = 0; i < BUFSIZE; i++){
    (*buffer)[i] = vga_entry(0, fore_color, back_color);
  }
  next_line_index = 1;
  vga_index = 0;
}

void init_vga(uint8 fore_color, uint8 back_color)
{  
  vga_buffer = (uint16*)VGA_ADDRESS;
  clear_vga_buffer(&vga_buffer, fore_color, back_color);
  g_fore_color = fore_color;
  g_back_color = back_color;
}

void print_new_line()
{
  if(next_line_index >= 55){
    next_line_index = 0;
    clear_vga_buffer(&vga_buffer, g_fore_color, g_back_color);
  }
  vga_index = 80*next_line_index;
  next_line_index++;
}

void print_char(char ch)
{
  vga_buffer[vga_index] = vga_entry(ch, g_fore_color, g_back_color);
  vga_index++;
}

void print_string(char *str)
{
  uint32 index = 0;
  while(str[index]){
    print_char(str[index]);
    index++;
  }
}

void print_color_string(char *str, uint8 fore_color, uint8 back_color)
{
  uint32 index = 0;
  uint8 fc, bc;
  fc = g_fore_color;
  bc = g_back_color;
  g_fore_color = fore_color;
  g_back_color = back_color;
  while(str[index]){
    print_char(str[index]);
    index++;
  }
  g_fore_color = fc;
  g_back_color = bc;
}

void print_int(int num)
{
  char str_num[digit_count(num)+1];
  itoa(num, str_num);
  print_string(str_num);
}

uint16 get_box_draw_char(uint8 chn, uint8 fore_color, uint8 back_color)
{
  uint16 ax = 0;
  uint8 ah = 0;

  ah = back_color;
  ah <<= 4;
  ah |= fore_color;
  ax = ah;
  ax <<= 8;
  ax |= chn;

  return ax;
}

void gotoxy(uint16 x, uint16 y)
{
  vga_index = 80*y;
  vga_index += x;
}

void draw_generic_box(uint16 x, uint16 y, 
                      uint16 width, uint16 height,
                      uint8 fore_color, uint8 back_color,
                      uint8 topleft_ch,
                      uint8 topbottom_ch,
                      uint8 topright_ch,
                      uint8 leftrightside_ch,
                      uint8 bottomleft_ch,
                      uint8 bottomright_ch)
{
  uint32 i;

  //increase vga_index to x & y location
  vga_index = 80*y;
  vga_index += x;

  //draw top-left box character
  vga_buffer[vga_index] = get_box_draw_char(topleft_ch, fore_color, back_color);

  vga_index++;
  //draw box top characters, -
  for(i = 0; i < width; i++){
    vga_buffer[vga_index] = get_box_draw_char(topbottom_ch, fore_color, back_color);
    vga_index++;
  }

  //draw top-right box character
  vga_buffer[vga_index] = get_box_draw_char(topright_ch, fore_color, back_color);

  // increase y, for drawing next line
  y++;
  // goto next line
  vga_index = 80*y;
  vga_index += x;

  //draw left and right sides of box
  for(i = 0; i < height; i++){
    //draw left side character
    vga_buffer[vga_index] = get_box_draw_char(leftrightside_ch, fore_color, back_color);
    vga_index++;
    //increase vga_index to the width of box
    vga_index += width;
    //draw right side character
    vga_buffer[vga_index] = get_box_draw_char(leftrightside_ch, fore_color, back_color);
    //goto next line
    y++;
    vga_index = 80*y;
    vga_index += x;
  }
  //draw bottom-left box character
  vga_buffer[vga_index] = get_box_draw_char(bottomleft_ch, fore_color, back_color);
  vga_index++;
  //draw box bottom characters, -
  for(i = 0; i < width; i++){
    vga_buffer[vga_index] = get_box_draw_char(topbottom_ch, fore_color, back_color);
    vga_index++;
  }
  //draw bottom-right box character
  vga_buffer[vga_index] = get_box_draw_char(bottomright_ch, fore_color, back_color);

  vga_index = 0;
}

void draw_box(uint8 boxtype, 
              uint16 x, uint16 y, 
              uint16 width, uint16 height,
              uint8 fore_color, uint8 back_color)
{
  switch(boxtype){
    case BOX_SINGLELINE : 
      draw_generic_box(x, y, width, height, 
                      fore_color, back_color, 
                      218, 196, 191, 179, 192, 217);
      break;

    case BOX_DOUBLELINE : 
      draw_generic_box(x, y, width, height, 
                      fore_color, back_color, 
                      201, 205, 187, 186, 200, 188);
      break;
  }
}

void fill_box(uint8 ch, uint16 x, uint16 y, uint16 width, uint16 height, uint8 color)
{
  uint32 i,j;

  for(i = 0; i < height; i++){
    //increase vga_index to x & y location
    vga_index = 80*y;
    vga_index += x;

    for(j = 0; j < width; j++){
      vga_buffer[vga_index] = get_box_draw_char(ch, 0, color);
      vga_index++;
    }
    y++;
  }
}

void draw_grid(){
  for (int i=0; i<BOX_MAX_WIDTH; i=i+5){
    draw_box(BOX_SINGLELINE, 0, 0, BOX_MAX_WIDTH-i, BOX_MAX_HEIGHT, WHITE, BLACK);
  }
  for (int i=0; i<BOX_MAX_HEIGHT; i=i+3){
    draw_box(BOX_SINGLELINE, 0, 0, BOX_MAX_WIDTH, BOX_MAX_HEIGHT-i, WHITE, BLACK);
  }
}

void color_case(int i, int j, int w, int h, uint8 col){
  if (i==0){
    fill_box(0, i*BOX_MAX_WIDTH/w+1, j*(BOX_MAX_HEIGHT/h+1)+1, BOX_MAX_WIDTH/w-1, BOX_MAX_HEIGHT/h, col);
  }else{
    if (i>=9){ // Pourquoi ??! Je ne sais pas...
      fill_box(0, i*BOX_MAX_WIDTH/w+2, j*(BOX_MAX_HEIGHT/h+1)+1, BOX_MAX_WIDTH/w, BOX_MAX_HEIGHT/h, col);
    }else{
      fill_box(0, i*BOX_MAX_WIDTH/w+1, j*(BOX_MAX_HEIGHT/h+1)+1, BOX_MAX_WIDTH/w, BOX_MAX_HEIGHT/h, col);
    }
  }
}

void draw_state(int w, int h, uint8 (*pos)[h]){
  for (int i=0; i<w; i++){
    for (int j=0; j<h; j++){
      color_case(i, j, w, h, pos[i][j]);
    }
  }
}

void start_game(int w, int h){
  int direction=0;
  uint8 grid[w][h];
  int speed = 20000;
  int max_snake_length = w*h;
  int snake_x[128] = {0};
  int snake_y[128] = {0};
  int snake_length = 0;
  
  for (int i=0; i<w; i++){
    for (int j=0; j<h; j++){
      grid[i][j] = BLACK;
    }
  }

  snake_x[0]=3;
  snake_y[0]=3;
  grid[3][3]=BROWN;
  grid[9][6]=BRIGHT_MAGENTA;
  draw_state(w, h, grid);
  int old_posx, old_posy;
  uint8 finished = 0;
  while (!finished){
    
    for(int i=0; i<speed; i++)
      for(int j=0; j<speed; j++);
    
    //wait_for_io(32000);
    old_posx = snake_x[snake_length];
    old_posy = snake_y[snake_length];
    for(int i=snake_length; i>0; i--){
      snake_x[snake_length]=snake_x[snake_length-1];
      snake_y[snake_length]=snake_y[snake_length-1];
    } // On modifie le serpent
    //On met à jour la grille
    grid[old_posx][old_posy] = BLACK;
    /*  0 right
        1 left
        2 up
        3 down  */
    if (direction==0){ //right
      snake_x[0]=snake_x[0]+1;  //width
      snake_y[0]=snake_y[0];    // height
    }else if(direction==1){
      snake_x[0]=snake_x[0]-1;  //width
      snake_y[0]=snake_y[0];    // height
    }else if(direction==2){
      snake_x[0]=snake_x[0];  //width
      snake_y[0]=snake_y[0]-1;    // height
    }else{
      snake_x[0]=snake_x[0];  //width
      snake_y[0]=snake_y[0]+1;    // height
    }
    grid[snake_x[0]][snake_y[0]] = BROWN;
    if (snake_x[0]>=w || snake_y[0]>=h || snake_x[0]<0 || snake_y[0]<0){
      finished=1;
    }
    draw_state(w, h, grid);
    speed-=50;
  }
  
  char* str = "Game Over";
  gotoxy((VGA_MAX_WIDTH/2)-strlen(str), 1);
  print_color_string(str, WHITE, BLACK);
}

void kernel_entry()
{
  //const char*str = "Box Demo";

  init_vga(WHITE, BLACK);

  //gotoxy((VGA_MAX_WIDTH/2)-strlen(str), 1);
  //print_color_string("Box Demo", WHITE, BLACK);

  int w=0;
  int h=0;
  for (int i=0; i<BOX_MAX_WIDTH; i=i+5){
    draw_box(BOX_SINGLELINE, 0, 0, BOX_MAX_WIDTH-i, BOX_MAX_HEIGHT, WHITE, BLACK);
    w++;
  }
  for (int i=0; i<BOX_MAX_HEIGHT; i=i+3){
    draw_box(BOX_SINGLELINE, 0, 0, BOX_MAX_WIDTH, BOX_MAX_HEIGHT-i, WHITE, BLACK);
    h++;
  }
  
  start_game(w, h);

  //draw_grid()
  
  // 0 for only to fill colors, or provide any other character
  /*fill_box(0, 36, 5, 30, 10, RED);

  fill_box(1, 6, 16, 30, 4, GREEN);
  draw_box(BOX_DOUBLELINE, 6, 16, 28, 3, BLUE, GREEN);
  */
}

