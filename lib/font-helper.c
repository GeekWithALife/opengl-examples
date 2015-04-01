#include "font-helper.h"

#define min(x, y) (x < y) ? x : y
#define max(x, y) (x > y) ? x : y

// State
static FT_Library lib;
static FT_Face curFace;
float curColor[4] = {1, 1, 1, 1};
float curColorBG[4] = {0, 0, 0, 1};
unsigned int curPointSize = 12;
unsigned int curPixelSize = 2;

// OpenGL
static GLuint program = 0;
static GLuint tex = 0;
static GLuint vbo = 0;
static GLuint vao = 0;
static GLint uniform_tex = 0;
static GLint attribute_coord = -1;
static GLint uniform_color = -1;

// Setup/Teardown
int kuhl_font_init(GLuint prog) {
	if (program > 0) {
		fprintf(stderr, "FreeType has already been initialized!\n");
		return 0;
	}
	// Init FreeType
	if (FT_Init_FreeType(&lib)) {
		fprintf(stderr, "Could not initialize the FreeType library\n");
		return 0;
	}
	
	// Make sure it's got the right variables
	uniform_tex = glGetUniformLocation(prog, "tex");
	kuhl_errorcheck();
	if (uniform_tex < 0) {
		fprintf(stderr, "Could not bind texture uniform\n");
		return 0;
	}
	uniform_color = glGetUniformLocation(prog, "color");
	kuhl_errorcheck();
	if (uniform_color < 0) {
		fprintf(stderr, "Could not bind color uniform\n");
		return 0;
	}
	attribute_coord = glGetAttribLocation(prog, "coord");
	kuhl_errorcheck();
	if (attribute_coord < 0) {
		fprintf(stderr, "Could not bind coord attribute\n");
		return 0;
	}
	
	// It's valid - set the program.
	program = prog;
	
	// Set up the shader program
	glUseProgram(program);
	kuhl_errorcheck();

	glActiveTexture(GL_TEXTURE0);
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glUniform1i(uniform_tex, 0);
	kuhl_errorcheck();
	
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	glGenVertexArrays(1, &vao);
	kuhl_errorcheck();
	glBindVertexArray(vao);
	kuhl_errorcheck();
	
	glGenBuffers(1, &vbo);
	kuhl_errorcheck();
	glEnableVertexAttribArray(attribute_coord);
	kuhl_errorcheck();
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	kuhl_errorcheck();
	glVertexAttribPointer(attribute_coord, 4, GL_FLOAT, GL_FALSE, 0, 0);
	kuhl_errorcheck();
	
	return 1;
}
void kuhl_font_release() {
	glDeleteTextures(1, &tex);
	glDeleteVertexArrays(1, &vao);
	glDeleteBuffers(1, &vbo);
	FT_Done_FreeType(lib);
}

int kuhl_font_load(char* fileName, FT_Face* face) {
	if(FT_New_Face(lib, fileName, 0, face)) {
		fprintf(stderr, "Could not open font '%s'\n", fileName);
		return 0;
	}
	FT_Set_Pixel_Sizes(*face, 0, 12);
	if(FT_Load_Char(*face, 'X', FT_LOAD_RENDER)) {
		fprintf(stderr, "Could not load character for font '%s'\n", (*face)->family_name);
		return 0;
	}
	return 1;
}
int kuhl_font_set(FT_Face* face, unsigned int pointSize) {
	if (curPointSize != pointSize) {
		FT_Set_Pixel_Sizes(*face, 0, pointSize);
		if(FT_Load_Char(*face, 'X', FT_LOAD_RENDER)) {
			fprintf(stderr, "Could not load character for font '%s'\n", (*face)->family_name);
			return 0;
		}
		curPointSize = pointSize;
	}
	curFace = *face;
	return 1;
}
void kuhl_font_set_pixel_size(int pxPerPoint) {
	curPixelSize = pxPerPoint;
}

static void render_char(const char ch, float* x, float* y, float sx, float sy, float startX, float startY) {
	FT_GlyphSlot g = curFace->glyph;
	if(FT_Load_Char(curFace, ch, FT_LOAD_RENDER))
		return;
	
	glTexImage2D(
		GL_TEXTURE_2D,
		0,
		GL_RED,
		g->bitmap.width,
		g->bitmap.rows,
		0,
		GL_RED,
		GL_UNSIGNED_BYTE,
		g->bitmap.buffer
	);
	kuhl_errorcheck();
	
	if (ch == '\n') {
		*y -= curPointSize * sy;
		*x = startX;
		return;
	} else if (ch == '\r')
		return;

	float x2 = *x + g->bitmap_left * sx;
	float y2 = -*y - g->bitmap_top * sy;
	float w = g->bitmap.width * sx;
	float h = g->bitmap.rows * sy;
	
	int advanceW = (g->advance.x >> 6);
	x2 += ((curPointSize - advanceW)/2.0) * sx;
	
	GLfloat box[4][4] = {
		{x2,     -y2    , 0, 0},
		{x2 + w, -y2    , 1, 0},
		{x2,     -y2 - h, 0, 1},
		{x2 + w, -y2 - h, 1, 1},
	};

	glBufferData(GL_ARRAY_BUFFER, sizeof box, box, GL_DYNAMIC_DRAW);
	kuhl_errorcheck();
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	kuhl_errorcheck();
	
	*x += (g->advance.x >> 6) * sx;
	*y += (g->advance.y >> 6) * sy;
}
void kuhl_font_draw(const char *text, float x, float y, int width, int height) {
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, tex);
	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glEnableVertexAttribArray(attribute_coord);
	
	y += curPointSize; // Bitmaps start at bottom-left corner.
	/*height *= curPointSize;
	
	FT_GlyphSlot g = curFace->glyph;
	if(FT_Load_Char(curFace, 'X', FT_LOAD_RENDER))
		width *= g->bitmap.width;
	else
		width *= curPointSize;
	width += x;
	height += y;*/
	float aspect = 1.0; //min((float) glutGet(GLUT_WINDOW_WIDTH) / width, (float) glutGet(GLUT_WINDOW_HEIGHT) / height);
	float sx = aspect * (float)curPixelSize / glutGet(GLUT_WINDOW_WIDTH);//
	float sy = aspect * (float)curPixelSize / glutGet(GLUT_WINDOW_HEIGHT);

	x = -1 + x * sx;
	y = 1 - y * sy;
	float startX = x, startY = y;
	const char *p;
	for(p = text; *p; p++)
		render_char(*p, &x, &y, sx, sy, startX, startY);
}
//void StopRender() {}
