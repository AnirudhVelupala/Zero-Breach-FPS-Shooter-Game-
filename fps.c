/*
 * ╔══════════════════════════════════════════════════════════════════╗
 * ║           ZERO BREACH  v5  ─  ULTIMATE EDITION                   ║
 * ║  Pure C · SDL2 · 5 Named Maps · 5 Enemy Types · BGM · Settings   ║
 * ╚══════════════════════════════════════════════════════════════════╝
 *
 *  BUILD:
 *    gcc -O2 -o fps fps.c $(sdl2-config --cflags --libs) -lm
 *    ./fps
 *
 *  CONTROLS:
 *    W/S/A/D      Move
 *    MOUSE        Aim
 *    LEFT CLICK   Shoot
 *    SHIFT        Sprint
 *    CTRL         Crouch (reduces enemy detection)
 *    R            Reload
 *    E            Use / interact
 *    TAB          Shop
 *    M            Minimap
 *    ESC          Menu / pause
 *    F            Fullscreen
 */

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdint.h>
#include <stdarg.h>

/* ═══════════════════ CONFIG ═══════════════════ */
#define WIN_W         1280
#define WIN_H         720
#define VIEW_W        WIN_W
#define VIEW_H        560
#define HUD_Y         VIEW_H
#define HUD_H         (WIN_H - VIEW_H)
#define HALF_H        (VIEW_H / 2)
#define FOV_RAD       1.1f
#define MAX_DEPTH     24.0f
#define MOVE_SPEED    4.2f
#define SPRINT_MULT   1.65f
#define MOUSE_SENS    0.0018f
#define TEX_W         64
#define TEX_H         64
#define NUM_TEX       18
#define MAP_W         40
#define MAP_H         40
#define NUM_MAPS      6
#define MAX_ENEMIES   24
#define MAX_ITEMS     20
#define MAX_PARTICLES 1200
#define NUM_WEAPONS   5
#define ENEMY_ATTACK_RANGE  7.0f   /* max range enemies can shoot */
#define ENEMY_MELEE_RANGE   1.4f
#define ENEMY_ALERT_RANGE   9.0f
#define ENEMY_DAMAGE_MIN    4.0f   /* reduced base damage */
#define ENEMY_DAMAGE_SCALE  0.7f   /* damage multiplier */

typedef uint32_t u32;
typedef uint8_t  u8;

/* ═══════════════════ COLORS ═══════════════════ */
static inline u32 rgb(int r,int g,int b){
    r=r<0?0:r>255?255:r; g=g<0?0:g>255?255:g; b=b<0?0:b>255?255:b;
    return (u32)(0xFF000000u|(r<<16)|(g<<8)|b);
}
static inline u32 darken(u32 c,float f){
    if(f<=0)return 0xFF000000u; if(f>=1)return c;
    return rgb((int)(((c>>16)&0xff)*f),(int)(((c>>8)&0xff)*f),(int)((c&0xff)*f));
}
static inline u32 blend_col(u32 a,u32 b,float t){
    float s=1.f-t;
    return rgb((int)((((a>>16)&0xff)*s)+(((b>>16)&0xff)*t)),
               (int)((((a>>8) &0xff)*s)+(((b>>8) &0xff)*t)),
               (int)(((a&0xff)*s)+((b&0xff)*t)));
}
static inline float clampf(float v,float lo,float hi){return v<lo?lo:v>hi?hi:v;}
static inline int   clampi(int v,int lo,int hi){return v<lo?lo:v>hi?hi:v;}

/* ═══════════════════ FRAMEBUFFER ═══════════════════ */
static u32   pixels[WIN_H][WIN_W];
static float zbuf[VIEW_W];
static inline void pset(int x,int y,u32 c){
    if((unsigned)x<WIN_W&&(unsigned)y<WIN_H) pixels[y][x]=c;
}
static void fill_rect(int x,int y,int w,int h,u32 c){
    for(int ry=y;ry<y+h;ry++) for(int rx=x;rx<x+w;rx++) pset(rx,ry,c);
}
static void outline_rect(int x,int y,int w,int h,u32 c){
    for(int i=x;i<x+w;i++){pset(i,y,c);pset(i,y+h-1,c);}
    for(int i=y;i<y+h;i++){pset(x,i,c);pset(x+w-1,i,c);}
}
static void draw_line(int x0,int y0,int x1,int y1,u32 c){
    int dx=abs(x1-x0),dy=abs(y1-y0),sx=x0<x1?1:-1,sy=y0<y1?1:-1,err=dx-dy;
    while(1){pset(x0,y0,c);if(x0==x1&&y0==y1)break;int e2=2*err;
        if(e2>-dy){err-=dy;x0+=sx;}if(e2<dx){err+=dx;y0+=sy;}}
}
static void draw_circle(int cx,int cy,int r,u32 c){
    for(int a=0;a<360;a++){
        int x=(int)(cx+cosf(a*0.01745f)*r),y=(int)(cy+sinf(a*0.01745f)*r);
        pset(x,y,c);
    }
}
static void fill_circle(int cx,int cy,int r,u32 c){
    for(int y=-r;y<=r;y++) for(int x=-r;x<=r;x++)
        if(x*x+y*y<=r*r) pset(cx+x,cy+y,c);
}

/* ═══════════════════ RNG ═══════════════════ */
static uint32_t rng_s=0xDEADBEEFu;
static inline uint32_t rnx(void){rng_s=rng_s*1664525u+1013904223u;return rng_s;}
static inline float rnf(void){return (float)(rnx()&0xFFFF)/65535.f;}
static inline int   rni(int a,int b){return a+(int)(rnx()%(unsigned)(b-a+1));}
static float noise2(float x,float y){
    int ix=(int)x,iy=(int)y; float fx=x-ix,fy=y-iy;
    fx=fx*fx*(3.f-2.f*fx); fy=fy*fy*(3.f-2.f*fy);
    uint32_t s=rng_s;
    rng_s=(uint32_t)(ix*73856093^iy*19349663);    float v00=(float)(rnx()&0xff)/255.f;
    rng_s=(uint32_t)((ix+1)*73856093^iy*19349663); float v10=(float)(rnx()&0xff)/255.f;
    rng_s=(uint32_t)(ix*73856093^(iy+1)*19349663); float v01=(float)(rnx()&0xff)/255.f;
    rng_s=(uint32_t)((ix+1)*73856093^(iy+1)*19349663); float v11=(float)(rnx()&0xff)/255.f;
    rng_s=s;
    return v00*(1-fx)*(1-fy)+v10*fx*(1-fy)+v01*(1-fx)*fy+v11*fx*fy;
}

/* ═══════════════════ TEXTURES ═══════════════════ */
static u32 tex[NUM_TEX][TEX_H][TEX_W];
static void make_brick(int id,u32 base,u32 mortar){
    for(int y=0;y<TEX_H;y++){
        int row=y/10,off=(row&1)?16:0,mh=(y%10<=1);
        for(int x=0;x<TEX_W;x++){
            int mv=((x+off)%32<=1);
            if(mh||mv){tex[id][y][x]=mortar;continue;}
            float n=noise2(x*0.25f+row*5.f,y*0.25f)*40.f-20.f;
            float hi=(y%10==2)?10.f:0.f;
            tex[id][y][x]=rgb((int)(((base>>16)&0xff)+n+hi),
                              (int)(((base>>8)&0xff)+n*0.6f),
                              (int)((base&0xff)+n*0.3f));
        }
    }
}
static void make_stone(int id,u32 base){
    for(int y=0;y<TEX_H;y++) for(int x=0;x<TEX_W;x++){
        float n=(noise2(x*0.18f,y*0.18f)*0.6f+noise2(x*0.4f,y*0.4f)*0.3f+noise2(x*0.8f,y*0.8f)*0.1f)*60.f-30.f;
        int v=(int)(((base>>16)&0xff)+n);
        tex[id][y][x]=rgb(v,v,(int)(v*1.04f));
    }
    for(int c=0;c<8;c++){
        rng_s=(uint32_t)(id*9999u+c*1111u);
        int cx2=rni(2,TEX_W-3),cy2=rni(2,TEX_H-3),len=rni(5,16);
        float cur=(float)rni(0,628)/100.f; int px2=cx2,py2=cy2;
        for(int i=0;i<len;i++){
            cur+=rnf()*0.5f-0.25f; px2+=(int)(cosf(cur)*1.5f); py2+=(int)(sinf(cur)*1.5f);
            if(px2<0||px2>=TEX_W||py2<0||py2>=TEX_H)break;
            tex[id][py2][px2]=darken(tex[id][py2][px2],0.35f);
        }
    }
}
static void make_metal(int id,u32 base){
    for(int y=0;y<TEX_H;y++) for(int x=0;x<TEX_W;x++){
        float s=sinf(y*0.55f)*20.f+sinf(x*0.08f)*6.f;
        rng_s^=(uint32_t)(x*y+(uint32_t)id);
        float n=rnf()*10.f-5.f;
        int r=(int)(((base>>16)&0xff)+s+n);
        int g=(int)(((base>>8)&0xff)+s+n);
        int b=(int)((base&0xff)+s*1.1f+n);
        if(x%24<2||y%24<2){r=g=b=clampi((r+g+b)/3-30,0,255);}
        if((x%24==12)&&(y%24==12)){r=210;g=210;b=215;}
        tex[id][y][x]=rgb(r,g,b);
    }
}
static void make_mossy(int id){
    make_stone(id,rgb(72,82,62));
    for(int p=0;p<12;p++){
        rng_s=(uint32_t)(p*77771u+id*3333u);
        int cx2=rni(0,TEX_W-1),cy2=rni(0,TEX_H-1),rad=rni(3,10);
        for(int dy=-rad;dy<=rad;dy++) for(int dx=-rad;dx<=rad;dx++){
            int px2=cx2+dx,py2=cy2+dy;
            if(px2<0||px2>=TEX_W||py2<0||py2>=TEX_H)continue;
            float d=sqrtf((float)(dx*dx+dy*dy)); if(d>rad)continue;
            rng_s^=(uint32_t)(px2*py2+p);
            u32 moss=rgb((int)(30+rnf()*15),(int)(85+rnf()*30),(int)(20+rnf()*15));
            tex[id][py2][px2]=blend_col(tex[id][py2][px2],moss,(1.f-d/rad)*0.7f);
        }
    }
}
static void make_rune(int id,u32 base){
    make_stone(id,base);
    u32 glow=rgb(30,200,140);
    for(int y=6;y<TEX_H;y+=14) for(int x=4;x<TEX_W-4;x+=10){
        rng_s=(uint32_t)(x*y*3u+(uint32_t)id+1u);
        for(int i=0;i<8;i++){
            int rx=x+rni(0,8),ry=y+rni(0,10);
            if(rx>=0&&rx<TEX_W&&ry>=0&&ry<TEX_H) tex[id][ry][rx]=glow;
        }
    }
}
static void make_wood(int id,u32 base){
    for(int y=0;y<TEX_H;y++){
        int plank=y/8;
        for(int x=0;x<TEX_W;x++){
            if(y%8==0){tex[id][y][x]=darken(base,0.5f);continue;}
            float n=noise2(x*0.15f+(float)plank*3.7f,y*0.3f)*30.f;
            float grain=sinf((x+plank*13)*0.4f)*5.f;
            tex[id][y][x]=rgb((int)(((base>>16)&0xff)+n+grain),
                              (int)(((base>>8)&0xff)+n*0.7f+grain*0.5f),
                              (int)((base&0xff)+n*0.3f));
        }
    }
}
static void make_concrete(int id,u32 base){
    for(int y=0;y<TEX_H;y++) for(int x=0;x<TEX_W;x++){
        float n=noise2(x*0.3f,y*0.3f)*25.f-12.f;
        rng_s^=(uint32_t)(x*3331u+y*5003u+(uint32_t)id);
        int agg=(rnx()&0xFF)<10?rni(-20,20):0;
        tex[id][y][x]=rgb((int)(((base>>16)&0xff)+n+agg),
                          (int)(((base>>8)&0xff)+n+agg),
                          (int)((base&0xff)+n+agg));
    }
}
static void make_lava(int id){
    /* animated-look lava using offset noise */
    for(int y=0;y<TEX_H;y++) for(int x=0;x<TEX_W;x++){
        float n=noise2(x*0.2f,y*0.2f)*0.5f+noise2(x*0.4f+3.f,y*0.4f)*0.3f+noise2(x*0.8f,y*0.9f)*0.2f;
        int rv=(int)(200+n*55.f),gv=(int)(50+n*80.f);
        tex[id][y][x]=rgb(rv,gv,0);
    }
}
static void make_ice(int id){
    for(int y=0;y<TEX_H;y++) for(int x=0;x<TEX_W;x++){
        float n=noise2(x*0.2f,y*0.2f)*20.f;
        int v=(int)(180+n);
        tex[id][y][x]=rgb((int)(v*0.85f),(int)(v*0.92f),v);
    }
}
static void make_floor_tile(int id,u32 a,u32 b){
    for(int y=0;y<TEX_H;y++) for(int x=0;x<TEX_W;x++){
        int tile=(x/16+y/16)&1;
        float n=noise2(x*0.2f,y*0.2f)*12.f-6.f;
        int grout=(x%16<2||y%16<2);
        if(grout)tex[id][y][x]=darken(tile?a:b,0.45f);
        else{u32 base=tile?a:b;
            tex[id][y][x]=rgb(clampi((int)(((base>>16)&0xff)+n),0,255),
                              clampi((int)(((base>>8)&0xff)+n),0,255),
                              clampi((int)((base&0xff)+n),0,255));}
    }
}
static void make_ceil_panel(int id,u32 base){
    for(int y=0;y<TEX_H;y++) for(int x=0;x<TEX_W;x++){
        float n=noise2(x*0.1f,y*0.1f)*8.f;
        int panel=(x%32<2||y%32<2);
        u32 c=panel?darken(base,0.55f):rgb(clampi((int)(((base>>16)&0xff)+n),0,255),
                                           clampi((int)(((base>>8)&0xff)+n),0,255),
                                           clampi((int)((base&0xff)+n),0,255));
        if(x%32==16&&y%32==16)c=rgb(220,210,160);
        tex[id][y][x]=c;
    }
}
static void gen_textures(void){
    rng_s=0xCAFEF00Du;
    make_brick    (0,rgb(155,72,52),  rgb(95,92,88));   /* red brick */
    make_stone    (1,rgb(100,105,112));                  /* grey stone */
    make_metal    (2,rgb(70,80,100));                    /* blue metal */
    make_mossy    (3);                                   /* mossy stone */
    make_brick    (4,rgb(50,80,140),  rgb(65,65,105));   /* blue brick */
    make_rune     (5,rgb(35,45,50));                     /* rune stone */
    make_wood     (6,rgb(90,55,25));                     /* brown wood */
    make_concrete (7,rgb(75,75,78));                     /* concrete */
    make_brick    (8,rgb(130,110,60), rgb(80,78,60));    /* sandstone */
    make_metal    (9,rgb(55,60,65));                     /* dark metal */
    make_stone    (10,rgb(60,40,38));                    /* dark stone */
    make_rune     (11,rgb(55,20,55));                    /* purple rune */
    make_floor_tile(12,rgb(44,44,50),rgb(32,32,38));     /* dungeon floor */
    make_ceil_panel(13,rgb(20,20,22));                   /* dungeon ceil */
    make_lava     (14);                                  /* lava */
    make_ice      (15);                                  /* ice */
    make_floor_tile(16,rgb(60,45,30),rgb(48,36,22));     /* wood floor */
    make_ceil_panel(17,rgb(35,28,18));                   /* wood ceil */
}
static inline u32 tex_sample(int id,float u,float v){
    int tx=(int)(u*(TEX_W-1))&(TEX_W-1);
    int ty=(int)(v*(TEX_H-1))&(TEX_H-1);
    return tex[id<NUM_TEX?id:0][ty][tx];
}

/* ═══════════════════ MAPS (40×40) ═══════════════════ */
/* Map texture sets: [floor_tex, ceil_tex, wall_tex_per_digit] */
static const int WFLOOR[NUM_MAPS] = {12,16,12,12,15,12};
static const int WCEIL [NUM_MAPS] = {13,17,13,13,13,13};
static const int WTEX[NUM_MAPS][10]={
    /* 0=DUNGEON */  {0, 0,1,2,3,6,7,0,0,0},
    /* 1=TEMPLE  */  {0, 8,8,2,5,9,6,0,0,0},
    /* 2=FORTRESS*/  {0, 7,9,2,1,3,0,0,0,0},
    /* 3=INFERNO */  {0,10,11,7,5,1,0,0,0,0},
    /* 4=GLACIER */  {0, 1,15,2,5,4,0,0,0,0},
    /* 5=DUST2   */  {0, 8,7,1,8,9,8,0,0,0},
};

/* MAP0: THE DUNGEON */
static const char MAP0[MAP_H][MAP_W+1]={
    "1111111111111111111111111111111111111111",
    "1......................................1",
    "1..111111..........11111111............1",
    "1..122221.................1............1",
    "1..122221.................1....33333...1",
    "1..122221.............55..1....33333...1",
    "1..11.111..........1......1....33333...1",
    "1.....1..1.........11111111....33333...1",
    "1.....1..1.............................1",
    "1.....1..111...111111..................1",
    "1.....1.......1......1.....22222.......1",
    "1.....11111...1..44..1.....22222.......1",
    "1.............1......1.....22222.......1",
    "1.............11111..1.....22222.......1",
    "1....................1.................1",
    "1...........111111111....6666666.......1",
    "1.......................6.......6......1",
    "1....3333...............6..555..6......1",
    "1....3333...............6..............1",
    "1....3333...............6666666........1",
    "1......................................1",
    "1..555555......44444....33333..........1",
    "1..555555......44444....33333..........1",
    "1..555555......44444....33333..........1",
    "1..555555......44444....33333..........1",
    "1......................................1",
    "1....111111111......222222222..........1",
    "1....1.............24444444442.........1",
    "1....1....555......24444444442.........1",
    "1....1.............24444444442.........1",
    "1....111111111.....222222222...........1",
    "1......................................1",
    "1......................................1",
    "1..2222222...3333333...................1",
    "1..2222222...3333333...................1",
    "1..2222222...3333333...................1",
    "1..2222222...3333333...................1",
    "1......................................1",
    "1......................................1",
    "1111111111111111111111111111111111111111",
};

