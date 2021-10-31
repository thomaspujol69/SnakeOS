#include "kernel.h"
#include "utils.h"
#include <unistd.h>

uint32 vga_index;
static uint32 next_line_index = 1;
uint8 g_fore_color = WHITE, g_back_color = BLUE;
int digit_ascii_codes[10] = {0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39};
typedef struct { uint64_t state;  uint64_t inc; } pcg32_random_t;
pcg32_random_t rngGlobal = { 0xeeee1fecd2ff7a9bULL, 0xda3efffb94b95bdbULL };
int ahValue = 4; // diviser par deux pour le mode nightmare
int axValue = 8; // diviser par deux pour le mode nightmare

uint8 inb(uint16 port)
{
  uint8 ret;
  asm volatile("inb %1, %0" : "=a"(ret) : "d"(port));
  return ret;
}

char get_input_keycode()
{
  char ch = inb(KEYBOARD_PORT);
  if (ch<0){
    return 0;
  }
  return ch;
}

uint32_t pcg32_random_r(pcg32_random_t* rng)
{
    uint64_t oldstate = rng->state;
    // Advance internal state
    rng->state = oldstate * 6364136223846793005ULL + (rng->inc|1);
    // Calculate output function (XSH RR), uses old state for max ILP
    uint32_t xorshifted = ((oldstate >> 18u) ^ oldstate) >> 27u;
    uint32_t rot = oldstate >> 59u;
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}

int random(int max) { 
  uint32 r = pcg32_random_r(&rngGlobal);
  if (max*r/547483647>((uint32)max)){
    return max;
  }
  return max*r/547483647;
}

void wait_for_io(){
  while(1){
    if (get_input_keycode()!=0){
      return;
    }
  }
}

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
  ah <<= ahValue;
  ah |= fore_color;
  ax = ah;
  ax <<= axValue;
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

int get_direction(int speed, int d){
  int direction = d;
  for(int i=0; i<speed; i++){
    for(int j=0; j<speed; j++){
      char keycode = get_input_keycode();
      /*  0 right
        1 left
        2 up
        3 down  */
      if (keycode==KEY_RIGHT){
        direction = 0;
      }else if(keycode==KEY_LEFT){
        direction = 1;
      }else if(keycode==KEY_UP){
        direction = 2;
      }else if(keycode==KEY_DOWN){
        direction = 3;
      }
    }
  } 
  return direction;
}

int is_in(int ap, int ap2, int ar1[], int ar2[], int sLen){
  for (int i=0; i<sLen; i++){
    if(ar1[i]==ap && ar2[i]==ap2){
      return 1;
    }
  }
  return 0;
}

int start_game(int w, int h){
  int direction=0;
  uint8 grid[w][h];
  int speed = 3000;
  speed = 2500;
  int snake_x[128] = {0};
  int snake_y[128] = {0};
  int snake_length = 1; 
  
  for (int i=0; i<w; i++){
    for (int j=0; j<h; j++){
      grid[i][j] = BLACK;
    }
  }

  snake_x[0]=3;
  snake_y[0]=3;
  grid[3][3]=BROWN;
  int applex, appley;
  do{
    applex=random(h);
    appley=random(w);
  }while (is_in(applex, appley, snake_x, snake_y, snake_length));
  grid[applex][appley]=BRIGHT_MAGENTA;
  draw_state(w, h, grid);
  int old_posx, old_posy;
  uint8 finished = 0;
  int addX, addY, cWhile;

  while (!finished){
    direction = get_direction(speed, direction);

    old_posx = snake_x[snake_length-1];
    old_posy = snake_y[snake_length-1];
    for(int i=snake_length-1; i>0; i--){ // On décale les valeurs
      snake_x[i]=snake_x[i-1];
      snake_y[i]=snake_y[i-1];
    } // On modifie le serpent
    addX=0; addY=0;
    if (direction==0){
      addX++;    // width
    }else if(direction==1){
      addX--;    // width
    }else if(direction==2){
      addY--;    // height
    }else{
      addY++;    // height
    }
    snake_x[0]=snake_x[0]+addX;    // width
    snake_y[0]=snake_y[0]+addY;    // height
    if (grid[snake_x[0]][snake_y[0]] != BLACK && grid[snake_x[0]][snake_y[0]] != BROWN){ // miam
      cWhile=0;
      do{
        applex=random(h);
        appley=random(w);
        cWhile++;
        if (cWhile>32000){
          for (int i=0; i<w; i++){
            for (int j=0;j<h; j++){
              applex=i;
              appley=j;
              if (is_in(applex, appley, snake_x, snake_y, snake_length)){
                goto EndDoubleFor;
              }
            }
          }
          EndDoubleFor:;
          //return snake_length;
        }
      }while (is_in(applex, appley, snake_x, snake_y, snake_length));
      grid[applex][appley]=BRIGHT_MAGENTA;
      snake_length++;
      grid[old_posx][old_posy] = BLACK;
    }else{
      grid[old_posx][old_posy] = BLACK; // Lorsqu'on mange on laisse le dernier carré
    }
    if (snake_x[0]>=w || snake_y[0]>=h || snake_x[0]<0 || snake_y[0]<0 || grid[snake_x[0]][snake_y[0]]==BROWN ||snake_length>=(w*h)){
      return snake_length;
    }
    print_int(snake_length);
    grid[snake_x[0]][snake_y[0]] = BROWN;
    draw_state(w, h, grid);
    //speed=speed*0.993;
    speed=speed*0.998;
  } 
  return 0;
}

void kernel_entry()
{ 
  int w=0;
  int h=0;

  char* str = "Appuyez sur une touche pour commencer la partie.";
  int result;
  init_vga(WHITE, BLACK); 
  while (1){
    print_string(str);
    for (int i=0; i<32000; i++){
      for (int j=0; j<22000; j++){
      
      }
    }
    wait_for_io();

    init_vga(WHITE, BLACK); 
    w=0;
    h=0;
    for (int i=0; i<BOX_MAX_WIDTH; i=i+5){
      draw_box(BOX_SINGLELINE, 0, 0, BOX_MAX_WIDTH-i, BOX_MAX_HEIGHT, WHITE, BLACK);
      w++;
    }
    for (int i=0; i<BOX_MAX_HEIGHT; i=i+3){
      draw_box(BOX_SINGLELINE, 0, 0, BOX_MAX_WIDTH, BOX_MAX_HEIGHT-i, WHITE, BLACK);
      h++;
    }
    
    result = start_game(w, h);

    init_vga(WHITE, BLACK); 

    print_new_line();
    print_string("Game Over !");
    print_new_line();
    print_string("Score : ");
    print_int(result);
    print_new_line();
  }
}

