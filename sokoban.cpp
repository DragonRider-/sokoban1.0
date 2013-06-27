// win32: g++ socoban.cpp -Wall -pedantic -lSDL -lopengl32 -lglu32 -lSDL_image
// linux: g++ socoban.cpp -Wall _Wextra -pedantic -lSDL -lGL -lGLU -lSDL_image

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdint.h>

#include <vector>

#include <GL/gl.h>
#include <GL/glu.h>

// Sometimes this needs to be changed to just <SDL.h> and <SDL_image.h>
// When in doubt, use sdl-config to find out
#include <SDL/SDL.h>
#include <SDL/SDL_image.h>

#ifdef _WIN32
#  include <windows.h>
#endif

using namespace std;

// Remove SDL main hook on Windows
#ifdef _WIN32
#  undef main
#endif

// Macros.
#define UNUSED(a) ((void)(a))

// ZMIENNE GLOBALNE
static int screen_width;
static int screen_height;
bool keys[SDLK_LAST];
float ratio;

bool game_over = false;
unsigned int level_id = 0;

unsigned int textures[32];
#define TEX_WALL        0
#define TEX_FLOOR       1
#define TEX_FLOOR_X     2
#define TEX_STONE       3
#define TEX_STONE_HI    4
#define TEX_PLAYER      5
#define TEX_SPLASH      6
#define TEX_LOGO        7
#define TEX_GAMEOVER    8

int Map[16][16];
//  statyczne elementy na mapie
#define MAP_EMPTY       -1
#define MAP_WALL        TEX_WALL
#define MAP_FLOOR       TEX_FLOOR
#define MAP_FLOOR_X     TEX_FLOOR_X

struct player_poz
{
    int x, y;
    //  skończone
} player_stone;

struct stone_poz
{
    int x,y;
    bool hi; // true - kamień jest w dobrym miejscu

    static struct stone_poz mk(int x, int y)
    {
        stone_poz s = {x, y, false };
        return s;
    }
};

vector<stone_poz> stone_map;

bool tytul();
bool LoadMap(const char *filename);

// Functions.
static bool InitSDL(bool fullscreen = false, int width = 640, int height = 480);
static bool InitOpenGL();

unsigned int ImgToTexture(const char *filename);
void DrawQuad(float x, float y, float w, float h);
void DrawQuadRGBA(float x, float y, float w, float h, float r, float g, float b, float a);
void DrawQuadTexture(float x, float y, float w, float h, unsigned int texture_id);

static bool Events();
static void Logic();
static void Scene();

// ------------------------------------------------------------------
// EVENTS
// ------------------------------------------------------------------

// Events
static bool
Events() {
  SDL_Event ev;
  memset(&ev, 0, sizeof(ev));

  while(SDL_PollEvent(&ev)) {
    switch(ev.type) {
      case SDL_KEYUP:
    keys[ev.key.keysym.sym] = false;
    break;

      case SDL_KEYDOWN:
        if(ev.key.keysym.sym == SDLK_ESCAPE)
          return false;
    keys[ev.key.keysym.sym] = true;
    break;

      case SDL_QUIT:
        return false;
    }
  }

  return true;
}


// ------------------------------------------------------------------
// LOGIC
// ------------------------------------------------------------------

//  warunek kończenia levelu
static bool LevelComplete()
{
    size_t s = stone_map.size();
    for(size_t i = 0; i < s; i++)
    {
        if(!stone_map[i].hi)
            return false;
    }
    return true;
}

//  przechodzenie do następnego lvl
static bool NextLevel()
{
    level_id++;

    char file_name[256] = {0};
    snprintf(file_name, sizeof(file_name) -1, "mapy/lvl_%u.txt", level_id);
    stone_map.clear();

    return LoadMap(file_name);
}

//  łapanie ID kamienia po koordynatach XY
int GetStoneByXY(int x, int y)
{
    size_t s = stone_map.size();
    for(size_t i = 0; i < s; i++)
    {
        if(stone_map[i].x == x &&
           stone_map[i].y == y)
            return i;
    }
    return -1;
}