/* MAP1: THE TEMPLE */
static const char MAP1[MAP_H][MAP_W+1]={
    "4444444444444444444444444444444444444444",
    "4......................................4",
    "4..44444444....55555555....44444444....4",
    "4..4......4....5......5....4......4....4",
    "4..4......4....5......5....4......4....4",
    "4..4......4....55555555....4......4....4",
    "4..44444444.......................4....4",
    "4..........44444444444444444......4....4",
    "4..........4...................4....4..4",
    "4..........4...333333333......4...4....4",
    "4..........4...3........3.....44444....4",
    "4..........4...3...55...3..............4",
    "4..........4...3........3..............4",
    "4..........4...333333333...............4",
    "4..........4......................66...4",
    "4..........444444444444444444.....66...4",
    "4......................................4",
    "4...5555555.....................55555..4",
    "4...5.....5.....................5...5..4",
    "4...5.....5.....................5...5..4",
    "4...5.....5.....................55555..4",
    "4...5555555............................4",
    "4......................................4",
    "4...44444.......33333......44444444....4",
    "4...4...4.......3...3......4......4....4",
    "4...4...4.......3...3......4......4....4",
    "4...4...4.......33333......4......4....4",
    "4...44444..................44444444....4",
    "4......................................4",
    "4..........55555555....................4",
    "4..........5......5....................4",
    "4..........5..44..5....................4",
    "4..........5......5....................4",
    "4..........55555555....................4",
    "4......................................4",
    "4......................................4",
    "4......................................4",
    "4......................................4",
    "4......................................4",
    "4444444444444444444444444444444444444444",
};

/* MAP2: THE FORTRESS */
static const char MAP2[MAP_H][MAP_W+1]={
    "1111111111111111111111111111111111111111",
    "1......................................1",
    "1...7777777777777777777777777777777....1",
    "1...7.............................7....1",
    "1...7...2222222.......2222222.....7....1",
    "1...7...2.....2.......2.....2.....7....1",
    "1...7...2..3..2.......2..3..2.....7....1",
    "1...7...2.....2.......2.....2.....7....1",
    "1...7...2222222.......2222222.....7....1",
    "1...7.............................7....1",
    "1...7...............9.............7....1",
    "1...7...............9.............7....1",
    "1...7...555555555...9...111111....7....1",
    "1...7...5.......5...9...1....1....7....1",
    "1...7...5...7...5...9...1....1....7....1",
    "1...7...5.......5...9...111111....7....1",
    "1...7...555555555......................1",
    "1...7.............................7....1",
    "1...7...2222222.......3333333.....7....1",
    "1...7...2.....2.......3.....3.....7....1",
    "1...7...2.....2.......3.....3.....7....1",
    "1...7...2222222.......3333333.....7....1",
    "1...7.............................7....1",
    "1...7...............1.............7....1",
    "1...7...............1.............7....1",
    "1...7777777777111777711177777777777....1",
    "1......................................1",
    "1......................................1",
    "1...1111111111111111111111111111111....1",
    "1...1.............................1....1",
    "1...1.....2222....33333...44444...1....1",
    "1...1.....2..2....3...3...4...4...1....1",
    "1...1.....2222....33333...44444...1....1",
    "1...1.............................1....1",
    "1...11111111111111111111111111111111...1",
    "1......................................1",
    "1......................................1",
    "1......................................1",
    "1......................................1",
    "1111111111111111111111111111111111111111",
};

/* MAP3: THE INFERNO - has lava pits (cells marked 'L' = just decorative since we only do walls) */
static const char MAP3[MAP_H][MAP_W+1]={
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
    "A......................................A",
    "A..1111.....55555....111111111111......A",
    "A..1..1.....5...5....1..........1......A",
    "A..1..1.....5...5....1..2222....1......A",
    "A..1111.....55555....1..2..2....1......A",
    "A..........1.........1..2222....1......A",
    "A..........1.........1..........1......A",
    "A..........111111111.111111111111......A",
    "A......................................A",
    "A...33333.....55555......33333333......A",
    "A...3...3.....5...5......3.......3.....A",
    "A...3...3.....5...5......3..111..3.....A",
    "A...33333.....55555......3..1.1..3.....A",
    "A...........1..1.........3..111..3.....A",
    "A...........1..1.........3.......3.....A",
    "A...........1..1.........33333333......A",
    "A......................................A",
    "A...........111111111111111............A",
    "A...........1...................1......A",
    "A...........1...22222...33333...1......A",
    "A...........1...2...2...3...3...1......A",
    "A...........1...2...2...3...3...1......A",
    "A...........1...22222...33333...1......A",
    "A...........1...................1......A",
    "A...........111111111111111............A",
    "A......................................A",
    "A...5555555............................A",
    "A...5.....5............................A",
    "A...5..3..5....3333333.................A",
    "A...5.....5....3.....3.................A",
    "A...5555555....3.....3.................A",
    "A..............3333333.................A",
    "A......................................A",
    "A......................................A",
    "A......................................A",
    "A......................................A",
    "A......................................A",
    "A......................................A",
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
};

/* MAP4: THE GLACIER */
static const char MAP4[MAP_H][MAP_W+1]={
    "2222222222222222222222222222222222222222",
    "2......................................2",
    "2..5555555555555555555555555555555.....2",
    "2..5.............................5.....2",
    "2..5..11111.........22222........5.....2",
    "2..5..1...1.........2...2........5.....2",
    "2..5..1...1.........2...2........5.....2",
    "2..5..11111.........22222........5.....2",
    "2..5.............................5.....2",
    "2..5555555555555555555555555555555.....2",
    "2......................................2",
    "2...22222..............................2",
    "2...2...2..............................2",
    "2...2...2...5555555555555555555........2",
    "2...22222...5.................5........2",
    "2...........5..11111...22222..5........2",
    "2...........5..1...1...2...2..5........2",
    "2...........5..11111...22222..5........2",
    "2...........5.................5........2",
    "2...........5555555555555555555........2",
    "2......................................2",
    "2...111111111..........................2",
    "2...1.......1..........................2",
    "2...1...5...1..........................2",
    "2...1.......1..........................2",
    "2...111111111..........................2",
    "2......................................2",
    "2......22222...........................2",
    "2......2...2...........................2",
    "2......2...2...11111111111111..........2",
    "2......22222...1...........1...........2",
    "2..............1..3333333..1...........2",
    "2..............1..3.....3..1...........2",
    "2..............1..3.....3..1...........2",
    "2..............1..3333333..1...........2",
    "2..............1...........1...........2",
    "2..............11111111111111..........2",
    "2......................................2",
    "2......................................2",
    "2222222222222222222222222222222222222222",
};

static const char MAP5[MAP_H][MAP_W+1]={
    "1111111111111111111111111111111111111111",
    "1......................................1",
    "1......................................1",
    "1....1....1.111...1111.................1",
    "1.1111111111111......11111.............1",
    "1.11111..111..1..........11............1",
    "1.111.1..11111111111111..11............1",
    "1.111.111...11....111111111............1",
    "1...1.111..1111...11...1111111.........1",
    "1...1.111111111...11...1111111.........1",
    "1...1111111.......11...11111.1.........1",
    "1...111..1........11...11111.1....111..1",
    "1...1.1..11......111...11....1.1111.1..1",
    "1...111.111....111.111111.1111.1111111.1",
    "1...1...111....11..11.11111111.1111.1111",
    "1...1111111....11111111111.111.1.11.1111",
    "1...1111111......11.11.....11111.11.1111",
    "1......11.........1.11.....1111111..1111",
    "1.....1111........1.11.....11.1111..1111",
    "1.....1.111111....1.11.....1111111..1111",
    "1..1111.111111...11.11.....11111111.1..1",
    "1..1111111111111111111............1.1..1",
    "1...1111...11111111111111111......1.1..1",
    "1...11.11........11111111111......1.1..1",
    "1...111111........111...1111......1.1111",
    "1..11111111.......111...1111......1..111",
    "1..1111111111111111111111111111111111.11",
    "1..1111.111111.....1111111.1.....1111.11",
    "1..1111111111......1111111.111.111111111",
    "1..11111.111..11...1111111.11111111.1..1",
    "1..1111111111111.11111.111111111111.1..1",
    "1...1111111111111.......11.11111111.1..1",
    "1...11111111111.........1111.111111.1..1",
    "1..1111111.....................11...1..1",
    "1..111111......................111111..1",
    "1..111.1...............................1",
    "1..111............11...................1",
    "111111............11...................1",
    "1.1111............11...................1",
    "1111111111111111111111111111111111111111",
};

static const char* MAPS_DATA[NUM_MAPS]={
    (const char*)MAP0,(const char*)MAP1,(const char*)MAP2,
    (const char*)MAP3,(const char*)MAP4,(const char*)MAP5
};
static const char* MAP_NAMES[NUM_MAPS]={
    "THE DUNGEON","THE TEMPLE","THE FORTRESS","THE INFERNO","THE GLACIER","DE_DUST2"
};
static const char* MAP_DESC[NUM_MAPS]={
    "Ancient corridors filled with undead",
    "Sacred halls guarded by cultists",
    "Heavily fortified military complex",
    "Hellfire and demonic horrors await",
    "Frozen wastes inhabited by ice beasts",
    "Classic CS map. Find the bomb sites!"
};

static inline char get_map_cell(int m,int x,int y){
    if(x<0||x>=MAP_W||y<0||y>=MAP_H)return '1';
    return MAPS_DATA[m][y*(MAP_W+1)+x];
}
static inline int map_wall(int m,int x,int y){
    char c=get_map_cell(m,x,y);
    if(c>='1'&&c<='9')return c-'0';
    if(c>='A'&&c<='Z')return 1; /* all uppercase = wall type 1 */
    return 0;
}
static inline int is_wall(int m,float x,float y){return map_wall(m,(int)x,(int)y)!=0;}

/* ═══════════════════ WEAPONS ═══════════════════ */
typedef struct{
    const char *name;
    int ammo_max,ammo_per_clip;
    float dmg_min,dmg_max,fire_rate,reload_time;
    int spread,cost,owned;
    u32 color;
}Weapon;
static Weapon weapons[NUM_WEAPONS]={
    {"PISTOL",    200, 15, 15.f,28.f, 2.5f,1.2f,  2,   0,1,0},
    {"SHOTGUN",    60,  8,  8.f,18.f, 1.2f,2.0f, 14, 450,0,0},
    {"MACHINEGUN",300, 30,  8.f,14.f, 8.0f,2.5f,  6, 750,0,0},
    {"PLASMA",    120, 20, 20.f,38.f, 3.5f,1.8f,  0,1200,0,0},
    {"ROCKET",     30,  4, 45.f,75.f, 0.7f,2.5f,  0,2000,0,0},
};

/* ═══════════════════ ENTITIES ═══════════════════ */
typedef enum{ES_IDLE,ES_PATROL,ES_ALERT,ES_CHASE,ES_ATTACK,ES_HURT,ES_DEAD}EState;
typedef enum{IT_HEALTH,IT_AMMO,IT_ARMOR,IT_COIN,IT_KEY}ItemType;

/* 5 enemy types: GRUNT, BRUTE, PHANTOM, DEMON, ICE_GOLEM */
typedef enum{ET_GRUNT=0,ET_BRUTE=1,ET_PHANTOM=2,ET_DEMON=3,ET_ICE_GOLEM=4}EType;

typedef struct{
    float x,y,health,max_health,shoot_cd,state_t,anim_t;
    float ptx,pty,pt;
    float alert_timer; /* seconds player must be in range before alerting */
    int alerting,active;
    EType type;
    EState state;
}Enemy;

typedef struct{float x,y,bob_t;int active;ItemType type;int value;}Item;
typedef struct{float x,y,vx,vy,life,size;u32 col;int active,gravity;}Particle;

typedef struct{
    float x,y,angle,health,armor;
    int ammo,clip,kills,coins;
    float shoot_cd,hurt_t,bob_t,cur_speed,recoil;
    int cur_weapon;
    int crouching;
}Player;

/* ═══════════════════ SPAWN DATA ═══════════════════ */
typedef struct{float x,y;}V2;
static V2 EPOS[NUM_MAPS][MAX_ENEMIES]={
 /* MAP0 DUNGEON */
 {{5.5f,5.5f},{10.5f,5.5f},{16.5f,5.5f},{22.5f,5.5f},{28.5f,5.5f},
  {5.5f,12.5f},{10.5f,12.5f},{16.5f,12.5f},{22.5f,12.5f},{28.5f,12.5f},
  {5.5f,20.5f},{10.5f,20.5f},{16.5f,20.5f},{22.5f,20.5f},{28.5f,20.5f},
  {5.5f,28.5f},{10.5f,28.5f},{16.5f,28.5f},{22.5f,28.5f},{28.5f,28.5f},
  {15.5f,15.5f},{20.5f,15.5f},{15.5f,25.5f},{20.5f,25.5f}},
 /* MAP1 TEMPLE */
 {{6.5f,6.5f},{33.5f,6.5f},{6.5f,33.5f},{33.5f,33.5f},{20.5f,20.5f},
  {10.5f,15.5f},{28.5f,15.5f},{10.5f,25.5f},{28.5f,25.5f},{20.5f,8.5f},
  {20.5f,31.5f},{6.5f,20.5f},{33.5f,20.5f},{15.5f,10.5f},{25.5f,10.5f},
  {15.5f,30.5f},{25.5f,30.5f},{12.5f,20.5f},{27.5f,20.5f},{20.5f,15.5f},
  {20.5f,25.5f},{8.5f,12.5f},{31.5f,12.5f},{8.5f,28.5f}},
 /* MAP2 FORTRESS */
 {{5.5f,5.5f},{35.5f,5.5f},{5.5f,35.5f},{35.5f,35.5f},{20.5f,12.5f},
  {8.5f,18.5f},{30.5f,18.5f},{8.5f,25.5f},{30.5f,25.5f},{20.5f,20.5f},
  {15.5f,8.5f},{25.5f,8.5f},{5.5f,20.5f},{35.5f,20.5f},{20.5f,35.5f},
  {12.5f,30.5f},{28.5f,30.5f},{6.5f,13.5f},{32.5f,13.5f},{6.5f,28.5f},
  {32.5f,28.5f},{15.5f,20.5f},{25.5f,20.5f},{20.5f,28.5f}},
 /* MAP3 INFERNO */
 {{5.5f,5.5f},{33.5f,5.5f},{5.5f,33.5f},{33.5f,33.5f},{18.5f,18.5f},
  {7.5f,10.5f},{30.5f,10.5f},{7.5f,25.5f},{30.5f,25.5f},{18.5f,7.5f},
  {18.5f,29.5f},{5.5f,18.5f},{33.5f,18.5f},{13.5f,13.5f},{23.5f,13.5f},
  {13.5f,23.5f},{23.5f,23.5f},{10.5f,18.5f},{27.5f,18.5f},{18.5f,13.5f},
  {18.5f,23.5f},{8.5f,8.5f},{28.5f,8.5f},{8.5f,28.5f}},
 /* MAP4 GLACIER */
 {{4.5f,4.5f},{35.5f,4.5f},{4.5f,35.5f},{35.5f,35.5f},{20.5f,10.5f},
  {6.5f,15.5f},{32.5f,15.5f},{6.5f,25.5f},{32.5f,25.5f},{20.5f,20.5f},
  {14.5f,6.5f},{26.5f,6.5f},{4.5f,20.5f},{35.5f,20.5f},{20.5f,35.5f},
  {10.5f,10.5f},{30.5f,10.5f},{10.5f,30.5f},{30.5f,30.5f},{15.5f,20.5f},
  {25.5f,20.5f},{20.5f,15.5f},{20.5f,25.5f},{18.5f,18.5f}},
 /* MAP5 DE_DUST2 */
 {{3.5f,3.5f},{37.5f,3.5f},{3.5f,37.5f},{35.5f,37.5f},{20.5f,2.5f},
  {10.5f,5.5f},{5.5f,12.5f},{20.5f,12.5f},{30.5f,5.5f},{35.5f,10.5f},
  {25.5f,20.5f},{15.5f,22.5f},{10.5f,28.5f},{20.5f,30.5f},{30.5f,28.5f},
  {35.5f,35.5f},{25.5f,35.5f},{15.5f,35.5f},{5.5f,35.5f},{20.5f,20.5f},
  {10.5f,15.5f},{30.5f,15.5f},{20.5f,25.5f},{15.5f,10.5f}},
};

