#include "FullScreenPassShaders.h"

IMPLEMENT_GLOBAL_SHADER(FFullScreenPassVS, "/FullScreenPass/Private/FullScreenShader.usf", "MainVS", SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(FSharpenPassVS, "/FullScreenPass/Private/SharpenShader.usf", "SharpenVS", SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(FFullScreenPassPS, "/FullScreenPass/Private/FullScreenShader.usf", "MainPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FSharpenPassPS, "/FullScreenPass/Private/SharpenShader.usf", "SharpenPS", SF_Pixel);