// Logic
static void
Logic() {
    int p_dx = 0;   //  delta poruszania playera
    int p_dy = 0;

    if      (keys[SDLK_UP])     { p_dy = -1; keys[SDLK_UP]     = false; }
    else if (keys[SDLK_DOWN])   { p_dy = 1;  keys[SDLK_DOWN]     = false; }
    else if (keys[SDLK_LEFT])   { p_dx = -1; keys[SDLK_LEFT]    = false; }
    else if (keys[SDLK_RIGHT])  { p_dx = 1;  keys[SDLK_RIGHT]   = false; }

    int p_nx = player_stone.x + p_dx;   //  nowa pozycja gracza
    int p_ny = player_stone.y + p_dy;

    bool p_move_ok = true;

    //  kolizje (ściany, próźnia)
    if (Map[p_ny][p_nx] == MAP_WALL ||
        Map[p_ny][p_nx] == MAP_EMPTY)
    {
        p_move_ok = false;
    }
    else
    {
        //  PRZESUWANIE KAMIENI
        int s_id = GetStoneByXY(p_nx, p_ny);
        if(s_id != -1)
        {
            bool s_move_ok = true;
            int s_nx = p_nx + p_dx;
            int s_ny = p_ny + p_dy;
            if ( Map[s_ny][s_nx] == MAP_WALL ||
                 Map[s_ny][s_nx] == MAP_EMPTY)
                s_move_ok = false;
            else
            {
                if (GetStoneByXY(s_nx, s_ny) != -1)
                    s_move_ok = false;
            }

            //  jeśli nie da się przesunąć kamienia
            //  to nie da się przesunąć gracza
            if(!s_move_ok)
                p_move_ok = false;
            //  jeśli się da to wylicz nową pozycję kamienia
            else
            {
            p_move_ok = true;
            stone_map[s_id].x = s_nx;
            stone_map[s_id].y = s_ny;
            }

            //  podświetlanie kamienia...

            // niby działający kod
            //stone_map[s_id].hi = (Map[s_ny][s_nx] == MAP_FLOOR_X);

            // i mój działający :)
            if (s_move_ok)
            {
                if (Map[s_ny][s_nx] == MAP_FLOOR_X)
                    stone_map[s_id].hi = true;
                else
                    stone_map[s_id].hi = false;
            }
            else
            {
                if (Map[p_ny][p_nx] == MAP_FLOOR_X)
                    stone_map[s_id].hi = true;
            }
            if(stone_map[s_id].hi)
            {
                //  czy level ukończony ?
                if(LevelComplete())
                {
                    if(!NextLevel())
                        game_over = true;
                    else
                        return;
                }
            }
        }

    }

    //  poruszanie gracza
    if(p_move_ok)
    {
        player_stone.x = p_nx;
        player_stone.y = p_ny;
    }
}

// ------------------------------------------------------------------
// SCENE
// ------------------------------------------------------------------

// Tytuł

bool tytul()
{
    //  wyświetlanie logo mw-dev itd... etc...
    SDL_Delay(1000);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glTranslatef(0.0f, 0.0f, -15.0f);
    glScalef(1.0f, -1.0f, 1.0f);
    DrawQuadTexture(0.0f, 0.0f, 24.0f, 20.0f, textures[TEX_LOGO]);

    SDL_GL_SwapBuffers();
    SDL_Delay(4000);

    DrawQuadTexture(0.0f, 0.0f, 4.0f*5.8f, 3.0f*5.8f, textures[TEX_SPLASH]);
    SDL_GL_SwapBuffers();
    SDL_Delay(3000);
    return false;
}


// Scene
static void
Scene() {
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glTranslatef(0.0f, 0.0f, -15.0f);

  // Tło
  glScalef(1.0f, -1.0f, 1.0f);
  const float k = 5.8;
  DrawQuadTexture(0.0f, -0.0f, 4.0f*k, 3.0f*k, textures[TEX_SPLASH]);

  // Ekran końca gry
  if (game_over)
  {
      DrawQuadTexture(0.0f, 0.0f, 24.0f, 20.0f, textures[TEX_GAMEOVER]);
      return;
  }

  // Rysowanie Mapy
  glTranslatef(-7.5f, -7.5f, 0.0f);

  for(int j=0; j<16; j++)
  {
      for(int i=0; i<16; i++)
      {
          if(Map[j][i] != MAP_EMPTY)
              DrawQuadTexture(
               float(i), float(j),
               1.0f, 1.0f,
               textures[Map[j][i]]);

      }
  }

  //    WŁĄCZENIE ALPHY
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_BLEND);

  // Rysowanie Gracza
  DrawQuadTexture( (float)player_stone.x, (float)player_stone.y,
                   1.0f, 1.0f, textures[TEX_PLAYER]);

  // Rysowanie Kamieni
  size_t s = stone_map.size();
  for(size_t i = 0; i < s; i++)
  {
      if (stone_map[i].hi)
          DrawQuadTexture( (float)stone_map[i].x, (float)stone_map[i].y,
                       1.0f, 1.0f, textures[TEX_STONE_HI]);
      else
          DrawQuadTexture( (float)stone_map[i].x, (float)stone_map[i].y,
                       1.0f, 1.0f, textures[TEX_STONE]);

  }

  //    TEST        TEST        TEST
  //                ||||
  //DrawQuadTexture(0.0f, 0.0f, 2.0f, 2.0f, textures[TEX_FLOOR_X]);
  //                ^^^^

  //    WYŁĄCZENIE ALPHY
  glDisable(GL_BLEND);

}