/* Enemy type per slot per map */
static EType ETYP[NUM_MAPS][MAX_ENEMIES]={
 {ET_GRUNT,ET_GRUNT,ET_BRUTE,ET_GRUNT,ET_PHANTOM,
  ET_GRUNT,ET_BRUTE,ET_GRUNT,ET_GRUNT,ET_BRUTE,
  ET_PHANTOM,ET_GRUNT,ET_DEMON,ET_GRUNT,ET_BRUTE,
  ET_GRUNT,ET_PHANTOM,ET_BRUTE,ET_GRUNT,ET_GRUNT,
  ET_DEMON,ET_GRUNT,ET_PHANTOM,ET_BRUTE},
 {ET_GRUNT,ET_GRUNT,ET_BRUTE,ET_BRUTE,ET_DEMON,
  ET_GRUNT,ET_BRUTE,ET_GRUNT,ET_PHANTOM,ET_BRUTE,
  ET_GRUNT,ET_PHANTOM,ET_DEMON,ET_GRUNT,ET_BRUTE,
  ET_GRUNT,ET_GRUNT,ET_BRUTE,ET_PHANTOM,ET_DEMON,
  ET_GRUNT,ET_BRUTE,ET_GRUNT,ET_PHANTOM},
 {ET_BRUTE,ET_BRUTE,ET_DEMON,ET_DEMON,ET_PHANTOM,
  ET_BRUTE,ET_DEMON,ET_BRUTE,ET_PHANTOM,ET_GRUNT,
  ET_BRUTE,ET_PHANTOM,ET_GRUNT,ET_DEMON,ET_BRUTE,
  ET_BRUTE,ET_GRUNT,ET_PHANTOM,ET_DEMON,ET_BRUTE,
  ET_GRUNT,ET_PHANTOM,ET_BRUTE,ET_DEMON},
 {ET_DEMON,ET_DEMON,ET_BRUTE,ET_PHANTOM,ET_DEMON,
  ET_BRUTE,ET_DEMON,ET_DEMON,ET_BRUTE,ET_PHANTOM,
  ET_DEMON,ET_GRUNT,ET_PHANTOM,ET_DEMON,ET_BRUTE,
  ET_DEMON,ET_PHANTOM,ET_DEMON,ET_GRUNT,ET_BRUTE,
  ET_DEMON,ET_PHANTOM,ET_BRUTE,ET_DEMON},
 {ET_ICE_GOLEM,ET_ICE_GOLEM,ET_PHANTOM,ET_ICE_GOLEM,ET_BRUTE,
  ET_ICE_GOLEM,ET_PHANTOM,ET_ICE_GOLEM,ET_BRUTE,ET_PHANTOM,
  ET_ICE_GOLEM,ET_GRUNT,ET_ICE_GOLEM,ET_PHANTOM,ET_ICE_GOLEM,
  ET_ICE_GOLEM,ET_BRUTE,ET_PHANTOM,ET_ICE_GOLEM,ET_GRUNT,
  ET_PHANTOM,ET_ICE_GOLEM,ET_BRUTE,ET_ICE_GOLEM},
 /* MAP5 DE_DUST2 */
 {ET_GRUNT,ET_GRUNT,ET_BRUTE,ET_PHANTOM,ET_GRUNT,
  ET_GRUNT,ET_BRUTE,ET_GRUNT,ET_DEMON,ET_GRUNT,
  ET_BRUTE,ET_GRUNT,ET_PHANTOM,ET_GRUNT,ET_BRUTE,
  ET_GRUNT,ET_DEMON,ET_GRUNT,ET_PHANTOM,ET_BRUTE,
  ET_GRUNT,ET_BRUTE,ET_GRUNT,ET_PHANTOM},
};

static V2 IPOS[NUM_MAPS][MAX_ITEMS]={
 {{9.5f,4.5f},{4.5f,19.5f},{19.5f,9.5f},{13.5f,13.5f},{7.5f,7.5f},{15.5f,18.5f},
  {19.5f,4.5f},{4.5f,13.5f},{10.5f,3.5f},{3.5f,10.5f},{16.5f,6.5f},{8.5f,16.5f},
  {22.5f,8.5f},{8.5f,22.5f},{22.5f,22.5f},{25.5f,12.5f},{30.5f,5.5f},{5.5f,30.5f},
  {30.5f,30.5f},{20.5f,35.5f}},
 {{4.5f,4.5f},{37.5f,37.5f},{4.5f,37.5f},{37.5f,4.5f},{20.5f,3.5f},{20.5f,36.5f},
  {3.5f,20.5f},{36.5f,20.5f},{8.5f,8.5f},{31.5f,8.5f},{8.5f,31.5f},{31.5f,31.5f},
  {14.5f,14.5f},{24.5f,14.5f},{14.5f,24.5f},{24.5f,24.5f},{10.5f,20.5f},
  {28.5f,20.5f},{20.5f,10.5f},{20.5f,28.5f}},
 {{5.5f,5.5f},{34.5f,5.5f},{5.5f,34.5f},{34.5f,34.5f},{20.5f,5.5f},{5.5f,20.5f},
  {34.5f,20.5f},{20.5f,34.5f},{10.5f,10.5f},{28.5f,10.5f},{10.5f,28.5f},{28.5f,28.5f},
  {15.5f,5.5f},{25.5f,5.5f},{15.5f,34.5f},{25.5f,34.5f},{5.5f,15.5f},
  {34.5f,15.5f},{5.5f,25.5f},{34.5f,25.5f}},
 {{6.5f,6.5f},{31.5f,6.5f},{6.5f,31.5f},{31.5f,31.5f},{18.5f,3.5f},{3.5f,18.5f},
  {34.5f,18.5f},{18.5f,34.5f},{12.5f,12.5f},{24.5f,12.5f},{12.5f,24.5f},{24.5f,24.5f},
  {9.5f,18.5f},{28.5f,18.5f},{18.5f,9.5f},{18.5f,28.5f},{6.5f,18.5f},
  {31.5f,18.5f},{18.5f,6.5f},{18.5f,31.5f}},
 {{4.5f,4.5f},{35.5f,4.5f},{4.5f,35.5f},{35.5f,35.5f},{20.5f,4.5f},{4.5f,20.5f},
  {35.5f,20.5f},{20.5f,35.5f},{10.5f,10.5f},{30.5f,10.5f},{10.5f,30.5f},{30.5f,30.5f},
  {15.5f,15.5f},{25.5f,15.5f},{15.5f,25.5f},{25.5f,25.5f},{20.5f,10.5f},
  {20.5f,30.5f},{10.5f,20.5f},{30.5f,20.5f}},
 /* MAP5 DE_DUST2 */
 {{3.5f,2.5f},{38.5f,2.5f},{2.5f,20.5f},{38.5f,20.5f},{20.5f,3.5f},
  {20.5f,38.5f},{10.5f,10.5f},{30.5f,10.5f},{10.5f,30.5f},{30.5f,30.5f},
  {15.5f,5.5f},{25.5f,5.5f},{5.5f,15.5f},{35.5f,15.5f},{5.5f,25.5f},
  {35.5f,25.5f},{15.5f,35.5f},{25.5f,35.5f},{20.5f,15.5f},{20.5f,25.5f}},
};
static ItemType ITYP[MAX_ITEMS]={
    IT_HEALTH,IT_AMMO,IT_ARMOR,IT_COIN,IT_HEALTH,IT_AMMO,
    IT_COIN,IT_ARMOR,IT_HEALTH,IT_AMMO,IT_COIN,IT_HEALTH,
    IT_AMMO,IT_ARMOR,IT_COIN,IT_HEALTH,IT_AMMO,IT_COIN,IT_ARMOR,IT_COIN
};
static V2    PSTART[NUM_MAPS]={{2.5f,2.5f},{2.5f,2.5f},{2.5f,2.5f},{2.5f,2.5f},{2.5f,2.5f},{2.5f,2.5f}};
static float PANG  [NUM_MAPS]={0.5f,0.5f,0.5f,0.5f,0.5f,0.5f};

/* ═══════════════════ GAME STATE ═══════════════════ */
static Player   player;
static Enemy    enemies[MAX_ENEMIES];
static Item     items[MAX_ITEMS];
static Particle particles[MAX_PARTICLES];
static int      cur_map=0,minimap_on=1,game_over=0,shop_open=0;
static int      fog_of_war[MAP_H][MAP_W];
static float    hurt_flash=0,shoot_flash=0,game_time=0;

/* ─── screen states ─── */
typedef enum{SCREEN_INTRO=0,SCREEN_MAPSEL,SCREEN_SETTINGS,SCREEN_GAME,SCREEN_PAUSE}Screen;
static Screen cur_screen=SCREEN_INTRO;
static int    map_sel_idx=0;
static float  intro_timer=0;

/* ─── settings ─── */
static struct{
    float mouse_sens;  /* 0.5–3.0 */
    float sfx_vol;     /* 0–1 */
    float bgm_vol;     /* 0–1 */
    int   fullscreen;
    int   crosshair;   /* 0=dot 1=classic 2=none */
    int   fov_idx;     /* 0=narrow 1=normal 2=wide */
    int   settings_sel;
}settings={1.0f,0.8f,0.5f,0,1,1,0};

/* ═══════════════════ AUDIO ═══════════════════ */
#define AUDIO_FREQ 44100
#define AUDIO_BUF  512
#define MAX_SND    16

typedef struct{float ph,freq,vol,decay,t,dur;int active,type;}SndCh;
static SndCh snd[MAX_SND];
static SDL_AudioDeviceID aud;
static SDL_mutex *aud_mtx;

/* BGM state (procedural music engine) */
#define BGM_VOICES 4
typedef struct{
    float ph,freq,vol;
    int   note_t;   /* samples until next note */
    int   pattern;  /* which pattern for this map */
    int   step;
}BgmVoice;
static BgmVoice bgm[BGM_VOICES];
static int bgm_on=1;

/* Pentatonic-ish note tables per map mood */
static float NOTE_DARK []={110.f,130.8f,146.8f,164.8f,196.f,220.f,246.9f,261.6f};
static float NOTE_TEMPLE[]={130.8f,155.6f,174.6f,196.f,233.1f,261.6f,311.1f,349.2f};
static float NOTE_FORT []={87.3f,103.8f,116.5f,130.8f,155.6f,174.6f,196.f,207.7f};
static float NOTE_HELL []={82.4f,92.5f,110.f,123.5f,138.6f,155.6f,164.8f,185.f};
static float NOTE_ICE  []={174.6f,196.f,220.f,246.9f,293.7f,329.6f,369.9f,392.f};
static float* MAP_NOTES[NUM_MAPS]={NOTE_DARK,NOTE_TEMPLE,NOTE_FORT,NOTE_HELL,NOTE_ICE};

static void bgm_tick(float *out, int n){
    static int init=0;
    if(!init){
        for(int v=0;v<BGM_VOICES;v++){bgm[v].ph=0;bgm[v].freq=110.f;bgm[v].vol=0;bgm[v].note_t=0;bgm[v].step=v*3;}
        init=1;
    }
    float *notes=MAP_NOTES[cur_map<NUM_MAPS?cur_map:0];
    int interval[BGM_VOICES]={22050,33075,44100,66150}; /* note durations in samples */
    float vol_base[BGM_VOICES]={0.18f,0.12f,0.08f,0.06f};
    int patterns[BGM_VOICES][8]={
        {0,2,4,5,4,2,1,0},
        {3,5,7,5,3,1,0,1},
        {0,0,3,3,5,5,4,4},
        {7,5,3,1,0,2,4,6}
    };
    for(int i=0;i<n;i++){
        float s=0;
        for(int v=0;v<BGM_VOICES;v++){
            if(bgm[v].note_t<=0){
                bgm[v].step=(bgm[v].step+1)%8;
                int ni=patterns[v][bgm[v].step];
                bgm[v].freq=notes[ni&7];
                if(v==3) bgm[v].freq*=0.5f; /* bass octave down */
                bgm[v].vol=vol_base[v]*settings.bgm_vol;
                bgm[v].note_t=interval[v];
            }
            bgm[v].note_t--;
            /* voice 3 is square wave bass, others sine */
            float wave;
            if(v==3) wave=(sinf(bgm[v].ph*6.28318f)>0)?1.f:-1.f;
            else wave=sinf(bgm[v].ph*6.28318f);
            s+=wave*bgm[v].vol;
            bgm[v].ph+=bgm[v].freq/(float)AUDIO_FREQ;
            if(bgm[v].ph>1.f)bgm[v].ph-=1.f;
        }
        out[i]+=s;
    }
}

static void audio_cb(void*ud,uint8_t*stream,int len){
    (void)ud; int16_t*out=(int16_t*)stream; int n=len/2;
    float mix[2048]={0};
    SDL_LockMutex(aud_mtx);
    for(int i=0;i<n&&i<2048;i++){
        float s=0;
        for(int c=0;c<MAX_SND;c++){
            if(!snd[c].active)continue;
            float v=snd[c].vol*settings.sfx_vol;
            switch(snd[c].type){
                case 0:s+=sinf(snd[c].ph*6.28318f)*v;break;
                case 1:s+=((float)(rand()&0xFF)/128.f-1.f)*v;break;
                case 2:s+=(sinf(snd[c].ph*6.28318f)>0?1.f:-1.f)*v;break;
                case 3:s+=(sinf(snd[c].ph*6.28318f)+sinf(snd[c].ph*12.5664f)*0.5f)*v;break;
            }
            snd[c].ph+=snd[c].freq/(float)AUDIO_FREQ;
            if(snd[c].ph>1.f)snd[c].ph-=1.f;
            snd[c].t+=1.f/AUDIO_FREQ; snd[c].vol-=snd[c].decay/AUDIO_FREQ;
            if(snd[c].t>=snd[c].dur||snd[c].vol<=0)snd[c].active=0;
        }
        mix[i]=s;
    }
    SDL_UnlockMutex(aud_mtx);
    /* BGM on top (no mutex needed, single-float ops) */
    if(bgm_on) bgm_tick(mix,n>2048?2048:n);
    for(int i=0;i<n;i++) out[i]=(int16_t)(clampf(mix[i>2047?2047:i],-1.f,1.f)*26000);
}

static void play_snd(float freq,float vol,float dur,int type){
    if(!aud)return;
    SDL_LockMutex(aud_mtx);
    for(int i=0;i<MAX_SND;i++) if(!snd[i].active){
        snd[i]=(SndCh){0,freq,vol,vol/dur,0,dur,1,type};break;
    }
    SDL_UnlockMutex(aud_mtx);
}
static void snd_shoot(int w){
    switch(w){
        case 0:play_snd(200,0.8f,0.07f,1);play_snd(80,0.5f,0.12f,1);break;
        case 1:play_snd(120,1.0f,0.12f,1);play_snd(60,0.7f,0.2f,1);play_snd(40,0.4f,0.15f,2);break;
        case 2:play_snd(280,0.6f,0.04f,1);play_snd(140,0.4f,0.06f,1);break;
        case 3:play_snd(440,0.7f,0.1f,3);play_snd(220,0.4f,0.15f,0);break;
        case 4:play_snd(80,1.0f,0.22f,1);play_snd(55,0.8f,0.35f,2);play_snd(130,0.5f,0.18f,0);break;
    }
}
static void snd_hit(void){play_snd(130,0.6f,0.1f,1);play_snd(65,0.4f,0.18f,0);}
static void snd_die(void){play_snd(90,0.8f,0.3f,1);play_snd(55,0.5f,0.4f,0);play_snd(40,0.3f,0.5f,2);}
static void snd_pickup(void){play_snd(440,0.4f,0.06f,0);play_snd(660,0.4f,0.06f,0);play_snd(880,0.3f,0.1f,0);}
static void snd_step(void){play_snd(55+(int)(rnf()*15),0.1f,0.04f,1);}
static void snd_hurt(void){play_snd(180,0.8f,0.09f,1);play_snd(110,0.5f,0.18f,2);}
static void snd_empty(void){play_snd(180,0.3f,0.04f,2);}
static void snd_buy(void){play_snd(330,0.4f,0.06f,0);play_snd(440,0.4f,0.06f,0);play_snd(550,0.5f,0.12f,3);}
static void snd_ui(void){play_snd(600,0.3f,0.05f,0);}

/* ═══════════════════ PARTICLES ═══════════════════ */
static void spawn_ptcl(float wx,float wy,u32 col,int n,float spd,int grav){
    for(int i=0;i<MAX_PARTICLES&&n>0;i++){
        if(particles[i].active)continue;
        float ang=rnf()*6.28318f,s=spd*(0.3f+rnf()*0.9f);
        particles[i]=(Particle){wx,wy,cosf(ang)*s,sinf(ang)*s,0.3f+rnf()*0.7f,2.f+rnf()*3.f,col,1,grav};
        n--;
    }
}
static void update_ptcl(float dt){
    for(int i=0;i<MAX_PARTICLES;i++){
        if(!particles[i].active)continue;
        particles[i].life-=dt;
        if(particles[i].life<=0){particles[i].active=0;continue;}
        particles[i].x+=particles[i].vx*dt; particles[i].y+=particles[i].vy*dt;
        if(particles[i].gravity)particles[i].vy+=3.5f*dt;
        particles[i].vx*=0.97f; particles[i].vy*=0.97f;
    }
}

