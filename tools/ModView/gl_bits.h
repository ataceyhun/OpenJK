// Filename:-	gl_bits.h
//

#ifndef GL_BITS_H
#define GL_BITS_H

#include <string>

//HGLRC	GL_GenerateRC	(HDC hDC, bool bDoubleBuffer = true);
//GLuint	GL_BindImage	(CImage *image);
void	GL_Enter3D( double dFOV, int iWindowWidth, int iWindowDepth, bool bWireFrame, bool bCLS = true );
void	GL_Enter2D		(int iWindowWidth, int iWindowDepth, bool bCLS = true);
void	GL_Exit2D		(void);
int		GL_GetCorrectedDim		(int iDim);
char*	GL_GetCorrectedDimString(int iDim);
std::string GL_GetInfo();
void    GL_CacheDriverInfo();


#ifdef _DEBUG
#define ASSERT_GL AssertGL(__FILE__,__LINE__)
void	AssertGL(const char *sFile, int iLine);
#else
#define ASSERT_GL
#endif

typedef struct tagFRECT
{
    float	left;
    float    top;
    float    right;
    float    bottom;
} FRECT, *LPFRECT;



#endif	// #ifndef GL_BITS_H


/////////////////// eof //////////////////