// ------------------------------------------------------------------
// Init & utils
// ------------------------------------------------------------------

bool LoadMap(const char *filename)
{
    // -1 == 0xffffffff -> 0xff
    memset(Map, 0xff, sizeof(Map));

    FILE *f = fopen(filename, "r");
    if (!f)
    {
        fprintf(stderr, "error: nie mozna otworzyc pliku mapy \"%s\"\n", filename);
        return false;
    }

    for(int j=0; j<16; j++)
    {
        char line[64];
        fgets(line, sizeof(line), f);
        if(feof(f))
        {
            fprintf(stderr, "error: niespodziewany koniec pliku \"%s\"\n", filename);
            fclose(f);
            return false;
        }

        for(int i=0; i<16; i++)
        {
            switch (line[i])
            {
            case ' ':   Map[j][i] = MAP_EMPTY; break;
            case '#':   Map[j][i] = MAP_WALL; break;
            case '.':   Map[j][i] = MAP_FLOOR; break;
            case 'x':   Map[j][i] = MAP_FLOOR_X; break;
            case 'P':   player_stone.x = i;
                        player_stone.y = j;
                        Map[j][i] = MAP_FLOOR; break;
            case 'o':   stone_map.push_back(stone_poz::mk(i, j));
                        Map[j][i] = MAP_FLOOR; break;
            default:
                fprintf(stderr, "error: nieznany obiekt \"%c\" na mapie \"%s\"\n",
                        line[i], filename);
                fclose(f);
                return false;
            }

        }
    }

    printf("info: zaladowana mapa \"%s\"\n", filename);

    fclose(f);
    return true;
}

// main
int
main(int argc, char **argv, char **envp)
{
  // Unused.
  UNUSED(argc);
  UNUSED(argv);
  UNUSED(envp);

  // Init SDL.
  if(!InitSDL())
    return 1;

  // Init OpenGL.
  if(!InitOpenGL())
    return 2;

  // Title
  SDL_WM_SetCaption("My game", "My game");


  textures[TEX_WALL]     = ImgToTexture("grafika/murek.png");
  textures[TEX_FLOOR]    = ImgToTexture("grafika/kafelka.png");
  textures[TEX_FLOOR_X]  = ImgToTexture("grafika/kafelka2.png");
  textures[TEX_STONE]    = ImgToTexture("grafika/kamien1.png");
  textures[TEX_STONE_HI] = ImgToTexture("grafika/kamien_hi1.png");
  textures[TEX_PLAYER]   = ImgToTexture("grafika/user.png");
  textures[TEX_SPLASH]   = ImgToTexture("grafika/splash.png");
  textures[TEX_LOGO]     = ImgToTexture("grafika/logo3.png");
  textures[TEX_GAMEOVER] = ImgToTexture("grafika/gameover.png");

  NextLevel();

  //    EKRAN TYTUŁOWY

  // Main loop:
  unsigned int last_time = SDL_GetTicks();
  ratio = 0.0f;

  tytul();

  for(;;) {

    // Main stuff.
    if(!Events())
      break;

    Logic();
    Scene();

    SDL_GL_SwapBuffers();

    // Calc time.
    unsigned int curr_time = SDL_GetTicks();
    ratio = (float)(curr_time - last_time) / 1000.0f;
    last_time = curr_time;
  }

  // Done.
  SDL_Quit();
  return 0;
}

//              FUNKCJE ENGINE'A