/* ═══════════════════ FONT ═══════════════════ */
static const uint8_t FONT[96][7]={
 {0,0,0,0,0,0,0},{4,4,4,4,0,4,0},{10,10,0,0,0,0,0},{10,31,10,31,10,0,0},
 {14,21,12,6,21,14,0},{17,10,4,10,17,0,0},{6,9,6,21,9,22,0},{4,4,0,0,0,0,0},
 {2,4,4,4,2,0,0},{8,4,4,4,8,0,0},{0,10,4,10,0,0,0},{0,4,14,4,0,0,0},
 {0,0,0,0,4,8,0},{0,0,14,0,0,0,0},{0,0,0,0,4,0,0},{1,2,4,8,16,0,0},
 {14,17,19,21,25,14,0},{4,12,4,4,4,14,0},{14,17,2,4,8,31,0},{31,2,6,1,17,14,0},
 {2,6,10,31,2,2,0},{31,16,30,1,17,14,0},{6,8,30,17,17,14,0},{31,1,2,4,8,8,0},
 {14,17,14,17,17,14,0},{14,17,17,15,1,14,0},{0,4,0,4,0,0,0},{0,4,0,4,8,0,0},
 {2,4,8,4,2,0,0},{0,31,0,31,0,0,0},{8,4,2,4,8,0,0},{14,17,2,4,0,4,0},
 {14,17,23,21,22,12,0},{14,17,31,17,17,17,0},{30,17,30,17,17,30,0},{14,17,16,16,17,14,0},
 {28,18,17,17,18,28,0},{31,16,30,16,16,31,0},{31,16,30,16,16,16,0},{14,17,16,19,17,14,0},
 {17,17,31,17,17,17,0},{14,4,4,4,4,14,0},{1,1,1,1,17,14,0},{17,18,28,20,18,17,0},
 {16,16,16,16,16,31,0},{17,27,21,17,17,17,0},{17,25,21,19,17,17,0},{14,17,17,17,17,14,0},
 {30,17,30,16,16,16,0},{14,17,17,21,18,13,0},{30,17,30,20,18,17,0},{15,16,14,1,1,30,0},
 {31,4,4,4,4,4,0},{17,17,17,17,17,14,0},{17,17,17,17,10,4,0},{17,17,21,21,21,10,0},
 {17,10,4,10,17,0,0},{17,10,4,4,4,4,0},{31,2,4,8,16,31,0},{14,8,8,8,8,14,0},
 {16,8,4,2,1,0,0},{14,2,2,2,2,14,0},{4,10,17,0,0,0,0},{0,0,0,0,0,31,0},
 {8,4,0,0,0,0,0},{0,14,1,15,17,15,0},{16,16,30,17,17,30,0},{0,14,16,16,16,14,0},
 {1,1,15,17,17,15,0},{0,14,17,31,16,14,0},{6,8,28,8,8,8,0},{0,15,17,15,1,14,0},
 {16,16,30,17,17,17,0},{4,0,4,4,4,14,0},{2,0,2,2,18,12,0},{16,17,18,28,18,17,0},
 {12,4,4,4,4,14,0},{0,26,21,21,21,17,0},{0,30,17,17,17,17,0},{0,14,17,17,17,14,0},
 {0,30,17,30,16,16,0},{0,15,17,15,1,1,0},{0,22,25,16,16,16,0},{0,14,16,14,1,30,0},
 {8,28,8,8,9,6,0},{0,17,17,17,17,14,0},{0,17,17,17,10,4,0},{0,17,21,21,21,10,0},
 {0,17,10,4,10,17,0},{0,17,17,15,1,14,0},{0,31,2,4,8,31,0},{6,4,8,4,6,0,0},
 {4,4,4,4,4,0,0},{12,4,2,4,12,0,0},{8,21,2,0,0,0,0},{31,31,31,31,31,0,0},
};
static void draw_char(int x,int y,char c,u32 col,int sc){
    int idx=(unsigned char)c-32;if(idx<0||idx>=96)return;
    for(int r=0;r<7;r++){uint8_t b=FONT[idx][r];
        for(int bit=4;bit>=0;bit--) if(b&(1<<bit))
            for(int sy=0;sy<sc;sy++) for(int sx=0;sx<sc;sx++)
                pset(x+(4-bit)*sc+sx,y+r*sc+sy,col);}
}
static void draw_str(int x,int y,const char*s,u32 col,int sc){
    int ox=x;while(*s){if(*s=='\n'){x=ox;y+=7*sc+2;s++;continue;}
        draw_char(x,y,*s,col,sc);x+=6*sc;s++;}
}
static void draw_strf(int x,int y,u32 col,int sc,const char*fmt,...){
    char buf[256];va_list va;va_start(va,fmt);vsnprintf(buf,sizeof buf,fmt,va);va_end(va);
    draw_str(x,y,buf,col,sc);
}
static int str_w(const char*s,int sc){int n=0;while(*s++){n+=6*sc;}return n>0?n-sc:0;}

/* ═══════════════════ LOAD MAP ═══════════════════ */
static void load_map(int id){
    cur_map=id;
    player.x=PSTART[id].x; player.y=PSTART[id].y; player.angle=PANG[id];
    player.health=100.f; player.armor=0.f; player.crouching=0;
    if(id==0){player.ammo=weapons[0].ammo_per_clip*2;player.clip=weapons[0].ammo_per_clip;}
    else{player.ammo=weapons[player.cur_weapon].ammo_per_clip*2;
         player.clip=weapons[player.cur_weapon].ammo_per_clip;}
    player.shoot_cd=0; player.hurt_t=0; player.bob_t=0; player.cur_speed=0; player.recoil=0;

    /* enemy health by type */
    float hp_table[5]={60.f,160.f,80.f,220.f,300.f};

    for(int i=0;i<MAX_ENEMIES;i++){
        EType t=ETYP[id][i];
        float spd=1.0f+(float)(i%4)*0.2f;
        enemies[i]=(Enemy){EPOS[id][i].x,EPOS[id][i].y,hp_table[t],hp_table[t],
            0,0,0,EPOS[id][i].x+1.5f,EPOS[id][i].y,0,0,0,1,t,ES_IDLE};
        (void)spd;
    }
    for(int i=0;i<MAX_ITEMS;i++){
        int val=(ITYP[i]==IT_COIN)?((i%4==0)?50:25):0;
        items[i]=(Item){IPOS[id][i].x,IPOS[id][i].y,(float)i*0.4f,1,ITYP[i],val};
    }
    memset(fog_of_war,0,sizeof fog_of_war);
    memset(particles,0,sizeof particles);
    game_over=0; shop_open=0;
    game_time=0;
}

/* ═══════════════════ PLAYER SHOOT ═══════════════════ */
static void player_shoot(void){
    Weapon *w=&weapons[player.cur_weapon];
    if(player.clip<=0){snd_empty();return;}
    if(player.shoot_cd>0)return;
    player.clip--; player.shoot_cd=1.f/w->fire_rate; player.recoil=1.f;
    snd_shoot(player.cur_weapon); shoot_flash=0.12f;
    spawn_ptcl(player.x+cosf(player.angle)*0.6f,player.y+sinf(player.angle)*0.6f,
               rgb(220,190,80),10,0.6f,0);

    int rays=(player.cur_weapon==1)?6:1;
    for(int ray=0;ray<rays;ray++){
        float spread=(float)w->spread*0.001f;
        float ang=player.angle+(rnf()-0.5f)*spread*2.f;
        float best=ENEMY_ATTACK_RANGE+2.f; int hit=-1;
        for(int i=0;i<MAX_ENEMIES;i++){
            if(!enemies[i].active||enemies[i].health<=0)continue;
            float dx=enemies[i].x-player.x,dy=enemies[i].y-player.y;
            float d=sqrtf(dx*dx+dy*dy); if(d>best)continue;
            float ea=atan2f(dy,dx),diff=ea-ang;
            while(diff>(float)M_PI)diff-=2*(float)M_PI;
            while(diff<-(float)M_PI)diff+=2*(float)M_PI;
            if(fabsf(diff)>0.15f+0.1f/(d+0.5f))continue;
            /* DDA wall check */
            float rdx=cosf(ang),rdy=sinf(ang);
            int mx=(int)player.x,my=(int)player.y;
            float ddx=fabsf(rdx)<1e-8f?1e30f:fabsf(1.f/rdx);
            float ddy=fabsf(rdy)<1e-8f?1e30f:fabsf(1.f/rdy);
            float sx,sy; int stepx,stepy;
            if(rdx<0){stepx=-1;sx=(player.x-mx)*ddx;}else{stepx=1;sx=(mx+1.f-player.x)*ddx;}
            if(rdy<0){stepy=-1;sy=(player.y-my)*ddy;}else{stepy=1;sy=(my+1.f-player.y)*ddy;}
            int blocked=0;
            for(int s=0;s<64;s++){
                float wd=(sx<sy)?sx:sy; if(wd>=d)break;
                if(sx<sy){sx+=ddx;mx+=stepx;}else{sy+=ddy;my+=stepy;}
                if(map_wall(cur_map,mx,my)){blocked=1;break;}
            }
            if(blocked)continue;
            best=d; hit=i;
        }
        if(hit>=0){
            /* rocket does splash damage */
            float dmg=w->dmg_min+rnf()*(w->dmg_max-w->dmg_min);
            /* headshot bonus (25% chance) */
            if(rnf()<0.18f) dmg*=1.5f;
            enemies[hit].health-=dmg; enemies[hit].alerting=1;
            enemies[hit].state=ES_HURT; enemies[hit].state_t=0.22f;
            snd_hit();
            spawn_ptcl(enemies[hit].x,enemies[hit].y,rgb(220,15,15),20,2.2f,1);
            spawn_ptcl(enemies[hit].x,enemies[hit].y,rgb(255,80,0),6,1.f,0);
            if(enemies[hit].health<=0){
                enemies[hit].state=ES_DEAD; player.kills++;
                for(int ii=0;ii<MAX_ITEMS;ii++) if(!items[ii].active){
                    items[ii]=(Item){enemies[hit].x,enemies[hit].y,0,1,IT_COIN,25};break;
                }
                snd_die();
                spawn_ptcl(enemies[hit].x,enemies[hit].y,rgb(160,15,15),30,2.5f,1);
                /* alert nearby enemies on death */
                for(int j=0;j<MAX_ENEMIES;j++){
                    if(j==hit||!enemies[j].active||enemies[j].health<=0)continue;
                    float ex=enemies[j].x-enemies[hit].x,ey=enemies[j].y-enemies[hit].y;
                    if(sqrtf(ex*ex+ey*ey)<6.f) enemies[j].alerting=1;
                }
            }
        }
    }
}

/* ═══════════════════ UPDATE ═══════════════════ */
static float step_t=0;
static void update(float dt){
    game_time+=dt;
    if(player.shoot_cd>0)player.shoot_cd-=dt;
    if(player.hurt_t>0) player.hurt_t-=dt*2.f;
    if(player.recoil>0) player.recoil-=dt*5.f;
    if(shoot_flash>0)   shoot_flash-=dt;
    if(hurt_flash>0)    hurt_flash-=dt;
    player.bob_t+=dt; player.cur_speed*=0.82f;
    if(player.cur_speed>0.5f){step_t-=dt;if(step_t<=0){snd_step();step_t=0.4f;}}

    for(int i=0;i<MAX_ITEMS;i++){
        if(!items[i].active)continue;
        items[i].bob_t+=dt;
        float dx=items[i].x-player.x,dy=items[i].y-player.y;
        if(sqrtf(dx*dx+dy*dy)<0.65f){
            snd_pickup();
            switch(items[i].type){
                case IT_HEALTH:player.health=clampf(player.health+40.f,0,100.f);
                    spawn_ptcl(items[i].x,items[i].y,rgb(40,220,40),12,1.2f,0);break;
                case IT_AMMO:  player.ammo=clampi(player.ammo+30,0,weapons[player.cur_weapon].ammo_max);
                    spawn_ptcl(items[i].x,items[i].y,rgb(220,200,40),10,1.f,0);break;
                case IT_ARMOR: player.armor=clampf(player.armor+35.f,0,100.f);
                    spawn_ptcl(items[i].x,items[i].y,rgb(40,120,220),10,1.f,0);break;
                case IT_COIN:  player.coins+=items[i].value;
                    spawn_ptcl(items[i].x,items[i].y,rgb(255,215,0),10,1.2f,0);break;
                case IT_KEY:break;
            }
            items[i].active=0;
        }
    }

    /* enemy type properties */
    float spd_table[5] ={1.3f,1.8f,2.6f,2.0f,1.5f};     /* GRUNT,BRUTE,PHANTOM,DEMON,ICE */
    float range_table[5]={6.f,4.f,8.f,7.f,5.f};           /* attack range per type */
    float cd_table[5]  ={1.8f,2.2f,1.0f,1.2f,2.5f};       /* shoot cooldown base */
    float dmg_table[5] ={5.f,9.f,6.f,11.f,8.f};           /* base damage */

    for(int i=0;i<MAX_ENEMIES;i++){
        if(!enemies[i].active)continue;
        if(enemies[i].health<=0){enemies[i].state=ES_DEAD;continue;}
        enemies[i].anim_t+=dt; enemies[i].state_t-=dt;
        float dx=player.x-enemies[i].x,dy=player.y-enemies[i].y;
        float d=sqrtf(dx*dx+dy*dy);
        EType t=enemies[i].type;

        /* ── Line-of-sight check for alerting ── */
        /* Only alert if can see player (no wall in between) AND close enough */
        float alert_r=ENEMY_ALERT_RANGE;
        if(player.crouching) alert_r*=0.55f; /* crouching reduces detection */
        if(d<alert_r&&!enemies[i].alerting){
            /* DDA LOS check */
            float rdx=dx/d,rdy=dy/d;
            int mx=(int)enemies[i].x,my=(int)enemies[i].y;
            float ddx2=fabsf(rdx)<1e-8f?1e30f:fabsf(1.f/rdx);
            float ddy2=fabsf(rdy)<1e-8f?1e30f:fabsf(1.f/rdy);
            float sx2,sy2; int stepx2,stepy2;
            if(rdx<0){stepx2=-1;sx2=(enemies[i].x-mx)*ddx2;}else{stepx2=1;sx2=(mx+1.f-enemies[i].x)*ddx2;}
            if(rdy<0){stepy2=-1;sy2=(enemies[i].y-my)*ddy2;}else{stepy2=1;sy2=(my+1.f-enemies[i].y)*ddy2;}
            int los_clear=1;
            for(int s=0;s<80;s++){
                float wd=(sx2<sy2)?sx2:sy2; if(wd>=d)break;
                if(sx2<sy2){sx2+=ddx2;mx+=stepx2;}else{sy2+=ddy2;my+=stepy2;}
                if(map_wall(cur_map,mx,my)){los_clear=0;break;}
            }
            if(los_clear){
                enemies[i].alert_timer+=dt;
                if(enemies[i].alert_timer>0.5f) enemies[i].alerting=1;
            } else {
                enemies[i].alert_timer*=0.9f; /* decay if blocked */
            }
        }

        if(!enemies[i].alerting){
            enemies[i].state=ES_PATROL;
            float ptx=enemies[i].ptx-enemies[i].x,pty=enemies[i].pty-enemies[i].y;
            float pd=sqrtf(ptx*ptx+pty*pty);
            if(pd<0.4f||enemies[i].pt>5.f){
                enemies[i].ptx=EPOS[cur_map][i].x+(rnf()-0.5f)*5.f;
                enemies[i].pty=EPOS[cur_map][i].y+(rnf()-0.5f)*5.f;
                enemies[i].pt=0;
            }
            enemies[i].pt+=dt;
            if(pd>0.01f){
                float ps=spd_table[t]*0.5f*dt;
                float nx=enemies[i].x+(ptx/pd)*ps,ny=enemies[i].y+(pty/pd)*ps;
                if(!is_wall(cur_map,nx,enemies[i].y))enemies[i].x=nx;
                if(!is_wall(cur_map,enemies[i].x,ny))enemies[i].y=ny;
            }
            continue;
        }

        /* PHANTOM: can phase through some walls (just faster movement) */
        float spd2=(t==ET_PHANTOM?spd_table[t]*1.2f:spd_table[t])*dt;
        if(enemies[i].state_t<=0)
            enemies[i].state=(d<ENEMY_MELEE_RANGE)?ES_ATTACK:ES_CHASE;

        if(d>0.8f&&enemies[i].state!=ES_HURT){
            float nx=enemies[i].x+(dx/d)*spd2,ny=enemies[i].y+(dy/d)*spd2;
            /* separation from other enemies */
            for(int j=0;j<MAX_ENEMIES;j++){
                if(j==i||!enemies[j].active||enemies[j].health<=0)continue;
                float ex=enemies[j].x-nx,ey=enemies[j].y-ny;
                if(sqrtf(ex*ex+ey*ey)<0.75f){nx=enemies[i].x;ny=enemies[i].y;break;}
            }
            if(t!=ET_PHANTOM){ /* normal wall collision */
                if(!is_wall(cur_map,nx,enemies[i].y))enemies[i].x=nx;
                if(!is_wall(cur_map,enemies[i].x,ny))enemies[i].y=ny;
            } else {
                /* phantom can move through walls but gets slowed */
                enemies[i].x=nx; enemies[i].y=ny;
            }
        }

        /* ── Enemy attack: MUST have LOS and be within range ── */
        enemies[i].shoot_cd-=dt;
        float atk_range=range_table[t];
        if(enemies[i].shoot_cd<=0&&d<atk_range){
            /* DDA LOS check before attacking */
            float rdx=dx/d,rdy=dy/d;
            int mx=(int)enemies[i].x,my=(int)enemies[i].y;
            float ddx2=fabsf(rdx)<1e-8f?1e30f:fabsf(1.f/rdx);
            float ddy2=fabsf(rdy)<1e-8f?1e30f:fabsf(1.f/rdy);
            float sx2,sy2; int stepx2,stepy2;
            if(rdx<0){stepx2=-1;sx2=(enemies[i].x-mx)*ddx2;}else{stepx2=1;sx2=(mx+1.f-enemies[i].x)*ddx2;}
            if(rdy<0){stepy2=-1;sy2=(enemies[i].y-my)*ddy2;}else{stepy2=1;sy2=(my+1.f-enemies[i].y)*ddy2;}
            int blocked=0;
            for(int s=0;s<64;s++){
                float wd=(sx2<sy2)?sx2:sy2; if(wd>=d)break;
                if(sx2<sy2){sx2+=ddx2;mx+=stepx2;}else{sy2+=ddy2;my+=stepy2;}
                if(map_wall(cur_map,mx,my)){blocked=1;break;}
            }
            float cd_b=cd_table[t]+(float)(rand()%12)*0.1f;
            enemies[i].shoot_cd=cd_b;

            if(!blocked){
                enemies[i].state=ES_ATTACK; enemies[i].state_t=0.3f;
                float base_d=dmg_table[t]*ENEMY_DAMAGE_SCALE;
                /* damage falls off with distance */
                float dist_factor=clampf(1.f-(d/atk_range)*0.5f,0.3f,1.0f);
                float dmg=(base_d+(float)(rand()%5)*0.5f)*dist_factor;
                float actual=player.armor>0?dmg*0.45f:dmg;
                if(player.crouching)actual*=0.7f; /* crouching reduces damage taken */
                if(player.armor>0){player.armor-=dmg*0.4f;if(player.armor<0)player.armor=0;}
                player.health-=actual; player.hurt_t=1.f; hurt_flash=0.2f; snd_hurt();
                if(player.health<=0){player.health=0;game_over=1;}
                spawn_ptcl(player.x,player.y,rgb(200,0,0),6,0.8f,0);
            }
        }
    }
    update_ptcl(dt);
}

