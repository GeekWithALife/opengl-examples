#ifndef __KUHL_FONT_H__
#define __KUHL_FONT_H__

#include <GL/glew.h>
#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/freeglut.h>
#endif

#include "kuhl-util.h"

// Freetype
#include <ft2build.h>
#include FT_FREETYPE_H

// Setup/Teardown
int kuhl_font_init(GLuint prog);
void kuhl_font_release();
int kuhl_font_load(char* fileName, FT_Face* face);
int kuhl_font_set(FT_Face* face, unsigned int pointSize);
void kuhl_font_set_pixel_size(int pxPerPoint);
void kuhl_font_draw(const char *text, float x, float y, int width, int height);

#endif