// InitSDL
static bool
InitSDL(bool fullscreen, int width, int height)
{
  int screen_bpp;
  int screen_flag = SDL_OPENGL;
  if(fullscreen) screen_flag |= SDL_FULLSCREEN;

  screen_width = width;
  screen_height = height;

  const SDL_VideoInfo *info;
  SDL_Surface *surface;

  if(SDL_Init(SDL_INIT_VIDEO) < 0)
    return false;

  info = SDL_GetVideoInfo();
  if(info == NULL)
    goto err;

  screen_bpp = info->vfmt->BitsPerPixel;
  SDL_GL_SetAttribute(SDL_GL_RED_SIZE,   8);
  SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,  8);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

  SDL_putenv("SDL_VIDEO_CENTERED=center");

  surface = SDL_SetVideoMode(
            screen_width, screen_height,
            screen_bpp, screen_flag);

  if(surface == NULL)
    goto err;

  SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);

  return true;

err:
  SDL_Quit();
  return false;
}

// InitOpenGL
static bool
InitOpenGL()
{
  float ratio;
  ratio = (float)screen_width / (float)screen_height;

  glShadeModel(GL_SMOOTH);
  glClearColor(0, 0, 0, 0);
  glViewport(0, 0, screen_width, screen_height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity( );
  gluPerspective( 60.0, ratio, 1.0, 1024.0 );
  glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
  glDisable(GL_DEPTH_TEST); // uncomment this if going 2D
  return true;
}


// Note: older version of opengl required image width and height
// to be a power of 2 (e.g. 16x32 or 512x128 or 256x256, etc).
unsigned int
ImgToTexture(const char *filename) {
  // Load image.
  SDL_Surface *img = IMG_Load(filename);
  if(!img) {
    fprintf(stderr, "error: ImgToTexture could not load image \"%s\"\n",
            filename);
    return ~0;
  }

  // Image type? Only 24 and 32 supported, rest must be converted.
  unsigned int img_type = 0;

  if(img->format->BitsPerPixel == 32) {
    img_type = GL_RGBA;
  } else if(img->format->BitsPerPixel == 24) {
    img_type = GL_RGB;
  } else {

    // Convert to 32 bits.
    SDL_PixelFormat fmt = {
      NULL, 32, 4,
      0, 0, 0, 0,
      0, 8, 16, 24,
      0xff, 0xff00, 0xff0000, 0xff000000,
      0,
      0xff
    };

    SDL_Surface *nimg = SDL_ConvertSurface(img, &fmt, SDL_SWSURFACE);
    SDL_FreeSurface(img);

    if(!nimg) {
      fprintf(stderr, "error: ImgToTexture could not convert image \"%s\" to 32-bit\n",
              filename);
      return ~0;
    }

    // Done converting.
    img = nimg;
    img_type = GL_RGBA;
  }

  // Create texture.
  unsigned int texture_id = ~0;
  glGenTextures(1, &texture_id);
  glBindTexture(GL_TEXTURE_2D, texture_id);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  // You might want to play with these parameters.
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

  // Gen texture.
  glTexImage2D(GL_TEXTURE_2D, 0, img_type, img->w, img->h,
    0, img_type, GL_UNSIGNED_BYTE, img->pixels);

  printf("info: ImgToTexture loaded texture \"%s\" as %u\n",
      filename, texture_id);

  // Done
  SDL_FreeSurface(img);
  return texture_id;
}

void
DrawQuad(float x, float y, float w, float h)
{
  w /= 2.0f; h /= 2.0f;

  glBegin(GL_QUADS);
  glTexCoord2f(0.0f, 0.0f); glVertex2f(x - w, y - h);
  glTexCoord2f(1.0f, 0.0f); glVertex2f(x + w, y - h);
  glTexCoord2f(1.0f, 1.0f); glVertex2f(x + w, y + h);
  glTexCoord2f(0.0f, 1.0f); glVertex2f(x - w, y + h);
  glEnd();
}

void
DrawQuadRGBA(float x, float y, float w, float h, float r, float g, float b, float a)
{
  // Get old color.
  float current_color[4];
  glGetFloatv(GL_CURRENT_COLOR, current_color);

  // Set new color and draw quad.
  glColor4f(r, g, b, a);
  DrawQuad(x, y, w, h);

  // Set old color.
  glColor4fv(current_color);
}

void
DrawQuadTexture(float x, float y, float w, float h, unsigned int texture_id)
{
  // Enable texturing if needed.
  bool texturing_enabled = glIsEnabled(GL_TEXTURE_2D);
  if(!texturing_enabled)
    glEnable(GL_TEXTURE_2D);

  // Bind texture and draw.
  glBindTexture(GL_TEXTURE_2D, texture_id);
  DrawQuad(x, y, w, h);

  // Disable if was disable.
  if(!texturing_enabled)
    glDisable(GL_TEXTURE_2D);
}