/* ═══════════════════ RAYCASTER ═══════════════════ */
static void render_world(void){
    int fl=WFLOOR[cur_map],cl=WCEIL[cur_map];
    float ca=cosf(player.angle),sa=sinf(player.angle);
    float fov_mult=1.0f;
    if(settings.fov_idx==0)fov_mult=0.8f;
    else if(settings.fov_idx==2)fov_mult=1.25f;
    float px=-sa*tanf(FOV_RAD*0.5f*fov_mult),py=ca*tanf(FOV_RAD*0.5f*fov_mult);
    /* vertical offset for crouching */
    int half_h2=HALF_H+(player.crouching?30:0);

    for(int col=0;col<VIEW_W;col++){
        float cam=2.f*col/(float)VIEW_W-1.f;
        float rdx=ca+px*cam,rdy=sa+py*cam;
        int mx=(int)player.x,my=(int)player.y;
        float ddx=fabsf(rdx)<1e-8f?1e30f:fabsf(1.f/rdx);
        float ddy=fabsf(rdy)<1e-8f?1e30f:fabsf(1.f/rdy);
        float sx,sy;int stepx,stepy,side=0,wt=0;
        if(rdx<0){stepx=-1;sx=(player.x-mx)*ddx;}else{stepx=1;sx=(mx+1.f-player.x)*ddx;}
        if(rdy<0){stepy=-1;sy=(player.y-my)*ddy;}else{stepy=1;sy=(my+1.f-player.y)*ddy;}
        for(int s=0;s<100;s++){
            if(sx<sy){sx+=ddx;mx+=stepx;side=0;}else{sy+=ddy;my+=stepy;side=1;}
            wt=map_wall(cur_map,mx,my);if(wt)break;
        }
        float dist=(side==0)?((mx-player.x+(1-stepx)*0.5f)/rdx):((my-player.y+(1-stepy)*0.5f)/rdy);
        if(dist<0.01f)dist=0.01f;
        zbuf[col]=dist;
        int wh=(int)(VIEW_H/dist);if(wh>VIEW_H*2)wh=VIEW_H*2;
        int ds=half_h2-wh/2,de=half_h2+wh/2;
        if(ds<0)ds=0;if(de>=VIEW_H)de=VIEW_H-1;
        float wu;if(side==0)wu=player.y+dist*rdy;else wu=player.x+dist*rdx;
        wu-=floorf(wu);
        /* Get wall texture for this map and wall type */
        int wtype=(wt>0&&wt<10)?wt:1;
        int tid=WTEX[cur_map][wtype];
        /* Atmospheric fog with map-specific color tint */
        float fog=clampf(1.f-dist/MAX_DEPTH,0.05f,1.f);
        float light=fog*fog*(side==1?0.55f:1.f);
        /* Inferno map: add slight orange tint to walls */
        for(int row=ds;row<=de;row++){
            float tv=(float)(row-ds)/(float)(de-ds+1);
            u32 wc=darken(tex_sample(tid,wu,tv),light);
            if(cur_map==3) wc=blend_col(wc,rgb(40,10,0),0.15f);
            if(cur_map==4) wc=blend_col(wc,rgb(0,10,30),0.12f);
            pset(col,row,wc);
        }
        /* Floor & Ceiling */
        float fxw,fyw;
        if(side==0&&rdx>0){fxw=(float)mx;fyw=(float)my+wu;}
        else if(side==0){fxw=(float)mx+1.f;fyw=(float)my+wu;}
        else if(rdy>0){fxw=(float)mx+wu;fyw=(float)my;}
        else{fxw=(float)mx+wu;fyw=(float)my+1.f;}
        for(int row=de+1;row<VIEW_H;row++){
            float rd2=(float)VIEW_H/(2.f*row-VIEW_H+1);
            float wt2=rd2/dist;
            float fx=wt2*fxw+(1.f-wt2)*player.x;
            float fy=wt2*fyw+(1.f-wt2)*player.y;
            float ff=clampf(1.f-rd2/MAX_DEPTH,0,1);
            u32 fc=darken(tex_sample(fl,fx-floorf(fx),fy-floorf(fy)),ff*0.75f);
            u32 cc=darken(tex_sample(cl,fx-floorf(fx),fy-floorf(fy)),ff*0.5f);
            /* map atmosphere */
            if(cur_map==3){fc=blend_col(fc,rgb(40,5,0),0.2f);cc=blend_col(cc,rgb(20,3,0),0.15f);}
            if(cur_map==4){fc=blend_col(fc,rgb(0,5,20),0.15f);cc=blend_col(cc,rgb(0,5,25),0.2f);}
            pset(col,row,fc);
            int cr=VIEW_H-row-1;
            if(cr>=0&&cr<VIEW_H) pset(col,cr,cc);
        }
    }
}

/* ═══════════════════ SPRITES ═══════════════════ */
/* Draw 5 enemy types with distinct appearances */
static void draw_enemy_spr(int cx,int cy,int size,EState st,float anim_t,float dist,EType type){
    if(size<4||cx<-size||cx>VIEW_W+size)return;
    int half=size/2;
    float fog=clampf(1.f-dist/MAX_DEPTH,0.f,1.f);
    float light=fog*fog;
    int frame=(int)(anim_t*6.f)%2;

    /* colors per type */
    u32 body,body2,eyes,arm_c,detail;
    switch(type){
        case ET_GRUNT:
            body =darken(rgb(170,45,35),light); body2=darken(rgb(120,28,22),light);
            eyes =darken(rgb(255,220,20),light); arm_c=darken(rgb(80,85,95),light);
            detail=darken(rgb(100,100,100),light); break;
        case ET_BRUTE:
            body =darken(rgb(100,65,30),light); body2=darken(rgb(70,40,15),light);
            eyes =darken(rgb(255,120,20),light); arm_c=darken(rgb(120,100,60),light);
            detail=darken(rgb(140,100,50),light); break;
        case ET_PHANTOM:
            body =darken(rgb(120,80,180),light); body2=darken(rgb(70,40,120),light);
            eyes =darken(rgb(200,80,255),light); arm_c=darken(rgb(100,80,140),light);
            detail=darken(rgb(160,100,220),light); break;
        case ET_DEMON:
            body =darken(rgb(180,30,10),light); body2=darken(rgb(100,10,5),light);
            eyes =darken(rgb(255,60,0),light); arm_c=darken(rgb(120,30,15),light);
            detail=darken(rgb(220,80,20),light); break;
        case ET_ICE_GOLEM:
            body =darken(rgb(120,160,200),light); body2=darken(rgb(80,120,170),light);
            eyes =darken(rgb(100,220,255),light); arm_c=darken(rgb(100,140,180),light);
            detail=darken(rgb(200,230,255),light); break;
        default:
            body=darken(rgb(150,40,40),light); body2=darken(rgb(100,25,25),light);
            eyes=darken(rgb(255,255,0),light); arm_c=darken(rgb(80,80,80),light);
            detail=darken(rgb(120,120,120),light); break;
    }

    for(int py2=0;py2<size;py2++){
        float fy=(float)py2/size;
        for(int px2=0;px2<size;px2++){
            float fx=(float)px2/size;
            int rx=cx-half+px2,ry=cy-half+py2;
            if(rx<0||rx>=VIEW_W||ry<0||ry>=VIEW_H)continue;
            if(dist>=zbuf[rx])continue;
            u32 c=0;int draw=0;

            if(st==ES_DEAD){
                /* Dead: flat puddle */
                if(fy>0.62f&&fx>0.08f&&fx<0.92f){
                    c=(fy<0.78f)?body:body2;draw=1;
                    if(fy>0.68f&&fy<0.82f&&fx>0.3f&&fx<0.7f)c=rgb(120,8,8);
                }
            } else {
                if(type==ET_BRUTE){
                    /* BRUTE: wider, bulkier body */
                    float hd=sqrtf((fx-0.5f)*(fx-0.5f)+(fy-0.16f)*(fy-0.16f));
                    if(hd<0.16f){c=(hd<0.11f)?body:body2;draw=1;
                        float ex1=fabsf(fx-0.36f),ex2=fabsf(fx-0.64f);
                        if((ex1<0.06f||ex2<0.06f)&&fy>0.09f&&fy<0.2f){
                            c=st==ES_ATTACK?rgb(255,80,0):eyes;draw=1;}
                    }
                    /* wide torso */
                    if(fy>0.28f&&fy<0.68f&&fx>0.12f&&fx<0.88f){c=blend_col(body,body2,(fy-0.28f)/0.4f);draw=1;}
                    /* armor plates */
                    if(fy>0.3f&&fy<0.5f&&(fx<0.22f||fx>0.78f)){c=detail;draw=1;}
                    int aoff2=(frame&&st==ES_CHASE)?1:0;
                    if(fy>0.28f&&fy<0.62f){
                        if(fx>0.02f&&fx<0.14f){c=body;draw=1;}
                        if(fx>0.86f&&fx<0.98f+aoff2*0.01f){c=body;draw=1;}
                    }
                    if(fy>0.68f&&fy<1.f){
                        float loff2=frame?0.04f:0.f;
                        if(fx>0.18f&&fx<0.44f+loff2){c=body2;draw=1;}
                        if(fx>0.56f-loff2&&fx<0.82f){c=body2;draw=1;}
                    }
                } else if(type==ET_PHANTOM){
                    /* PHANTOM: wispy, floating shape */
                    float cd=sqrtf((fx-0.5f)*(fx-0.5f)+(fy-0.4f)*(fy-0.4f));
                    float t2=game_time;
                    float wobble=sinf(t2*4.f+fx*8.f)*0.04f;
                    if(cd<0.42f+wobble){
                        float alpha=1.f-(cd/(0.42f+wobble));
                        c=darken(body,(float)alpha*light);draw=1;
                        /* glowing eyes */
                        float ex=fabsf(fx-0.38f),ey=fabsf(fx-0.62f);
                        if((ex<0.05f||ey<0.05f)&&fy>0.2f&&fy<0.32f){c=eyes;draw=1;}
                        /* tentacle at bottom */
                        if(fy>0.7f&&fy<0.95f){
                            float tw=sinf(t2*3.f+fy*10.f)*0.12f;
                            if(fabsf(fx-0.5f-tw)<0.06f){c=body2;draw=1;}
                        }
                    }
                } else if(type==ET_DEMON){
                    /* DEMON: horns, wide mouth, intimidating */
                    float hd=sqrtf((fx-0.5f)*(fx-0.5f)+(fy-0.2f)*(fy-0.2f));
                    /* horns */
                    if(fy<0.1f){
                        if(fabsf(fx-0.3f)<0.04f||fabsf(fx-0.7f)<0.04f){c=detail;draw=1;}
                    }
                    if(hd<0.17f){c=(hd<0.12f)?body:body2;draw=1;
                        float ex1=fabsf(fx-0.36f),ex2=fabsf(fx-0.64f);
                        if((ex1<0.055f||ex2<0.055f)&&fy>0.13f&&fy<0.23f){
                            c=st==ES_ATTACK?rgb(255,100,0):eyes;draw=1;}
                        /* wide grin */
                        if(fy>0.26f&&fy<0.32f&&fx>0.3f&&fx<0.7f&&st==ES_ATTACK){c=rgb(255,200,20);draw=1;}
                    }
                    if(fy>0.35f&&fy<0.72f&&fx>0.18f&&fx<0.82f){c=blend_col(body,body2,(fy-0.35f)/0.37f);draw=1;}
                    /* wings! */
                    if(fy>0.35f&&fy<0.65f){
                        float wspan=0.4f*(1.f-(fy-0.35f)/0.3f);
                        if(fx<0.18f&&fx>0.18f-wspan){c=darken(body2,0.7f);draw=1;}
                        if(fx>0.82f&&fx<0.82f+wspan){c=darken(body2,0.7f);draw=1;}
                    }
                    int aoff3=(frame&&st==ES_CHASE)?1:0;
                    if(fy>0.72f&&fy<1.f){
                        float loff3=frame?0.03f:0.f;
                        if(fx>0.2f&&fx<0.46f+loff3){c=body2;draw=1;}
                        if(fx>0.54f-loff3&&fx<0.8f){c=body2;draw=1;}
                        (void)aoff3;
                    }
                } else if(type==ET_ICE_GOLEM){
                    /* ICE GOLEM: crystalline, angular */
                    /* head: diamond shape */
                    float ax=fabsf(fx-0.5f),ay=fabsf(fy-0.14f);
                    if(ax+ay<0.16f){c=(ax+ay<0.1f)?body:detail;draw=1;
                        if(fy>0.08f&&fy<0.16f&&(fabsf(fx-0.38f)<0.04f||fabsf(fx-0.62f)<0.04f)){
                            c=eyes;draw=1;}
                    }
                    /* crystal body: hexagonal-ish */
                    if(fy>0.28f&&fy<0.7f){
                        float bw=0.35f+0.15f*(1.f-fabsf(fy-0.49f)/0.21f);
                        if(fabsf(fx-0.5f)<bw){
                            c=blend_col(body,detail,(float)(fx-0.5f+bw)/(2.f*bw));draw=1;
                        }
                    }
                    /* ice shard arms */
                    int aoff4=(frame&&st==ES_CHASE)?1:0;
                    if(fy>0.3f&&fy<0.6f){
                        if(fx>0.04f&&fx<0.2f){c=detail;draw=1;}
                        if(fx>0.8f&&fx<0.96f+aoff4*0.01f){c=detail;draw=1;}
                    }
                    if(fy>0.7f&&fy<1.f){
                        float loff4=frame?0.035f:0.f;
                        if(fx>0.22f&&fx<0.46f+loff4){c=body2;draw=1;}
                        if(fx>0.54f-loff4&&fx<0.78f){c=body2;draw=1;}
                    }
                    (void)aoff4;
                } else {
                    /* GRUNT: original style */
                    float hd=sqrtf((fx-0.5f)*(fx-0.5f)+(fy-0.18f)*(fy-0.18f));
                    if(hd<0.14f){c=(hd<0.1f)?body:body2;draw=1;
                        float ex1=fabsf(fx-0.38f),ex2=fabsf(fx-0.62f);
                        if((ex1<0.05f||ex2<0.05f)&&fy>0.12f&&fy<0.2f){c=st==ES_ATTACK?rgb(255,50,50):eyes;draw=1;}
                        if(fy>0.22f&&fy<0.27f&&fx>0.38f&&fx<0.62f&&st==ES_ATTACK){c=rgb(200,20,20);draw=1;}
                    }
                    if(fy>0.3f&&fy<0.65f&&fx>0.18f&&fx<0.82f){c=blend_col(body,body2,(fy-0.3f)/0.35f);draw=1;}
                    int aoff5=(frame&&st==ES_CHASE)?1:0;
                    if(fy>0.32f&&fy<0.58f){
                        if(fx>0.06f&&fx<0.2f){c=body;draw=1;}
                        if(fx>0.8f&&fx<0.94f+aoff5*0.01f){c=body;draw=1;}
                    }
                    if(fy>0.64f&&fy<0.98f){
                        float loff5=frame?0.03f:0.f;
                        if(fx>0.23f&&fx<0.46f+loff5){c=body2;draw=1;}
                        if(fx>0.54f-loff5&&fx<0.77f){c=body2;draw=1;}
                        if(fy>0.9f){
                            if(fx>0.2f&&fx<0.48f+loff5){c=darken(body2,0.7f);draw=1;}
                            if(fx>0.52f-loff5&&fx<0.8f){c=darken(body2,0.7f);draw=1;}
                        }
                    }
                }
            }
            if(draw)pset(rx,ry,c);
        }
    }
    /* health bar above enemy */
    if(st!=ES_DEAD&&enemies[0].active){ /* find the enemy to get health fraction */
        int bidx=-1;
        for(int i=0;i<MAX_ENEMIES;i++){
            if(!enemies[i].active||enemies[i].health<=0)continue;
            float ddx2=enemies[i].x-player.x,ddy2=enemies[i].y-player.y;
            float dist2=sqrtf(ddx2*ddx2+ddy2*ddy2);
            if(fabsf(dist2-dist)<0.1f){bidx=i;break;}
        }
        if(bidx>=0&&dist<8.f){
            float hp_frac=enemies[bidx].health/enemies[bidx].max_health;
            int bw=size*3/5,bx2=cx-bw/2,by2=cy-half-8;
            fill_rect(bx2,by2,bw,4,rgb(40,40,40));
            u32 hbc=hp_frac>0.5f?rgb(40,200,40):hp_frac>0.25f?rgb(200,180,0):rgb(200,30,30);
            fill_rect(bx2,by2,(int)(bw*hp_frac),4,hbc);
        }
    }
}

static void draw_item_spr(int cx,int cy,int size,ItemType type,float dist){
    if(size<4)return;
    int half=size/2;
    float fog=clampf(1.f-dist/MAX_DEPTH,0.f,1.f);
    float light=fog*fog,t=game_time*3.f;
    for(int py2=0;py2<size;py2++){
        float fy=(float)py2/size;
        for(int px2=0;px2<size;px2++){
            float fx=(float)px2/size;
            int rx=cx-half+px2,ry=cy-half+py2;
            if(rx<0||rx>=VIEW_W||ry<0||ry>=VIEW_H)continue;
            if(dist>=zbuf[rx])continue;
            u32 c=0;int draw=0;
            switch(type){
                case IT_HEALTH:{
                    int cv=(fy>0.18f&&fy<0.82f&&fx>0.4f&&fx<0.6f);
                    int ch=(fx>0.18f&&fx<0.82f&&fy>0.4f&&fy<0.6f);
                    float glow=0.8f+sinf(t)*0.2f;
                    if(cv||ch){c=darken(rgb(220,30,30),light*glow);draw=1;}
                    else if(fx>0.12f&&fx<0.88f&&fy>0.12f&&fy<0.88f){c=darken(rgb(190,190,200),light);draw=1;}
                    break;}
                case IT_AMMO:{
                    if(fx>0.1f&&fx<0.9f&&fy>0.18f&&fy<0.85f){
                        if(fx<0.13f||fx>0.87f||fy<0.21f||fy>0.82f)c=darken(rgb(90,70,15),light);
                        else if(fy<0.42f)c=darken(rgb(180,155,50),light);
                        else c=darken(rgb(130,110,35),light);
                        draw=1;
                    }
                    if(fy>0.1f&&fy<0.22f&&fx>0.25f&&fx<0.75f){c=darken(rgb(200,180,60),light);draw=1;}
                    break;}
                case IT_ARMOR:{
                    float sx2=fx-0.5f,sw=0.42f*(1.f-fy*0.6f)+0.02f;
                    float glow=0.8f+sinf(t+1.f)*0.2f;
                    if(fabsf(sx2)<sw&&fy>0.08f&&fy<0.92f){
                        c=darken(fabsf(sx2)>sw-0.06f||fy<0.14f?rgb(80,140,220):rgb(40,90,175),light*glow);draw=1;}
                    break;}
                case IT_COIN:{
                    float cd=sqrtf((fx-0.5f)*(fx-0.5f)+(fy-0.5f)*(fy-0.5f));
                    float glow=0.85f+sinf(t*2.f)*0.15f;
                    if(cd<0.4f){c=darken(cd<0.28f?rgb(255,210,0):rgb(200,160,0),light*glow);draw=1;}
                    break;}
                case IT_KEY:{
                    /* key shape */
                    float kd=sqrtf((fx-0.3f)*(fx-0.3f)+(fy-0.35f)*(fy-0.35f));
                    float glow=0.8f+sinf(t)*0.2f;
                    if(kd<0.2f){c=darken(rgb(255,200,50),light*glow);draw=1;}
                    if(fx>0.44f&&fx<0.88f&&fy>0.48f&&fy<0.58f){c=darken(rgb(220,170,30),light*glow);draw=1;}
                    if(fx>0.65f&&fx<0.72f&&fy>0.58f&&fy<0.72f){c=darken(rgb(220,170,30),light*glow);draw=1;}
                    break;}
            }
            if(draw)pset(rx,ry,c);
        }
    }
}

static void render_sprites(void){
    typedef struct{float dist;int kind,idx;}SE;
    SE sl[MAX_ENEMIES+MAX_ITEMS];int ns=0;
    for(int i=0;i<MAX_ENEMIES;i++){if(!enemies[i].active)continue;
        float ddx2=enemies[i].x-player.x,ddy2=enemies[i].y-player.y;
        sl[ns++]=(SE){sqrtf(ddx2*ddx2+ddy2*ddy2),0,i};}
    for(int i=0;i<MAX_ITEMS;i++){if(!items[i].active)continue;
        float ddx2=items[i].x-player.x,ddy2=items[i].y-player.y;
        sl[ns++]=(SE){sqrtf(ddx2*ddx2+ddy2*ddy2),1,i};}
    for(int i=0;i<ns-1;i++) for(int j=i+1;j<ns;j++)
        if(sl[j].dist>sl[i].dist){SE tmp=sl[i];sl[i]=sl[j];sl[j]=tmp;}
    for(int si=0;si<ns;si++){
        float dist=sl[si].dist;if(dist<0.3f)continue;
        float sx,sy;
        if(sl[si].kind==0){sx=enemies[sl[si].idx].x;sy=enemies[sl[si].idx].y;}
        else{sx=items[sl[si].idx].x;sy=items[sl[si].idx].y;}
        float ddx2=sx-player.x,ddy2=sy-player.y;
        float ang=atan2f(ddy2,ddx2)-player.angle;
        while(ang>(float)M_PI)ang-=2*(float)M_PI;
        while(ang<-(float)M_PI)ang+=2*(float)M_PI;
        if(fabsf(ang)>FOV_RAD*0.75f)continue;
        int scx=(int)((ang/FOV_RAD+0.5f)*VIEW_W);
        int siz=(int)(VIEW_H*0.7f/dist);if(siz<2)continue;if(siz>VIEW_H)siz=VIEW_H;
        if(sl[si].kind==0){
            Enemy*e=&enemies[sl[si].idx];
            draw_enemy_spr(scx,HALF_H+(player.crouching?30:0),siz,e->state,e->anim_t,dist,e->type);
        } else {
            int bob=(int)(sinf(items[sl[si].idx].bob_t*3.f)*4.f*(siz/40.f));
            draw_item_spr(scx,HALF_H+bob+(player.crouching?30:0),siz*3/5,items[sl[si].idx].type,dist);
        }
    }
}

/* ═══════════════════ WEAPON RENDER ═══════════════════ */
static void render_weapon(void){
    float rc=player.recoil>0?player.recoil:0.f;
    float by2=sinf(player.bob_t*7.f)*player.cur_speed*0.012f;
    float bx2=cosf(player.bob_t*3.5f)*player.cur_speed*0.006f;
    /* crouching slightly lowers weapon */
    int crouch_off=player.crouching?20:0;
    int bx=(int)(VIEW_W/2-80+bx2*VIEW_W);
    int by=(int)(VIEW_H-130+by2*VIEW_H+rc*35.f+crouch_off);
    int w=player.cur_weapon;
    if(w==0){
        for(int y=0;y<100;y++) for(int x=0;x<160;x++){
            float fx=(float)x/160.f,fy=(float)y/100.f;
            int rx=bx+x,ry=by+y;
            if(rx<0||rx>=VIEW_W||ry<0||ry>=VIEW_H)continue;
            int barrel=(fx>0.36f&&fx<0.56f&&fy<0.32f);
            int slide =(fx>0.22f&&fx<0.78f&&fy>0.3f&&fy<0.62f);
            int grip  =(fx>0.3f&&fx<0.58f&&fy>0.6f);
            if(barrel)pset(rx,ry,fx<0.38f||fx>0.54f||fy<0.02f?rgb(185,190,205):rgb(62,67,82));
            else if(slide){u32 c=fy<0.32f?rgb(180,185,200):fx<0.24f?rgb(150,155,170):rgb(58,63,78);
                if(fx>0.33f&&fx<0.67f&&fy>0.34f&&fy<0.5f&&player.shoot_cd>0.05f)c=rgb(28,18,8);
                pset(rx,ry,c);}
            else if(grip){int gx=(int)(fx*8),gy=(int)(fy*8);pset(rx,ry,(gx+gy)&1?rgb(30,25,22):rgb(20,16,14));}
        }
    } else if(w==1){
        for(int y=0;y<120;y++) for(int x=0;x<200;x++){
            float fx=(float)x/200.f,fy=(float)y/120.f;
            int rx=bx-20+x,ry=by-10+y;
            if(rx<0||rx>=VIEW_W||ry<0||ry>=VIEW_H)continue;
            int b1=(fx>0.3f&&fx<0.46f&&fy<0.38f),b2=(fx>0.48f&&fx<0.64f&&fy<0.38f);
            int body=(fx>0.18f&&fx<0.82f&&fy>0.35f&&fy<0.62f);
            int stock=(fx>0.22f&&fx<0.55f&&fy>0.6f);
            if(b1||b2){u32 c=(b1?(fx<0.32f||fx>0.44f):(fx<0.5f||fx>0.62f))||fy<0.02f?rgb(65,70,80):rgb(45,48,55);pset(rx,ry,c);}
            else if(body)pset(rx,ry,fy<0.37f?rgb(90,70,40):rgb(70,55,30));
            else if(stock){int gx=(int)(fx*10),gy=(int)(fy*10);pset(rx,ry,(gx+gy)&1?rgb(80,55,25):rgb(65,45,20));}
        }
    } else if(w==2){
        for(int y=0;y<110;y++) for(int x=0;x<220;x++){
            float fx=(float)x/220.f,fy=(float)y/110.f;
            int rx=bx-30+x,ry=by-5+y;
            if(rx<0||rx>=VIEW_W||ry<0||ry>=VIEW_H)continue;
            int barrel=(fx>0.2f&&fx<0.85f&&fy<0.28f);
            int body=(fx>0.18f&&fx<0.82f&&fy>0.27f&&fy<0.62f);
            int mag=(fx>0.5f&&fx<0.65f&&fy>0.55f&&fy<0.9f);
            int stock=(fx>0.1f&&fx<0.3f&&fy>0.45f&&fy<0.7f);
            if(barrel)pset(rx,ry,(int)(fx*100)%8<4?rgb(55,60,72):rgb(45,50,62));
            else if(body)pset(rx,ry,fx<0.2f||fx>0.8f?rgb(60,65,78):rgb(50,55,68));
            else if(mag)pset(rx,ry,rgb(35,38,45));
            else if(stock){int gx=(int)(fx*8),gy=(int)(fy*8);pset(rx,ry,(gx+gy)&1?rgb(55,50,45):rgb(40,36,32));}
        }
    } else if(w==3){
        for(int y=0;y<100;y++) for(int x=0;x<180;x++){
            float fx=(float)x/180.f,fy=(float)y/100.f;
            int rx=bx-10+x,ry=by+y;
            if(rx<0||rx>=VIEW_W||ry<0||ry>=VIEW_H)continue;
            int barrel=(fx>0.25f&&fx<0.75f&&fy<0.3f);
            int body=(fx>0.15f&&fx<0.85f&&fy>0.28f&&fy<0.65f);
            int handle=(fx>0.35f&&fx<0.6f&&fy>0.63f);
            float glow=0.8f+sinf(game_time*8.f)*0.2f;
            if(barrel)pset(rx,ry,darken(rgb(20,180,130),glow));
            else if(body){u32 c=fx<0.2f||fx>0.8f?rgb(30,35,55):rgb(20,25,45);
                if((int)(fx*8+(fy*4))%2==0)c=blend_col(c,rgb(0,150,100),0.3f*glow);
                pset(rx,ry,c);}
            else if(handle){int gx=(int)(fx*6),gy=(int)(fy*6);pset(rx,ry,(gx+gy)&1?rgb(25,30,50):rgb(15,20,38));}
        }
    } else { /* ROCKET LAUNCHER */
        for(int y=0;y<110;y++) for(int x=0;x<200;x++){
            float fx=(float)x/200.f,fy=(float)y/110.f;
            int rx=bx-20+x,ry=by-5+y;
            if(rx<0||rx>=VIEW_W||ry<0||ry>=VIEW_H)continue;
            int tube=(fx>0.1f&&fx<0.9f&&fabsf(fy-0.3f)<0.22f);
            int body=(fx>0.2f&&fx<0.8f&&fy>0.5f&&fy<0.75f);
            int stock=(fx>0.1f&&fx<0.45f&&fy>0.73f);
            if(tube){
                float rad=fabsf(fy-0.3f)/0.22f;
                pset(rx,ry,rad>0.85f?rgb(45,50,60):rgb(60,65,75));
            }
            else if(body)pset(rx,ry,rgb(75,60,40));
            else if(stock){int gx=(int)(fx*8),gy=(int)(fy*8);pset(rx,ry,(gx+gy)&1?rgb(70,55,30):rgb(55,42,22));}
        }
    }
    /* Muzzle flash */
    if(rc>0.55f){
        float intensity=(rc-0.55f)*2.2f;
        int fx2=VIEW_W/2,fy2=by+(w==1?-15:w==2?-5:3);
        u32 fcol=(w==3)?rgb(0,(int)(200*intensity),(int)(140*intensity)):
                 (w==4)?rgb((int)(255*intensity),(int)(100*intensity),(int)(30*intensity)):
                 rgb((int)(255*intensity),(int)(180*intensity),0);
        int r2=(int)(25*intensity);
        for(int dy=-r2;dy<=r2;dy++) for(int dx=-r2;dx<=r2;dx++){
            float d=sqrtf((float)(dx*dx+dy*dy));if(d>r2)continue;
            float a=(1.f-d/r2)*intensity;
            int ry=fy2+dy,rxf=fx2+dx;
            if(rxf>=0&&rxf<VIEW_W&&ry>=0&&ry<VIEW_H)
                pset(rxf,ry,blend_col(pixels[ry][rxf],fcol,a));
        }
    }
}

/* ═══════════════════ HUD ═══════════════════ */
static void render_hud(void){
    /* gradient HUD background */
    for(int y=HUD_Y;y<WIN_H;y++){
        float t=(float)(y-HUD_Y)/HUD_H;
        u32 hud_bg;
        switch(cur_map){
            case 3: hud_bg=rgb((int)(12+t*6),(int)(5+t*2),(int)(5+t*2));break;
            case 4: hud_bg=rgb((int)(5+t*2),(int)(8+t*3),(int)(14+t*7));break;
            default:hud_bg=rgb((int)(8+t*3),(int)(8+t*3),(int)(14+t*5));break;
        }
        for(int x=0;x<WIN_W;x++) pset(x,y,hud_bg);
    }
    for(int x=0;x<WIN_W;x++){
        pset(x,HUD_Y,rgb(60,60,90));pset(x,HUD_Y+1,rgb(25,25,40));
    }
    int y0=HUD_Y+6;
    int hp=(int)player.health;
    u32 hc=hp>60?rgb(30,210,55):hp>30?rgb(210,170,25):rgb(210,45,35);
    /* HP */
    draw_str(18,y0,"HP",rgb(140,140,165),1);
    fill_rect(18,y0+10,102,12,rgb(18,18,24));
    fill_rect(18,y0+10,hp,12,hc);
    for(int x=18;x<18+hp&&x<120;x++) pset(x,y0+10,blend_col(hc,rgb(255,255,255),0.25f));
    outline_rect(18,y0+10,102,12,rgb(50,50,70));
    draw_strf(126,y0+8,hc,1,"%d",hp);
    /* Armor */
    int arm=(int)player.armor;
    draw_str(18,y0+26,"ARM",rgb(140,140,165),1);
    fill_rect(18,y0+36,102,8,rgb(18,18,24));fill_rect(18,y0+36,arm,8,rgb(40,100,195));
    outline_rect(18,y0+36,102,8,rgb(50,50,70));
    draw_strf(126,y0+34,rgb(80,140,210),1,"%d",arm);
    /* Weapon */
    int ax=280; Weapon*cw=&weapons[player.cur_weapon];
    draw_strf(ax,y0,rgb(140,140,165),1,"%.10s",cw->name);
    u32 ac=player.clip>cw->ammo_per_clip/3?rgb(210,190,50):player.clip>0?rgb(210,120,35):rgb(190,40,40);
    draw_strf(ax,y0+10,ac,3,"%d",player.clip);
    draw_strf(ax+48,y0+18,rgb(120,120,140),1,"/%d",player.ammo);
    int pips=player.clip>40?40:player.clip;
    for(int i=0;i<pips;i++){int px2=ax+(i%20)*7,py2=y0+38+(i/20)*7;fill_rect(px2,py2,5,4,ac);}
    /* Coins */
    draw_str(540,y0,"COINS",rgb(140,140,165),1);
    draw_strf(540,y0+10,rgb(255,215,0),3,"%d",player.coins);
    /* Kills */
    draw_str(700,y0,"KILLS",rgb(140,140,165),1);
    draw_strf(700,y0+10,rgb(210,75,75),3,"%d",player.kills);
    /* Map info */
    draw_strf(880,y0,rgb(140,140,165),1,"MAP %d/%d",cur_map+1,NUM_MAPS);
    draw_str(880,y0+10,MAP_NAMES[cur_map],rgb(110,110,155),1);
    draw_str(WIN_W-200,y0+30,"[TAB] SHOP",rgb(80,80,110),1);
    if(player.crouching)draw_str(WIN_W-200,y0+14,"[CROUCHING]",rgb(100,200,100),1);
    /* Crosshair */
    if(settings.crosshair!=2){
        int cxh=VIEW_W/2,cyh=VIEW_H/2;
        u32 xc=rgb(220,220,220);
        if(settings.crosshair==0){
            /* dot crosshair */
            fill_circle(cxh,cyh,3,rgb(255,80,80));
        } else {
            /* classic dynamic crosshair */
            int gap=(int)(6+player.cur_speed*0.5f);
            for(int i=gap;i<gap+9;i++){pset(cxh+i,cyh,xc);pset(cxh-i,cyh,xc);pset(cxh,cyh+i,xc);pset(cxh,cyh-i,xc);}
            pset(cxh,cyh,rgb(255,80,80));
        }
    }
}

/* ═══════════════════ SHOP ═══════════════════ */
static int shop_sel=0;
static void render_shop(void){
    for(int y=0;y<WIN_H;y++) for(int x=0;x<WIN_W;x++) pixels[y][x]=darken(pixels[y][x],0.35f);
    int pw=740,ph=520,px=(WIN_W-pw)/2,py=(WIN_H-ph)/2;
    fill_rect(px,py,pw,ph,rgb(10,10,18));
    outline_rect(px,py,pw,ph,rgb(60,60,100));
    outline_rect(px+2,py+2,pw-4,ph-4,rgb(30,30,55));
    const char*title="WEAPON SHOP";
    draw_str(px+(pw-str_w(title,3))/2,py+14,title,rgb(220,180,50),3);
    draw_line(px+20,py+44,px+pw-20,py+44,rgb(60,60,100));
    draw_strf(px+pw-150,py+14,rgb(255,215,0),2,"COINS: %d",player.coins);
    for(int i=0;i<NUM_WEAPONS;i++){
        Weapon*w=&weapons[i]; int wy=py+56+i*86,sel=(i==shop_sel);
        fill_rect(px+20,wy,pw-40,78,sel?rgb(22,22,42):rgb(13,13,25));
        outline_rect(px+20,wy,pw-40,78,sel?rgb(100,100,180):rgb(32,32,58));
        u32 nc=w->owned?rgb(80,200,80):rgb(200,200,200);
        draw_str(px+35,wy+10,w->name,nc,2);
        if(w->owned)draw_str(px+35+str_w(w->name,2)+8,wy+12,"[OWNED]",rgb(60,160,60),1);
        draw_strf(px+35, wy+32,rgb(160,160,180),1,"DMG: %.0f-%.0f",w->dmg_min,w->dmg_max);
        draw_strf(px+175,wy+32,rgb(160,160,180),1,"RATE: %.1f/s",w->fire_rate);
        draw_strf(px+320,wy+32,rgb(160,160,180),1,"CLIP: %d",w->ammo_per_clip);
        draw_strf(px+450,wy+32,rgb(160,160,180),1,"AMMO: %d",w->ammo_max);
        int dbar=(int)(w->dmg_max/80.f*150.f),fbar=(int)(w->fire_rate/10.f*150.f);
        fill_rect(px+35,wy+48,150,7,rgb(20,20,30));fill_rect(px+35,wy+48,dbar,7,rgb(200,50,50));
        fill_rect(px+35,wy+59,150,7,rgb(20,20,30));fill_rect(px+35,wy+59,fbar,7,rgb(50,180,50));
        if(i==player.cur_weapon){
            fill_rect(px+pw-120,wy+20,100,36,rgb(20,60,20));
            outline_rect(px+pw-120,wy+20,100,36,rgb(40,140,40));
            draw_str(px+pw-112,wy+30,"ACTIVE",rgb(60,200,60),1);
        } else if(w->owned){
            fill_rect(px+pw-120,wy+20,100,36,sel?rgb(30,80,30):rgb(18,50,18));
            outline_rect(px+pw-120,wy+20,100,36,rgb(40,130,40));
            draw_str(px+pw-108,wy+30,"EQUIP",rgb(80,210,80),1);
        } else {
            int can=(player.coins>=w->cost);
            fill_rect(px+pw-120,wy+20,100,36,can?(sel?rgb(50,40,10):rgb(28,22,7)):rgb(26,16,16));
            outline_rect(px+pw-120,wy+20,100,36,can?rgb(180,140,30):rgb(80,40,40));
            draw_strf(px+pw-120+(100-str_w("$9999",2))/2,wy+30,can?rgb(220,180,40):rgb(120,80,60),2,"$%d",w->cost);
        }
    }
    draw_str(px+20,py+ph-26,"UP/DN: Select   ENTER: Buy/Equip   TAB/ESC: Close",rgb(70,70,100),1);
}
static void shop_action(void){
    Weapon*w=&weapons[shop_sel];
    if(shop_sel==player.cur_weapon)return;
    if(w->owned){player.cur_weapon=shop_sel;
        player.clip=w->ammo_per_clip<player.ammo?w->ammo_per_clip:player.ammo;snd_buy();return;}
    if(player.coins>=w->cost){
        player.coins-=w->cost; w->owned=1;
        player.ammo=clampi(player.ammo+w->ammo_per_clip*2,0,w->ammo_max);
        player.cur_weapon=shop_sel; player.clip=w->ammo_per_clip; snd_buy();
    }
}

/* ═══════════════════ MINIMAP ═══════════════════ */
#define MM_C 3
static void render_minimap(void){
    int mmx=WIN_W-MAP_W*MM_C-6,mmy=6;
    int px_m=(int)player.x,py_m=(int)player.y;
    for(int dy=-6;dy<=6;dy++) for(int dx=-6;dx<=6;dx++){
        int nx=px_m+dx,ny=py_m+dy;
        if(nx>=0&&nx<MAP_W&&ny>=0&&ny<MAP_H)fog_of_war[ny][nx]=1;
    }
    fill_rect(mmx-2,mmy-2,MAP_W*MM_C+4,MAP_H*MM_C+4,rgb(4,4,8));
    outline_rect(mmx-2,mmy-2,MAP_W*MM_C+4,MAP_H*MM_C+4,rgb(40,40,65));
    for(int my=0;my<MAP_H;my++) for(int mx=0;mx<MAP_W;mx++){
        int rx=mmx+mx*MM_C,ry=mmy+my*MM_C;
        if(!fog_of_war[my][mx]){fill_rect(rx,ry,MM_C-1,MM_C-1,rgb(2,2,5));continue;}
        int w2=map_wall(cur_map,mx,my);
        fill_rect(rx,ry,MM_C-1,MM_C-1,w2?rgb(62,65,82):rgb(14,16,22));
    }
    for(int i=0;i<MAX_ITEMS;i++){if(!items[i].active)continue;
        int rx=mmx+(int)(items[i].x*MM_C),ry=mmy+(int)(items[i].y*MM_C);
        u32 ic=items[i].type==IT_HEALTH?rgb(30,180,30):items[i].type==IT_ARMOR?rgb(30,90,200):
               items[i].type==IT_COIN?rgb(220,180,0):rgb(200,180,40);
        fill_rect(rx,ry,MM_C-1,MM_C-1,ic);}
    for(int i=0;i<MAX_ENEMIES;i++){if(!enemies[i].active||enemies[i].health<=0)continue;
        if(!fog_of_war[(int)enemies[i].y][(int)enemies[i].x])continue;
        int rx=mmx+(int)(enemies[i].x*MM_C),ry=mmy+(int)(enemies[i].y*MM_C);
        u32 ec;
        switch(enemies[i].type){
            case ET_PHANTOM:ec=rgb(150,30,200);break;
            case ET_DEMON:ec=rgb(200,60,0);break;
            case ET_BRUTE:ec=rgb(180,120,0);break;
            case ET_ICE_GOLEM:ec=rgb(80,180,220);break;
            default:ec=rgb(200,35,35);break;
        }
        fill_rect(rx-1,ry-1,MM_C+1,MM_C+1,ec);}
    int ppx=mmx+(int)(player.x*MM_C),ppy=mmy+(int)(player.y*MM_C);
    fill_rect(ppx-1,ppy-1,MM_C+1,MM_C+1,rgb(50,240,70));
    for(int i=2;i<9;i++) pset((int)(ppx+cosf(player.angle)*i),(int)(ppy+sinf(player.angle)*i),rgb(240,240,60));
    /* Map name label */
    draw_str(mmx,mmy+MAP_H*MM_C+3,MAP_NAMES[cur_map],rgb(90,90,130),1);
}

/* ═══════════════════ EFFECTS ═══════════════════ */
static void render_vignette(void){
    for(int y=0;y<VIEW_H;y++) for(int x=0;x<VIEW_W;x++){
        float fx=(float)x/VIEW_W,fy=(float)y/VIEW_H;
        float e=fx*(1-fx)*fy*(1-fy)*16.f;
        pixels[y][x]=darken(pixels[y][x],0.62f+0.38f*clampf(e,0,1));
    }
}
static void render_hurt(void){
    if(hurt_flash<=0)return;
    float a=hurt_flash*0.7f; int b=(int)(VIEW_W*0.1f*a)+2;
    for(int y=0;y<VIEW_H;y++) for(int x=0;x<b;x++){
        pixels[y][x]=blend_col(pixels[y][x],rgb(200,15,15),a*0.85f);
        pixels[y][VIEW_W-1-x]=blend_col(pixels[y][VIEW_W-1-x],rgb(200,15,15),a*0.85f);}
    for(int x=0;x<VIEW_W;x++) for(int y=0;y<b;y++){
        pixels[y][x]=blend_col(pixels[y][x],rgb(200,15,15),a*0.65f);
        pixels[VIEW_H-1-y][x]=blend_col(pixels[VIEW_H-1-y][x],rgb(200,15,15),a*0.65f);}
}
static void render_flash(void){
    if(shoot_flash<=0)return;
    u32 fc=(player.cur_weapon==3)?rgb(0,200,140):rgb(255,220,100);
    for(int y=0;y<VIEW_H;y++) for(int x=0;x<VIEW_W;x++)
        pixels[y][x]=blend_col(pixels[y][x],fc,shoot_flash*0.09f);
}

/* ═══════════════════ INTRO SCREEN ═══════════════════ */
static void draw_intro(float t){
    /* Animated dark background */
    for(int y=0;y<WIN_H;y++) for(int x=0;x<WIN_W;x++){
        float fy=(float)y/WIN_H,fx=(float)x/WIN_W;
        float wave=sinf(fx*4.f+t)*0.04f+sinf(fy*6.f+t*1.3f)*0.03f;
        pixels[y][x]=rgb((int)(4+fx*8+wave*20),(int)(2+fy*5+wave*10),(int)(10+fy*20+wave*30));
    }
    /* Scanlines */
    for(int y=0;y<WIN_H;y+=3) for(int x=0;x<WIN_W;x++) pixels[y][x]=darken(pixels[y][x],0.6f);
    /* Particles stars */
    for(int s=0;s<80;s++){
        rng_s=(uint32_t)(s*12345u+6789u);
        int sx=(int)(rnf()*WIN_W),sy=(int)(rnf()*VIEW_H);
        float bright=0.4f+sinf(t*2.f+s*0.3f)*0.3f+0.3f;
        int bv=(int)(bright*200);
        pset(sx,sy,rgb(bv,bv,bv));
    }

    /* Title glow shadow */
    int glow=(int)(140+sinf(t)*40);
    for(int d=-3;d<=3;d++) draw_str(WIN_W/2-250+d,WIN_H/2-155+d,"ZERO BREACH",rgb(glow/4,0,0),7);
    draw_str(WIN_W/2-250,WIN_H/2-155,"ZERO BREACH",rgb(glow,40,30),7);

    /* Subtitle */
    draw_str(WIN_W/2-200,WIN_H/2-30,"ULTIMATE  EDITION",rgb(170,150,130),2);
    draw_line(WIN_W/2-210,WIN_H/2-8,WIN_W/2+210,WIN_H/2-8,rgb(80,60,40));

    /* Menu options */
    const char* opts[]={"START GAME","SETTINGS"};
    static int sel2=0;
    for(int i=0;i<2;i++){
        int ox=WIN_W/2-str_w(opts[i],2)/2;
        int oy=WIN_H/2+20+i*38;
        if(sel2==i){
            fill_rect(ox-14,oy-4,str_w(opts[i],2)+28,22,rgb(25,20,12));
            outline_rect(ox-14,oy-4,str_w(opts[i],2)+28,22,rgb(180,130,30));
            draw_str(ox,oy,opts[i],rgb(240,200,60),2);
            draw_str(ox-10,oy,">",rgb(240,180,30),2);
        } else {
            draw_str(ox,oy,opts[i],rgb(140,120,90),2);
        }
    }

    int pulse=(int)(150+sinf(t*2.5f)*60);
    draw_str(WIN_W/2-120,WIN_H/2+120,"PRESS ENTER TO SELECT",rgb(pulse,pulse,60),1);

    /* store sel for input */
    static int *sel_ptr=NULL;
    if(!sel_ptr){static int s2=0;sel_ptr=&s2;}
    *sel_ptr=sel2;
    (void)sel_ptr;

    /* Controls hint */
    draw_str(WIN_W/2-140,WIN_H/2+150,"W/S = Navigate  ENTER = Select",rgb(80,80,100),1);
}

/* We need a persistent intro_sel */
static int intro_sel=0;

/* ═══════════════════ MAP SELECTION SCREEN ═══════════════════ */
static void draw_map_select(float t){
    for(int y=0;y<WIN_H;y++) for(int x=0;x<WIN_W;x++){
        float fy=(float)y/WIN_H,fx=(float)x/WIN_W;
        pixels[y][x]=rgb((int)(5+fx*6),(int)(5+fy*8),(int)(12+fy*15));
    }
    for(int y=0;y<WIN_H;y+=3) for(int x=0;x<WIN_W;x++) pixels[y][x]=darken(pixels[y][x],0.65f);

    draw_str(WIN_W/2-str_w("SELECT MAP",3)/2,30,"SELECT MAP",rgb(200,180,50),3);
    draw_line(80,70,WIN_W-80,70,rgb(60,55,30));

    /* Map cards */
    int card_w=200,card_h=260,gap=20;
    int total_w=NUM_MAPS*(card_w+gap)-gap;
    int start_x=(WIN_W-total_w)/2;

    /* map theme colors */
    u32 map_col[NUM_MAPS]={rgb(140,40,20),rgb(200,160,40),rgb(60,100,150),rgb(200,60,10),rgb(80,160,200),rgb(210,170,60)};
    const char* map_icons[NUM_MAPS]={"SKULL","ANKH","FORT","FIRE","ICE","CS"};

    for(int i=0;i<NUM_MAPS;i++){
        int cx=start_x+i*(card_w+gap);
        int cy=100+(map_sel_idx==i?-10:0);
        int sel=(i==map_sel_idx);

        /* card background */
        fill_rect(cx,cy,card_w,card_h,sel?rgb(20,18,12):rgb(12,12,18));
        outline_rect(cx,cy,card_w,card_h,sel?map_col[i]:rgb(35,35,50));
        if(sel){outline_rect(cx+2,cy+2,card_w-4,card_h-4,darken(map_col[i],0.5f));}

        /* icon area */
        u32 icon_bg=darken(map_col[i],0.3f);
        fill_rect(cx+8,cy+8,card_w-16,90,icon_bg);
        /* draw icon text */
        int iw=str_w(map_icons[i],2);
        float glow2=sel?(0.8f+sinf(t*3.f)*0.2f):0.6f;
        draw_str(cx+(card_w-iw)/2,cy+38,map_icons[i],darken(map_col[i],glow2),2);

        /* map number */
        draw_strf(cx+8,cy+106,rgb(100,100,130),1,"MAP %d",i+1);
        /* map name */
        draw_str(cx+(card_w-str_w(MAP_NAMES[i],1))/2,cy+120,MAP_NAMES[i],
                 sel?map_col[i]:rgb(150,140,120),1);
        /* desc (wrap manually at ~22 chars) */
        /* just draw short version */
        draw_str(cx+8,cy+140,MAP_DESC[i],rgb(90,88,80),1);

        /* difficulty dots */
        draw_str(cx+8,cy+190,"DIFFICULTY:",rgb(90,90,110),1);
        for(int d=0;d<5;d++){
            u32 dc=(d<=i)?map_col[i]:rgb(30,30,40);
            fill_rect(cx+8+d*18,cy+204,12,10,dc);
            outline_rect(cx+8+d*18,cy+204,12,10,rgb(50,50,70));
        }

        /* LOCKED indicator for maps > 0 if no kills */
        /* (all unlocked in this version) */
        if(sel){
            int plen=(int)(180+sinf(t*4.f)*30);
            draw_str(cx+(card_w-str_w("PRESS ENTER",1))/2,cy+230,"PRESS ENTER",rgb(plen,plen,50),1);
        }
    }

    draw_str(WIN_W/2-180,WIN_H-50,"A/D OR LEFT/RIGHT: Navigate   ENTER: Select   ESC: Back",
             rgb(70,70,100),1);
}

/* ═══════════════════ SETTINGS SCREEN ═══════════════════ */
static void draw_settings(float t){
    (void)t;
    for(int y=0;y<WIN_H;y++) for(int x=0;x<WIN_W;x++){
        pixels[y][x]=rgb(5,5,10);
    }
    int pw=600,ph=440,px=(WIN_W-pw)/2,py=(WIN_H-ph)/2;
    fill_rect(px,py,pw,ph,rgb(10,10,18));
    outline_rect(px,py,pw,ph,rgb(60,60,100));

    draw_str(px+(pw-str_w("SETTINGS",3))/2,py+14,"SETTINGS",rgb(200,180,50),3);
    draw_line(px+20,py+46,px+pw-20,py+46,rgb(50,50,80));

    const char* labels[]={"MOUSE SENSITIVITY","SFX VOLUME","BGM VOLUME","CROSSHAIR","FOV","FULLSCREEN"};
    int n_settings=6;
    for(int i=0;i<n_settings;i++){
        int sy2=py+58+i*58;
        int sel=(i==settings.settings_sel);
        fill_rect(px+20,sy2,pw-40,50,sel?rgb(18,18,35):rgb(10,10,22));
        outline_rect(px+20,sy2,pw-40,50,sel?rgb(80,80,160):rgb(25,25,45));
        draw_str(px+30,sy2+8,labels[i],sel?rgb(220,200,60):rgb(150,140,120),1);
        /* draw value widget */
        int vx=px+pw-260,vy=sy2+10;
        switch(i){
            case 0:{ /* slider 0.5-3.0 */
                float frac=(settings.mouse_sens-0.5f)/2.5f;
                fill_rect(vx,vy+10,200,8,rgb(20,20,35));
                fill_rect(vx,vy+10,(int)(frac*200),8,rgb(80,140,220));
                fill_circle(vx+(int)(frac*200),vy+14,6,rgb(120,180,255));
                draw_strf(vx+210,vy+6,rgb(160,160,200),1,"%.1f",settings.mouse_sens); break;}
            case 1:{ float frac=settings.sfx_vol;
                fill_rect(vx,vy+10,200,8,rgb(20,20,35));
                fill_rect(vx,vy+10,(int)(frac*200),8,rgb(80,200,80));
                fill_circle(vx+(int)(frac*200),vy+14,6,rgb(100,240,100));
                draw_strf(vx+210,vy+6,rgb(160,200,160),1,"%d%%",(int)(settings.sfx_vol*100)); break;}
            case 2:{ float frac=settings.bgm_vol;
                fill_rect(vx,vy+10,200,8,rgb(20,20,35));
                fill_rect(vx,vy+10,(int)(frac*200),8,rgb(80,180,120));
                fill_circle(vx+(int)(frac*200),vy+14,6,rgb(100,220,150));
                draw_strf(vx+210,vy+6,rgb(160,200,180),1,"%d%%",(int)(settings.bgm_vol*100)); break;}
            case 3:{ const char*cn[]={"DOT","CLASSIC","NONE"};
                for(int j=0;j<3;j++){
                    int bx2=vx+j*72;
                    fill_rect(bx2,vy+4,65,22,j==settings.crosshair?rgb(30,30,60):rgb(15,15,25));
                    outline_rect(bx2,vy+4,65,22,j==settings.crosshair?rgb(100,100,200):rgb(35,35,55));
                    draw_str(bx2+4,vy+9,cn[j],j==settings.crosshair?rgb(200,200,255):rgb(110,110,130),1);
                } break;}
            case 4:{ const char*fn[]={"NARROW","NORMAL","WIDE"};
                for(int j=0;j<3;j++){
                    int bx2=vx+j*72;
                    fill_rect(bx2,vy+4,65,22,j==settings.fov_idx?rgb(30,30,60):rgb(15,15,25));
                    outline_rect(bx2,vy+4,65,22,j==settings.fov_idx?rgb(100,100,200):rgb(35,35,55));
                    draw_str(bx2+4,vy+9,fn[j],j==settings.fov_idx?rgb(200,200,255):rgb(110,110,130),1);
                } break;}
            case 5:{ const char*on_off=settings.fullscreen?"ON":"OFF";
                fill_rect(vx,vy+4,80,22,settings.fullscreen?rgb(20,50,20):rgb(30,15,15));
                outline_rect(vx,vy+4,80,22,settings.fullscreen?rgb(40,140,40):rgb(100,40,40));
                draw_str(vx+10,vy+9,on_off,settings.fullscreen?rgb(60,220,60):rgb(220,80,80),1);
                break;}
        }
    }
    draw_str(px+20,py+ph-28,"UP/DN: Select   LEFT/RIGHT: Change   ESC: Back",rgb(60,60,90),1);
}

/* ═══════════════════ PAUSE SCREEN ═══════════════════ */
static int pause_sel=0;
static void draw_pause(float t){
    for(int y=0;y<WIN_H;y++) for(int x=0;x<WIN_W;x++) pixels[y][x]=darken(pixels[y][x],0.45f);
    int pw=400,ph=260,px=(WIN_W-pw)/2,py=(WIN_H-ph)/2;
    fill_rect(px,py,pw,ph,rgb(8,8,14));
    outline_rect(px,py,pw,ph,rgb(70,70,110));
    draw_str(px+(pw-str_w("PAUSED",3))/2,py+16,"PAUSED",rgb(200,180,60),3);
    draw_line(px+20,py+48,px+pw-20,py+48,rgb(50,50,80));
    const char* opts[]={"RESUME","SETTINGS","QUIT TO MENU"};
    for(int i=0;i<3;i++){
        int oy=py+62+i*52;
        int sel=(i==pause_sel);
        fill_rect(px+20,oy,pw-40,44,sel?rgb(22,20,12):rgb(12,12,20));
        outline_rect(px+20,oy,pw-40,44,sel?rgb(180,150,40):rgb(35,35,55));
        int ox=px+(pw-str_w(opts[i],2))/2;
        draw_str(ox,oy+13,opts[i],sel?rgb(240,210,60):rgb(150,140,110),2);
    }
    (void)t;
}

/* ═══════════════════ DEATH SCREEN ═══════════════════ */
static void draw_death(void){
    for(int y=0;y<WIN_H;y++) for(int x=0;x<WIN_W;x++){
        float fy=(float)y/WIN_H;
        pixels[y][x]=blend_col(darken(pixels[y][x],0.3f),rgb((int)(fy*160),(int)(fy*5),(int)(fy*5)),0.7f);
    }
    float t=SDL_GetTicks()*0.001f;
    for(int d=-3;d<=3;d++) draw_str(WIN_W/2-278+d,WIN_H/2-90+d,"YOU DIED",rgb(60,5,5),8);
    draw_str(WIN_W/2-278,WIN_H/2-90,"YOU DIED",rgb(210,25,25),8);
    draw_strf(WIN_W/2-100,WIN_H/2+50,rgb(190,170,140),2,"KILLS: %d",player.kills);
    draw_strf(WIN_W/2-105,WIN_H/2+80,rgb(220,180,0),2,"COINS: %d",player.coins);
    int pulse=(int)(150+sinf(t*3.f)*60);
    draw_str(WIN_W/2-148,WIN_H/2+120,"CLICK TO RESPAWN",rgb(pulse,pulse,90),2);
}

/* ═══════════════════ MAIN ═══════════════════ */
int main(void){
    srand((unsigned)time(NULL));
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO)<0){
        fprintf(stderr,"SDL: %s\n",SDL_GetError());return 1;
    }
    SDL_Window *win=SDL_CreateWindow("DOOMCRAFT – ULTIMATE EDITION",
        SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,WIN_W,WIN_H,SDL_WINDOW_SHOWN);
    if(!win){fprintf(stderr,"Window: %s\n",SDL_GetError());return 1;}
    SDL_Renderer *ren=SDL_CreateRenderer(win,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
    if(!ren){fprintf(stderr,"Renderer: %s\n",SDL_GetError());return 1;}
    SDL_Texture *scrtex=SDL_CreateTexture(ren,SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,WIN_W,WIN_H);

    aud_mtx=SDL_CreateMutex();
    SDL_AudioSpec want={0},got;
    want.freq=AUDIO_FREQ;want.format=AUDIO_S16SYS;want.channels=1;
    want.samples=AUDIO_BUF;want.callback=audio_cb;
    aud=SDL_OpenAudioDevice(NULL,0,&want,&got,0);
    if(aud)SDL_PauseAudioDevice(aud,0);

    gen_textures();
    load_map(0);

    /* Reset player persistent state */
    player.kills=0; player.coins=0; player.cur_weapon=0;
    for(int i=0;i<NUM_WEAPONS;i++) weapons[i].owned=(i==0)?1:0;

    const uint8_t*ks=SDL_GetKeyboardState(NULL);
    uint32_t prev=SDL_GetTicks();
    int running=1;

    while(running){
        SDL_Event ev;
        while(SDL_PollEvent(&ev)){
            switch(ev.type){
                case SDL_QUIT:running=0;break;
                case SDL_MOUSEMOTION:
                    if(cur_screen==SCREEN_GAME&&!game_over&&!shop_open)
                        player.angle+=ev.motion.xrel*(MOUSE_SENS*settings.mouse_sens);
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    if(cur_screen==SCREEN_GAME&&!game_over&&!shop_open&&ev.button.button==SDL_BUTTON_LEFT)
                        player_shoot();
                    if(cur_screen==SCREEN_GAME&&game_over){
                        int sc=player.coins,sk=player.kills,cw=player.cur_weapon;
                        for(int i=0;i<NUM_WEAPONS;i++){int o=weapons[i].owned;weapons[i].owned=o;}
                        load_map(cur_map);player.coins=sc;player.kills=sk;player.cur_weapon=cw;
                        player.clip=weapons[cw].ammo_per_clip;player.ammo=weapons[cw].ammo_per_clip*2;
                    }
                    break;
                case SDL_KEYDOWN: {
                    SDL_Keycode k=ev.key.keysym.sym;
                    /* ── INTRO ── */
                    if(cur_screen==SCREEN_INTRO){
                        if(k==SDLK_UP||k==SDLK_w){intro_sel=(intro_sel-1+2)%2;snd_ui();}
                        else if(k==SDLK_DOWN||k==SDLK_s){intro_sel=(intro_sel+1)%2;snd_ui();}
                        else if(k==SDLK_RETURN||k==SDLK_KP_ENTER){
                            snd_buy();
                            if(intro_sel==0){cur_screen=SCREEN_MAPSEL;}
                            else{cur_screen=SCREEN_SETTINGS;}
                        }
                        else if(k==SDLK_SPACE){cur_screen=SCREEN_MAPSEL;snd_buy();}
                        break;
                    }
                    /* ── MAP SELECT ── */
                    if(cur_screen==SCREEN_MAPSEL){
                        if(k==SDLK_LEFT||k==SDLK_a){map_sel_idx=(map_sel_idx-1+NUM_MAPS)%NUM_MAPS;snd_ui();}
                        else if(k==SDLK_RIGHT||k==SDLK_d){map_sel_idx=(map_sel_idx+1)%NUM_MAPS;snd_ui();}
                        else if(k==SDLK_RETURN||k==SDLK_KP_ENTER){
                            snd_buy();
                            load_map(map_sel_idx);
                            SDL_SetRelativeMouseMode(SDL_TRUE);
                            cur_screen=SCREEN_GAME;
                        }
                        else if(k==SDLK_ESCAPE){cur_screen=SCREEN_INTRO;snd_ui();}
                        break;
                    }
                    /* ── SETTINGS ── */
                    if(cur_screen==SCREEN_SETTINGS){
                        if(k==SDLK_UP||k==SDLK_w){settings.settings_sel=(settings.settings_sel-1+6)%6;snd_ui();}
                        else if(k==SDLK_DOWN||k==SDLK_s){settings.settings_sel=(settings.settings_sel+1)%6;snd_ui();}
                        else if(k==SDLK_LEFT||k==SDLK_a){
                            snd_ui();
                            switch(settings.settings_sel){
                                case 0:settings.mouse_sens=clampf(settings.mouse_sens-0.1f,0.5f,3.0f);break;
                                case 1:settings.sfx_vol=clampf(settings.sfx_vol-0.1f,0,1);break;
                                case 2:settings.bgm_vol=clampf(settings.bgm_vol-0.1f,0,1);break;
                                case 3:settings.crosshair=(settings.crosshair-1+3)%3;break;
                                case 4:settings.fov_idx=(settings.fov_idx-1+3)%3;break;
                                case 5:settings.fullscreen=0;SDL_SetWindowFullscreen(win,0);break;
                            }
                        }
                        else if(k==SDLK_RIGHT||k==SDLK_d){
                            snd_ui();
                            switch(settings.settings_sel){
                                case 0:settings.mouse_sens=clampf(settings.mouse_sens+0.1f,0.5f,3.0f);break;
                                case 1:settings.sfx_vol=clampf(settings.sfx_vol+0.1f,0,1);break;
                                case 2:settings.bgm_vol=clampf(settings.bgm_vol+0.1f,0,1);break;
                                case 3:settings.crosshair=(settings.crosshair+1)%3;break;
                                case 4:settings.fov_idx=(settings.fov_idx+1)%3;break;
                                case 5:settings.fullscreen=1;
                                       SDL_SetWindowFullscreen(win,SDL_WINDOW_FULLSCREEN_DESKTOP);break;
                            }
                        }
                        else if(k==SDLK_ESCAPE){snd_ui();
                            /* Return to whatever was before */
                            cur_screen=SCREEN_INTRO;
                        }
                        break;
                    }
                    /* ── PAUSE ── */
                    if(cur_screen==SCREEN_PAUSE){
                        if(k==SDLK_UP||k==SDLK_w){pause_sel=(pause_sel-1+3)%3;snd_ui();}
                        else if(k==SDLK_DOWN||k==SDLK_s){pause_sel=(pause_sel+1)%3;snd_ui();}
                        else if(k==SDLK_RETURN||k==SDLK_KP_ENTER){
                            snd_buy();
                            if(pause_sel==0){cur_screen=SCREEN_GAME;SDL_SetRelativeMouseMode(SDL_TRUE);}
                            else if(pause_sel==1){cur_screen=SCREEN_SETTINGS;}
                            else{cur_screen=SCREEN_INTRO;SDL_SetRelativeMouseMode(SDL_FALSE);}
                        }
                        else if(k==SDLK_ESCAPE){cur_screen=SCREEN_GAME;SDL_SetRelativeMouseMode(SDL_TRUE);}
                        break;
                    }
                    /* ── GAME ── */
                    if(cur_screen==SCREEN_GAME){
                        if(game_over){
                            if(k==SDLK_RETURN||k==SDLK_SPACE){
                                int sc=player.coins,sk=player.kills,cw=player.cur_weapon;
                                load_map(cur_map);player.coins=sc;player.kills=sk;player.cur_weapon=cw;
                                player.clip=weapons[cw].ammo_per_clip;player.ammo=weapons[cw].ammo_per_clip*2;
                            }
                            break;
                        }
                        switch(k){
                            case SDLK_ESCAPE:
                                if(shop_open){shop_open=0;SDL_SetRelativeMouseMode(SDL_TRUE);}
                                else{cur_screen=SCREEN_PAUSE;pause_sel=0;SDL_SetRelativeMouseMode(SDL_FALSE);}
                                break;
                            case SDLK_TAB:
                                shop_open=!shop_open;
                                SDL_SetRelativeMouseMode(shop_open?SDL_FALSE:SDL_TRUE);
                                break;
                            case SDLK_RETURN:case SDLK_KP_ENTER:
                                if(shop_open)shop_action(); break;
                            case SDLK_UP:if(shop_open)shop_sel=(shop_sel-1+NUM_WEAPONS)%NUM_WEAPONS;break;
                            case SDLK_DOWN:if(shop_open)shop_sel=(shop_sel+1)%NUM_WEAPONS;break;
                            case SDLK_r:if(!shop_open){Weapon*cw2=&weapons[player.cur_weapon];
                                int need=cw2->ammo_per_clip-player.clip,take=need<player.ammo?need:player.ammo;
                                player.clip+=take;player.ammo-=take;}break;
                            case SDLK_m:if(!shop_open)minimap_on=!minimap_on;break;
                            case SDLK_f:
                                settings.fullscreen=!settings.fullscreen;
                                SDL_SetWindowFullscreen(win,settings.fullscreen?SDL_WINDOW_FULLSCREEN_DESKTOP:0);
                                break;
                            case SDLK_SPACE:if(!shop_open)player_shoot();break;
                            case SDLK_1:case SDLK_2:case SDLK_3:case SDLK_4:case SDLK_5:case SDLK_6:{
                                int wi=k-SDLK_1;
                                if(wi<NUM_WEAPONS&&weapons[wi].owned){player.cur_weapon=wi;
                                    player.clip=weapons[wi].ammo_per_clip<player.ammo?weapons[wi].ammo_per_clip:player.ammo;}
                                break;}
                            default:break;
                        }
                    }
                    break;}
            }
        }

        uint32_t now=SDL_GetTicks();
        float dt=(float)(now-prev)/1000.f;
        if(dt>0.05f)dt=0.05f;
        prev=now;
        float global_t=now*0.001f;

        /* Game update */
        if(cur_screen==SCREEN_GAME&&!game_over&&!shop_open){
            float ca=cosf(player.angle),sa=sinf(player.angle);
            float spd=MOVE_SPEED*dt;
            float run=ks[SDL_SCANCODE_LSHIFT]?SPRINT_MULT:1.f;
            if(player.crouching)run*=0.55f;
            float nx=player.x,ny=player.y;
            if(ks[SDL_SCANCODE_W]){nx+=ca*spd*run;ny+=sa*spd*run;player.cur_speed=MOVE_SPEED*run;}
            if(ks[SDL_SCANCODE_S]){nx-=ca*spd*0.7f;ny-=sa*spd*0.7f;player.cur_speed=MOVE_SPEED*0.7f;}
            if(ks[SDL_SCANCODE_A]){nx+=sa*spd*0.85f;ny-=ca*spd*0.85f;player.cur_speed=MOVE_SPEED*0.85f;}
            if(ks[SDL_SCANCODE_D]){nx-=sa*spd*0.85f;ny+=ca*spd*0.85f;player.cur_speed=MOVE_SPEED*0.85f;}
            player.crouching=ks[SDL_SCANCODE_LCTRL]?1:0;
            float m=0.28f;
            if(!is_wall(cur_map,nx+m,player.y+m)&&!is_wall(cur_map,nx+m,player.y-m)&&
               !is_wall(cur_map,nx-m,player.y+m)&&!is_wall(cur_map,nx-m,player.y-m))player.x=nx;
            if(!is_wall(cur_map,player.x+m,ny+m)&&!is_wall(cur_map,player.x+m,ny-m)&&
               !is_wall(cur_map,player.x-m,ny+m)&&!is_wall(cur_map,player.x-m,ny-m))player.y=ny;
            update(dt);
        }

        /* Render */
        memset(pixels,0,sizeof pixels);

        if(cur_screen==SCREEN_INTRO){
            draw_intro(global_t);
            /* overlay selection indicator */
            for(int i=0;i<2;i++){
                const char* opts2[]={"START GAME","SETTINGS"};
                int ox=WIN_W/2-str_w(opts2[i],2)/2;
                int oy=WIN_H/2+20+i*38;
                if(intro_sel==i){
                    fill_rect(ox-14,oy-4,str_w(opts2[i],2)+28,22,rgb(25,20,12));
                    outline_rect(ox-14,oy-4,str_w(opts2[i],2)+28,22,rgb(180,130,30));
                    draw_str(ox,oy,opts2[i],rgb(240,200,60),2);
                    draw_str(ox-10,oy,">",rgb(240,180,30),2);
                } else {
                    draw_str(ox,oy,opts2[i],rgb(140,120,90),2);
                }
            }
        } else if(cur_screen==SCREEN_MAPSEL){
            draw_map_select(global_t);
        } else if(cur_screen==SCREEN_SETTINGS){
            draw_settings(global_t);
        } else if(cur_screen==SCREEN_PAUSE){
            render_world();render_sprites();render_vignette();render_weapon();render_hud();
            if(minimap_on)render_minimap();
            draw_pause(global_t);
        } else { /* SCREEN_GAME */
            render_world();render_sprites();render_vignette();render_flash();render_hurt();
            render_weapon();render_hud();if(minimap_on)render_minimap();
            if(shop_open)render_shop();
            if(game_over){render_world();render_sprites();render_vignette();render_weapon();render_hud();
                if(minimap_on)render_minimap();draw_death();}
        }

        SDL_UpdateTexture(scrtex,NULL,pixels,WIN_W*4);
        SDL_RenderCopy(ren,scrtex,NULL,NULL);
        SDL_RenderPresent(ren);
    }

    if(aud)SDL_CloseAudioDevice(aud);
    SDL_DestroyMutex(aud_mtx);
    SDL_DestroyTexture(scrtex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}