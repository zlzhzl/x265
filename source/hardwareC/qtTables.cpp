#include "qt.h"


/************************************************************************/
/*                    IDCT/DST coeffs                                    */
/************************************************************************/
int idst_4x4_coeff[4][4] = 
{ 
	{ 29, 74, 84, 55 },
	{ 55, 74, -29, -84},
	{ 74, 0, -74, 74 },
	{ 84, -74, 55, -29},
};

int idct_4x4_coeff[4][4] = 
{ 
	{ 64, 83, 64, 36 },
	{ 64, 36, -64, -83 },
	{ 64, -36, -64, 83 },
	{ 64, -83, 64, -36 },
}; 

int idct_8x8_coeff[8][8] = 
{
	{ 64, 89, 83, 75, 64, 50, 36, 18},	//1
	{ 64, 75, 36,-18,-64,-89,-83,-50},	//2
	{ 64, 50,-36,-89,-64, 18, 83, 75},	//3
	{ 64, 18,-83,-50, 64, 75,-36,-89},	//4
	{ 64,-18,-83, 50, 64,-75,-36, 89},	//5	
	{ 64,-50,-36, 89,-64,-18, 83,-75},	//6
	{ 64,-75, 36, 18,-64, 89,-83, 50},	//7
	{ 64,-89, 83,-75, 64,-50, 36,-18},	//8	
};

int idct_8x8_coeff_even[4][4] = 
{
	{ 64, 83, 64, 36},	//1
	{ 64, 36,-64,-83},	//2
	{ 64,-36,-64, 83},	//3
	{ 64,-83, 64,-36},	//4
};

int idct_8x8_coeff_odd[4][4] = 
{
	{  89, 75, 50, 18},	//1
	{  75,-18,-89,-50},	//2
	{  50,-89, 18, 75},	//3
	{  18,-50, 75,-89},	//4
};

int idct_16x16_coeff[16][16] = 
{
	{64,  90,  89,  87,  83,  80,  75,  70,  64,  57,  50,  43,  36,  25,  18,   9},
	{64,  87,  75,  57,  36,   9, -18, -43, -64, -80, -89, -90, -83, -70, -50, -25},
	{64,  80,  50,   9, -36, -70, -89, -87, -64, -25,  18,  57,  83,  90,  75,  43},
	{64,  70,  18, -43, -83, -87, -50,   9,  64,  90,  75,  25, -36, -80, -89, -57},
	{64,  57, -18, -80, -83, -25,  50,  90,  64,  -9, -75, -87, -36,  43,  89,  70},
	{64,  43, -50, -90, -36,  57,  89,  25, -64, -87, -18,  70,  83,   9, -75, -80},
	{64,  25, -75, -70,  36,  90,  18, -80, -64,  43,  89,   9, -83, -57,  50,  87},
	{64,   9, -89, -25,  83,  43, -75, -57,  64,  70, -50, -80,  36,  87, -18, -90},
	{64,  -9, -89,  25,  83, -43, -75,  57,  64, -70, -50,  80,  36, -87, -18,  90},
	{64, -25, -75,  70,  36, -90,  18,  80, -64, -43,  89,  -9, -83,  57,  50, -87},
	{64, -43, -50,  90, -36, -57,  89, -25, -64,  87, -18, -70,  83,  -9, -75,  80},
	{64, -57, -18,  80, -83,  25,  50, -90,  64,   9, -75,  87, -36, -43,  89, -70},
	{64, -70,  18,  43, -83,  87, -50,  -9,  64, -90,  75, -25, -36,  80, -89,  57},
	{64, -80,  50,  -9, -36,  70, -89,  87, -64,  25,  18, -57,  83, -90,  75, -43},
	{64, -87,  75, -57,  36,  -9, -18,  43, -64,  80, -89,  90, -83,  70, -50,  25},
	{64, -90,  89, -87,  83, -80,  75, -70,  64, -57,  50, -43,  36, -25,  18,  -9},
};

int idct_16x16_coeff_even[8][8] = 
{
	{64,  89,  83,  75,  64,  50,  36,  18},
	{64,  75,  36, -18, -64, -89, -83, -50},
	{64,  50, -36, -89, -64,  18,  83,  75},
	{64,  18, -83, -50,  64,  75, -36, -89},
	{64, -18, -83,  50,  64, -75, -36,  89},
	{64, -50, -36,  89, -64, -18,  83, -75},
	{64, -75,  36,  18, -64,  89, -83,  50},
	{64, -89,  83, -75,  64, -50,  36, -18},
};

int idct_16x16_coeff_even_part0[8][4] = 
{
	{64,  89,  83,  75},
	{64,  75,  36, -18},
	{64,  50, -36, -89},
	{64,  18, -83, -50},
	{64, -18, -83,  50},
	{64, -50, -36,  89},
	{64, -75,  36,  18},
	{64, -89,  83, -75},
};

int idct_16x16_coeff_even_part1[8][4] = 
{
	{ 64,  50,  36,  18},
	{-64, -89, -83, -50},
	{-64,  18,  83,  75},
	{ 64,  75, -36, -89},
	{ 64, -75, -36,  89},
	{-64, -18,  83, -75},
	{-64,  89, -83,  50},
	{ 64, -50,  36, -18},
};

int idct_16x16_coeff_odd[8][8] = 
{
	{90,  87,  80,  70,  57,  43,  25,   9},
	{87,  57,   9, -43, -80, -90, -70, -25},
	{80,   9, -70, -87, -25,  57,  90,  43},
	{70, -43, -87,   9,  90,  25, -80, -57},
	{57, -80, -25,  90,  -9, -87,  43,  70},
	{43, -90,  57,  25, -87,  70,   9, -80},
	{25, -70,  90, -80,  43,   9, -57,  87},
	{ 9, -25,  43, -57,  70, -80,  87, -90},
};

int idct_16x16_coeff_odd_part0[8][4] = 
{
	{90,  87,  80,  70},
	{87,  57,   9, -43},
	{80,   9, -70, -87},
	{70, -43, -87,   9},
	{57, -80, -25,  90},
	{43, -90,  57,  25},
	{25, -70,  90, -80},
	{ 9, -25,  43, -57},
};

int idct_16x16_coeff_odd_part1[8][4] = 
{
	{ 57,  43,  25,   9},
	{-80, -90, -70, -25},
	{-25,  57,  90,  43},
	{ 90,  25, -80, -57},
	{ -9, -87,  43,  70},
	{-87,  70,   9, -80},
	{ 43,   9, -57,  87},
	{ 70, -80,  87, -90},
};

int idct_32x32_coeff[32][32] = 
{
{  64,  90,  90,  90,  89,  88,  87,  85,  83,  82,  80,  78,  75,  73,  70,  67,  64,  61,  57,  54,  50,  46,  43,  38,  36,  31,  25,  22,  18,  13,   9,   4},
{  64,  90,  87,  82,  75,  67,  57,  46,  36,  22,   9,  -4, -18, -31, -43, -54, -64, -73, -80, -85, -89, -90, -90, -88, -83, -78, -70, -61, -50, -38, -25, -13},
{  64,  88,  80,  67,  50,  31,   9, -13, -36, -54, -70, -82, -89, -90, -87, -78, -64, -46, -25,  -4,  18,  38,  57,  73,  83,  90,  90,  85,  75,  61,  43,  22},
{  64,  85,  70,  46,  18, -13, -43, -67, -83, -90, -87, -73, -50, -22,   9,  38,  64,  82,  90,  88,  75,  54,  25,  -4, -36, -61, -80, -90, -89, -78, -57, -31},
{  64,  82,  57,  22, -18, -54, -80, -90, -83, -61, -25,  13,  50,  78,  90,  85,  64,  31,  -9, -46, -75, -90, -87, -67, -36,   4,  43,  73,  89,  88,  70,  38},
{  64,  78,  43,  -4, -50, -82, -90, -73, -36,  13,  57,  85,  89,  67,  25, -22, -64, -88, -87, -61, -18,  31,  70,  90,  83,  54,   9, -38, -75, -90, -80, -46},
{  64,  73,  25, -31, -75, -90, -70, -22,  36,  78,  90,  67,  18, -38, -80, -90, -64, -13,  43,  82,  89,  61,   9, -46, -83, -88, -57,  -4,  50,  85,  87,  54},
{  64,  67,   9, -54, -89, -78, -25,  38,  83,  85,  43, -22, -75, -90, -57,   4,  64,  90,  70,  13, -50, -88, -80, -31,  36,  82,  87,  46, -18, -73, -90, -61},
{  64,  61,  -9, -73, -89, -46,  25,  82,  83,  31, -43, -88, -75, -13,  57,  90,  64,  -4, -70, -90, -50,  22,  80,  85,  36, -38, -87, -78, -18,  54,  90,  67},
{  64,  54, -25, -85, -75,  -4,  70,  88,  36, -46, -90, -61,  18,  82,  80,  13, -64, -90, -43,  38,  89,  67,  -9, -78, -83, -22,  57,  90,  50, -31, -87, -73},
{  64,  46, -43, -90, -50,  38,  90,  54, -36, -90, -57,  31,  89,  61, -25, -88, -64,  22,  87,  67, -18, -85, -70,  13,  83,  73,  -9, -82, -75,   4,  80,  78},
{  64,  38, -57, -88, -18,  73,  80,  -4, -83, -67,  25,  90,  50, -46, -90, -31,  64,  85,   9, -78, -75,  13,  87,  61, -36, -90, -43,  54,  89,  22, -70, -82},
{  64,  31, -70, -78,  18,  90,  43, -61, -83,   4,  87,  54, -50, -88,  -9,  82,  64, -38, -90, -22,  75,  73, -25, -90, -36,  67,  80, -13, -89, -46,  57,  85},
{  64,  22, -80, -61,  50,  85,  -9, -90, -36,  73,  70, -38, -89,  -4,  87,  46, -64, -78,  25,  90,  18, -82, -57,  54,  83, -13, -90, -31,  75,  67, -43, -88},
{  64,  13, -87, -38,  75,  61, -57, -78,  36,  88,  -9, -90, -18,  85,  43, -73, -64,  54,  80, -31, -89,   4,  90,  22, -83, -46,  70,  67, -50, -82,  25,  90},
{  64,   4, -90, -13,  89,  22, -87, -31,  83,  38, -80, -46,  75,  54, -70, -61,  64,  67, -57, -73,  50,  78, -43, -82,  36,  85, -25, -88,  18,  90,  -9, -90},
{  64,  -4, -90,  13,  89, -22, -87,  31,  83, -38, -80,  46,  75, -54, -70,  61,  64, -67, -57,  73,  50, -78, -43,  82,  36, -85, -25,  88,  18, -90,  -9,  90},
{  64, -13, -87,  38,  75, -61, -57,  78,  36, -88,  -9,  90, -18, -85,  43,  73, -64, -54,  80,  31, -89,  -4,  90, -22, -83,  46,  70, -67, -50,  82,  25, -90},
{  64, -22, -80,  61,  50, -85,  -9,  90, -36, -73,  70,  38, -89,   4,  87, -46, -64,  78,  25, -90,  18,  82, -57, -54,  83,  13, -90,  31,  75, -67, -43,  88},
{  64, -31, -70,  78,  18, -90,  43,  61, -83,  -4,  87, -54, -50,  88,  -9, -82,  64,  38, -90,  22,  75, -73, -25,  90, -36, -67,  80,  13, -89,  46,  57, -85},
{  64, -38, -57,  88, -18, -73,  80,   4, -83,  67,  25, -90,  50,  46, -90,  31,  64, -85,   9,  78, -75, -13,  87, -61, -36,  90, -43, -54,  89, -22, -70,  82},
{  64, -46, -43,  90, -50, -38,  90, -54, -36,  90, -57, -31,  89, -61, -25,  88, -64, -22,  87, -67, -18,  85, -70, -13,  83, -73,  -9,  82, -75,  -4,  80, -78},
{  64, -54, -25,  85, -75,   4,  70, -88,  36,  46, -90,  61,  18, -82,  80, -13, -64,  90, -43, -38,  89, -67,  -9,  78, -83,  22,  57, -90,  50,  31, -87,  73},
{  64, -61,  -9,  73, -89,  46,  25, -82,  83, -31, -43,  88, -75,  13,  57, -90,  64,   4, -70,  90, -50, -22,  80, -85,  36,  38, -87,  78, -18, -54,  90, -67},
{  64, -67,   9,  54, -89,  78, -25, -38,  83, -85,  43,  22, -75,  90, -57,  -4,  64, -90,  70, -13, -50,  88, -80,  31,  36, -82,  87, -46, -18,  73, -90,  61},
{  64, -73,  25,  31, -75,  90, -70,  22,  36, -78,  90, -67,  18,  38, -80,  90, -64,  13,  43, -82,  89, -61,   9,  46, -83,  88, -57,   4,  50, -85,  87, -54},
{  64, -78,  43,   4, -50,  82, -90,  73, -36, -13,  57, -85,  89, -67,  25,  22, -64,  88, -87,  61, -18, -31,  70, -90,  83, -54,   9,  38, -75,  90, -80,  46},
{  64, -82,  57, -22, -18,  54, -80,  90, -83,  61, -25, -13,  50, -78,  90, -85,  64, -31,  -9,  46, -75,  90, -87,  67, -36,  -4,  43, -73,  89, -88,  70, -38},
{  64, -85,  70, -46,  18,  13, -43,  67, -83,  90, -87,  73, -50,  22,   9, -38,  64, -82,  90, -88,  75, -54,  25,   4, -36,  61, -80,  90, -89,  78, -57,  31},
{  64, -88,  80, -67,  50, -31,   9,  13, -36,  54, -70,  82, -89,  90, -87,  78, -64,  46, -25,   4,  18, -38,  57, -73,  83, -90,  90, -85,  75, -61,  43, -22},
{  64, -90,  87, -82,  75, -67,  57, -46,  36, -22,   9,   4, -18,  31, -43,  54, -64,  73, -80,  85, -89,  90, -90,  88, -83,  78, -70,  61, -50,  38, -25,  13},
{  64, -90,  90, -90,  89, -88,  87, -85,  83, -82,  80, -78,  75, -73,  70, -67,  64, -61,  57, -54,  50, -46,  43, -38,  36, -31,  25, -22,  18, -13,   9,  -4},
};

int idct_32x32_coeff_even[16][16] = 
{
	{  64,  90,  89,  87,  83,  80,  75,  70,  64,  57,  50,  43,  36,  25,  18,   9},
	{  64,  87,  75,  57,  36,   9, -18, -43, -64, -80, -89, -90, -83, -70, -50, -25},
	{  64,  80,  50,   9, -36, -70, -89, -87, -64, -25,  18,  57,  83,  90,  75,  43},
	{  64,  70,  18, -43, -83, -87, -50,   9,  64,  90,  75,  25, -36, -80, -89, -57},
	{  64,  57, -18, -80, -83, -25,  50,  90,  64,  -9, -75, -87, -36,  43,  89,  70},
	{  64,  43, -50, -90, -36,  57,  89,  25, -64, -87, -18,  70,  83,   9, -75, -80},
	{  64,  25, -75, -70,  36,  90,  18, -80, -64,  43,  89,   9, -83, -57,  50,  87},
	{  64,   9, -89, -25,  83,  43, -75, -57,  64,  70, -50, -80,  36,  87, -18, -90},
	{  64,  -9, -89,  25,  83, -43, -75,  57,  64, -70, -50,  80,  36, -87, -18,  90},
	{  64, -25, -75,  70,  36, -90,  18,  80, -64, -43,  89,  -9, -83,  57,  50, -87},
	{  64, -43, -50,  90, -36, -57,  89, -25, -64,  87, -18, -70,  83,  -9, -75,  80},
	{  64, -57, -18,  80, -83,  25,  50, -90,  64,   9, -75,  87, -36, -43,  89, -70},
	{  64, -70,  18,  43, -83,  87, -50,  -9,  64, -90,  75, -25, -36,  80, -89,  57},
	{  64, -80,  50,  -9, -36,  70, -89,  87, -64,  25,  18, -57,  83, -90,  75, -43},
	{  64, -87,  75, -57,  36,  -9, -18,  43, -64,  80, -89,  90, -83,  70, -50,  25},
	{  64, -90,  89, -87,  83, -80,  75, -70,  64, -57,  50, -43,  36, -25,  18,  -9},
};


int idct_32x32_coeff_even_part0[16][4] = 
{
	{  64,  90,  89,  87},
	{  64,  87,  75,  57},
	{  64,  80,  50,   9},
	{  64,  70,  18, -43},
	{  64,  57, -18, -80},
	{  64,  43, -50, -90},
	{  64,  25, -75, -70},
	{  64,   9, -89, -25},
	{  64,  -9, -89,  25},
	{  64, -25, -75,  70},
	{  64, -43, -50,  90},
	{  64, -57, -18,  80},
	{  64, -70,  18,  43},
	{  64, -80,  50,  -9},
	{  64, -87,  75, -57},
	{  64, -90,  89, -87},
};

int idct_32x32_coeff_even_part1[16][4] = 
{
	{ 83,  80,  75,  70},
	{ 36,   9, -18, -43},
	{-36, -70, -89, -87},
	{-83, -87, -50,   9},
	{-83, -25,  50,  90},
	{-36,  57,  89,  25},
	{ 36,  90,  18, -80},
	{ 83,  43, -75, -57},
	{ 83, -43, -75,  57},
	{ 36, -90,  18,  80},
	{-36, -57,  89, -25},
	{-83,  25,  50, -90},
	{-83,  87, -50,  -9},
	{-36,  70, -89,  87},
	{ 36,  -9, -18,  43},
	{ 83, -80,  75, -70},
};

int idct_32x32_coeff_even_part2[16][4] = 
{
	{ 64,  57,  50,  43,},
	{-64, -80, -89, -90,},
	{-64, -25,  18,  57,},
	{ 64,  90,  75,  25,},
	{ 64,  -9, -75, -87,},
	{-64, -87, -18,  70,},
	{-64,  43,  89,   9,},
	{ 64,  70, -50, -80,},
	{ 64, -70, -50,  80,},
	{-64, -43,  89,  -9,},
	{-64,  87, -18, -70,},
	{ 64,   9, -75,  87,},
	{ 64, -90,  75, -25,},
	{-64,  25,  18, -57,},
	{-64,  80, -89,  90,},
	{ 64, -57,  50, -43,},
};

int idct_32x32_coeff_even_part3[16][4] = 
{
	{ 36,  25,  18,   9},
	{-83, -70, -50, -25},
	{ 83,  90,  75,  43},
	{-36, -80, -89, -57},
	{-36,  43,  89,  70},
	{ 83,   9, -75, -80},
	{-83, -57,  50,  87},
	{ 36,  87, -18, -90},
	{ 36, -87, -18,  90},
	{-83,  57,  50, -87},
	{ 83,  -9, -75,  80},
	{-36, -43,  89, -70},
	{-36,  80, -89,  57},
	{ 83, -90,  75, -43},
	{-83,  70, -50,  25},
	{ 36, -25,  18,  -9},
};

int idct_32x32_coeff_odd[16][16] = 
{
	{  90,  90,  88,  85,  82,  78,  73,  67,  61,  54,  46,  38,  31,  22,  13,   4},
	{  90,  82,  67,  46,  22,  -4, -31, -54, -73, -85, -90, -88, -78, -61, -38, -13},
	{  88,  67,  31, -13, -54, -82, -90, -78, -46,  -4,  38,  73,  90,  85,  61,  22},
	{  85,  46, -13, -67, -90, -73, -22,  38,  82,  88,  54,  -4, -61, -90, -78, -31},
	{  82,  22, -54, -90, -61,  13,  78,  85,  31, -46, -90, -67,   4,  73,  88,  38},
	{  78,  -4, -82, -73,  13,  85,  67, -22, -88, -61,  31,  90,  54, -38, -90, -46},
	{  73, -31, -90, -22,  78,  67, -38, -90, -13,  82,  61, -46, -88,  -4,  85,  54},
	{  67, -54, -78,  38,  85, -22, -90,   4,  90,  13, -88, -31,  82,  46, -73, -61},
	{  61, -73, -46,  82,  31, -88, -13,  90,  -4, -90,  22,  85, -38, -78,  54,  67},
	{  54, -85,  -4,  88, -46, -61,  82,  13, -90,  38,  67, -78, -22,  90, -31, -73},
	{  46, -90,  38,  54, -90,  31,  61, -88,  22,  67, -85,  13,  73, -82,   4,  78},
	{  38, -88,  73,  -4, -67,  90, -46, -31,  85, -78,  13,  61, -90,  54,  22, -82},
	{  31, -78,  90, -61,   4,  54, -88,  82, -38, -22,  73, -90,  67, -13, -46,  85},
	{  22, -61,  85, -90,  73, -38,  -4,  46, -78,  90, -82,  54, -13, -31,  67, -88},
	{  13, -38,  61, -78,  88, -90,  85, -73,  54, -31,   4,  22, -46,  67, -82,  90},
	{   4, -13,  22, -31,  38, -46,  54, -61,  67, -73,  78, -82,  85, -88,  90, -90},
};

int idct_32x32_coeff_odd_part0[16][4] = 
{
	{  90,  90,  88,  85},
	{  90,  82,  67,  46},
	{  88,  67,  31, -13},
	{  85,  46, -13, -67},
	{  82,  22, -54, -90},
	{  78,  -4, -82, -73},
	{  73, -31, -90, -22},
	{  67, -54, -78,  38},
	{  61, -73, -46,  82},
	{  54, -85,  -4,  88},
	{  46, -90,  38,  54},
	{  38, -88,  73,  -4},
	{  31, -78,  90, -61},
	{  22, -61,  85, -90},
	{  13, -38,  61, -78},
	{   4, -13,  22, -31},
};

int idct_32x32_coeff_odd_part1[16][4] = 
{
	{ 82,  78,  73,  67},
	{ 22,  -4, -31, -54},
	{-54, -82, -90, -78},
	{-90, -73, -22,  38},
	{-61,  13,  78,  85},
	{ 13,  85,  67, -22},
	{ 78,  67, -38, -90},
	{ 85, -22, -90,   4},
	{ 31, -88, -13,  90},
	{-46, -61,  82,  13},
	{-90,  31,  61, -88},
	{-67,  90, -46, -31},
	{  4,  54, -88,  82},
	{ 73, -38,  -4,  46},
	{ 88, -90,  85, -73},
	{ 38, -46,  54, -61},
};

int idct_32x32_coeff_odd_part2[16][4] = 
{
	{ 61,  54,  46,  38},
	{-73, -85, -90, -88},
	{-46,  -4,  38,  73},
	{ 82,  88,  54,  -4},
	{ 31, -46, -90, -67},
	{-88, -61,  31,  90},
	{-13,  82,  61, -46},
	{ 90,  13, -88, -31},
	{ -4, -90,  22,  85},
	{-90,  38,  67, -78},
	{ 22,  67, -85,  13},
	{ 85, -78,  13,  61},
	{-38, -22,  73, -90},
	{-78,  90, -82,  54},
	{ 54, -31,   4,  22},
	{ 67, -73,  78, -82},
};

int idct_32x32_coeff_odd_part3[16][4] = 
{
	{ 31,  22,  13,   4},
	{-78, -61, -38, -13},
	{ 90,  85,  61,  22},
	{-61, -90, -78, -31},
	{  4,  73,  88,  38},
	{ 54, -38, -90, -46},
	{-88,  -4,  85,  54},
	{ 82,  46, -73, -61},
	{-38, -78,  54,  67},
	{-22,  90, -31, -73},
	{ 73, -82,   4,  78},
	{-90,  54,  22, -82},
	{ 67, -13, -46,  85},
	{-13, -31,  67, -88},
	{-46,  67, -82,  90},
	{ 85, -88,  90, -90},
};

/************************************************************************/
/*                     DCT/DST coeeffs                                  */
/************************************************************************/

int dct_4x4_coeff[4][4] =
{
	{ 64, 64, 64, 64 },
	{ 83, 36, -36, -83 },
	{ 64, -64, -64, 64 },
	{ 36, -83, 83, -36 }
};

int dst_4x4_coeff[4][4] = 
{
	{ 29, 55, 74, 84 },
	{ 74, 74, 0, -74 },
	{ 84, -29, -74, 55 },
	{ 55, -84, 74, -29 }
};

int dct_8x8_coeff[8][8] =
{
	{ 64, 64, 64, 64, 64, 64, 64, 64 },
	{ 89, 75, 50, 18, -18, -50, -75, -89 },
	{ 83, 36, -36, -83, -83, -36, 36, 83 },
	{ 75, -18, -89, -50, 50, 89, 18, -75 },
	{ 64, -64, -64, 64, 64, -64, -64, 64 },
	{ 50, -89, 18, 75, -75, -18, 89, -50 },
	{ 36, -83, 83, -36, -36, 83, -83, 36 },
	{ 18, -50, 75, -89, 89, -75, 50, -18 }
};

// not used actually
int dct_16x16_coeff[16][16] =
{
	{ 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64 },
	{ 90, 87, 80, 70, 57, 43, 25, 9, -9, -25, -43, -57, -70, -80, -87, -90 },
	{ 89, 75, 50, 18, -18, -50, -75, -89, -89, -75, -50, -18, 18, 50, 75, 89 },
	{ 87, 57, 9, -43, -80, -90, -70, -25, 25, 70, 90, 80, 43, -9, -57, -87 },
	{ 83, 36, -36, -83, -83, -36, 36, 83, 83, 36, -36, -83, -83, -36, 36, 83 },
	{ 80, 9, -70, -87, -25, 57, 90, 43, -43, -90, -57, 25, 87, 70, -9, -80 },
	{ 75, -18, -89, -50, 50, 89, 18, -75, -75, 18, 89, 50, -50, -89, -18, 75 },
	{ 70, -43, -87, 9, 90, 25, -80, -57, 57, 80, -25, -90, -9, 87, 43, -70 },
	{ 64, -64, -64, 64, 64, -64, -64, 64, 64, -64, -64, 64, 64, -64, -64, 64 },
	{ 57, -80, -25, 90, -9, -87, 43, 70, -70, -43, 87, 9, -90, 25, 80, -57 },
	{ 50, -89, 18, 75, -75, -18, 89, -50, -50, 89, -18, -75, 75, 18, -89, 50 },
	{ 43, -90, 57, 25, -87, 70, 9, -80, 80, -9, -70, 87, -25, -57, 90, -43 },
	{ 36, -83, 83, -36, -36, 83, -83, 36, 36, -83, 83, -36, -36, 83, -83, 36 },
	{ 25, -70, 90, -80, 43, 9, -57, 87, -87, 57, -9, -43, 80, -90, 70, -25 },
	{ 18, -50, 75, -89, 89, -75, 50, -18, -18, 50, -75, 89, -89, 75, -50, 18 },
	{ 9, -25, 43, -57, 70, -80, 87, -90, 90, -87, 80, -70, 57, -43, 25, -9 }
};


int dct_16x16_coeff_even[8][4] = 
{
	// in rows: 0, 4, 8, 12,  the coefficients in the cols 2, 3 are dependent
	// so, actually 16+8=24 coeffs are independent
	{ 64, 64, 64, 64 },	  // row[0], + +
	{ 89, 75, 50, 18 },   // row[2], 
	{ 83, 36, -36, -83 }, // row[4], - -
	{ 75, -18, -89, -50 },// row[6],
	{ 64, -64, -64, 64 }, // row[8], + +
	{ 50, -89, 18, 75 },  // row[10]	
	{ 36, -83, 83, -36 }, // row[12],- -
	{ 18, -50, 75, -89 }  // row[14]
};

int dct_16x16_coeff_odd[8][8] =
{
	{ 90, 87, 80, 70, 57, 43, 25, 9 },
	{ 87, 57, 9, -43, -80, -90, -70, -25 },
	{ 80, 9, -70, -87, -25, 57, 90, 43 },
	{ 70, -43, -87, 9, 90, 25, -80, -57 },
	{ 57, -80, -25, 90, -9, -87, 43, 70 },
	{ 43, -90, 57, 25, -87, 70, 9, -80 },
	{ 25, -70, 90, -80, 43, 9, -57, 87 },
	{ 9, -25, 43, -57, 70, -80, 87, -90 }
};

int dct_32x32_coeff[32][32] =
{
	{ 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64 },
	{ 90, 90, 88, 85, 82, 78, 73, 67, 61, 54, 46, 38, 31, 22, 13, 4, -4, -13, -22, -31, -38, -46, -54, -61, -67, -73, -78, -82, -85, -88, -90, -90 },
	{ 90, 87, 80, 70, 57, 43, 25, 9, -9, -25, -43, -57, -70, -80, -87, -90, -90, -87, -80, -70, -57, -43, -25, -9, 9, 25, 43, 57, 70, 80, 87, 90 },
	{ 90, 82, 67, 46, 22, -4, -31, -54, -73, -85, -90, -88, -78, -61, -38, -13, 13, 38, 61, 78, 88, 90, 85, 73, 54, 31, 4, -22, -46, -67, -82, -90 },
	{ 89, 75, 50, 18, -18, -50, -75, -89, -89, -75, -50, -18, 18, 50, 75, 89, 89, 75, 50, 18, -18, -50, -75, -89, -89, -75, -50, -18, 18, 50, 75, 89 },
	{ 88, 67, 31, -13, -54, -82, -90, -78, -46, -4, 38, 73, 90, 85, 61, 22, -22, -61, -85, -90, -73, -38, 4, 46, 78, 90, 82, 54, 13, -31, -67, -88 },
	{ 87, 57, 9, -43, -80, -90, -70, -25, 25, 70, 90, 80, 43, -9, -57, -87, -87, -57, -9, 43, 80, 90, 70, 25, -25, -70, -90, -80, -43, 9, 57, 87 },
	{ 85, 46, -13, -67, -90, -73, -22, 38, 82, 88, 54, -4, -61, -90, -78, -31, 31, 78, 90, 61, 4, -54, -88, -82, -38, 22, 73, 90, 67, 13, -46, -85 },
	{ 83, 36, -36, -83, -83, -36, 36, 83, 83, 36, -36, -83, -83, -36, 36, 83, 83, 36, -36, -83, -83, -36, 36, 83, 83, 36, -36, -83, -83, -36, 36, 83 },
	{ 82, 22, -54, -90, -61, 13, 78, 85, 31, -46, -90, -67, 4, 73, 88, 38, -38, -88, -73, -4, 67, 90, 46, -31, -85, -78, -13, 61, 90, 54, -22, -82 },
	{ 80, 9, -70, -87, -25, 57, 90, 43, -43, -90, -57, 25, 87, 70, -9, -80, -80, -9, 70, 87, 25, -57, -90, -43, 43, 90, 57, -25, -87, -70, 9, 80 },
	{ 78, -4, -82, -73, 13, 85, 67, -22, -88, -61, 31, 90, 54, -38, -90, -46, 46, 90, 38, -54, -90, -31, 61, 88, 22, -67, -85, -13, 73, 82, 4, -78 },
	{ 75, -18, -89, -50, 50, 89, 18, -75, -75, 18, 89, 50, -50, -89, -18, 75, 75, -18, -89, -50, 50, 89, 18, -75, -75, 18, 89, 50, -50, -89, -18, 75 },
	{ 73, -31, -90, -22, 78, 67, -38, -90, -13, 82, 61, -46, -88, -4, 85, 54, -54, -85, 4, 88, 46, -61, -82, 13, 90, 38, -67, -78, 22, 90, 31, -73 },
	{ 70, -43, -87, 9, 90, 25, -80, -57, 57, 80, -25, -90, -9, 87, 43, -70, -70, 43, 87, -9, -90, -25, 80, 57, -57, -80, 25, 90, 9, -87, -43, 70 },
	{ 67, -54, -78, 38, 85, -22, -90, 4, 90, 13, -88, -31, 82, 46, -73, -61, 61, 73, -46, -82, 31, 88, -13, -90, -4, 90, 22, -85, -38, 78, 54, -67 },
	{ 64, -64, -64, 64, 64, -64, -64, 64, 64, -64, -64, 64, 64, -64, -64, 64, 64, -64, -64, 64, 64, -64, -64, 64, 64, -64, -64, 64, 64, -64, -64, 64 },
	{ 61, -73, -46, 82, 31, -88, -13, 90, -4, -90, 22, 85, -38, -78, 54, 67, -67, -54, 78, 38, -85, -22, 90, 4, -90, 13, 88, -31, -82, 46, 73, -61 },
	{ 57, -80, -25, 90, -9, -87, 43, 70, -70, -43, 87, 9, -90, 25, 80, -57, -57, 80, 25, -90, 9, 87, -43, -70, 70, 43, -87, -9, 90, -25, -80, 57 },
	{ 54, -85, -4, 88, -46, -61, 82, 13, -90, 38, 67, -78, -22, 90, -31, -73, 73, 31, -90, 22, 78, -67, -38, 90, -13, -82, 61, 46, -88, 4, 85, -54 },
	{ 50, -89, 18, 75, -75, -18, 89, -50, -50, 89, -18, -75, 75, 18, -89, 50, 50, -89, 18, 75, -75, -18, 89, -50, -50, 89, -18, -75, 75, 18, -89, 50 },
	{ 46, -90, 38, 54, -90, 31, 61, -88, 22, 67, -85, 13, 73, -82, 4, 78, -78, -4, 82, -73, -13, 85, -67, -22, 88, -61, -31, 90, -54, -38, 90, -46 },
	{ 43, -90, 57, 25, -87, 70, 9, -80, 80, -9, -70, 87, -25, -57, 90, -43, -43, 90, -57, -25, 87, -70, -9, 80, -80, 9, 70, -87, 25, 57, -90, 43 },
	{ 38, -88, 73, -4, -67, 90, -46, -31, 85, -78, 13, 61, -90, 54, 22, -82, 82, -22, -54, 90, -61, -13, 78, -85, 31, 46, -90, 67, 4, -73, 88, -38 },
	{ 36, -83, 83, -36, -36, 83, -83, 36, 36, -83, 83, -36, -36, 83, -83, 36, 36, -83, 83, -36, -36, 83, -83, 36, 36, -83, 83, -36, -36, 83, -83, 36 },
	{ 31, -78, 90, -61, 4, 54, -88, 82, -38, -22, 73, -90, 67, -13, -46, 85, -85, 46, 13, -67, 90, -73, 22, 38, -82, 88, -54, -4, 61, -90, 78, -31 },
	{ 25, -70, 90, -80, 43, 9, -57, 87, -87, 57, -9, -43, 80, -90, 70, -25, -25, 70, -90, 80, -43, -9, 57, -87, 87, -57, 9, 43, -80, 90, -70, 25 },
	{ 22, -61, 85, -90, 73, -38, -4, 46, -78, 90, -82, 54, -13, -31, 67, -88, 88, -67, 31, 13, -54, 82, -90, 78, -46, 4, 38, -73, 90, -85, 61, -22 },
	{ 18, -50, 75, -89, 89, -75, 50, -18, -18, 50, -75, 89, -89, 75, -50, 18, 18, -50, 75, -89, 89, -75, 50, -18, -18, 50, -75, 89, -89, 75, -50, 18 },
	{ 13, -38, 61, -78, 88, -90, 85, -73, 54, -31, 4, 22, -46, 67, -82, 90, -90, 82, -67, 46, -22, -4, 31, -54, 73, -85, 90, -88, 78, -61, 38, -13 },
	{ 9, -25, 43, -57, 70, -80, 87, -90, 90, -87, 80, -70, 57, -43, 25, -9, -9, 25, -43, 57, -70, 80, -87, 90, -90, 87, -80, 70, -57, 43, -25, 9 },
	{ 4, -13, 22, -31, 38, -46, 54, -61, 67, -73, 78, -82, 85, -88, 90, -90, 90, -90, 88, -85, 82, -78, 73, -67, 61, -54, 46, -38, 31, -22, 13, -4 }
};

int dct_32x32_coeff_even[16][8]=
{
	{ 64, 64, 64, 64, 64, 64, 64, 64 },
	{ 90, 87, 80, 70, 57, 43, 25, 9 },
	{ 89, 75, 50, 18, -18, -50, -75, -89 },
	{ 87, 57, 9, -43, -80, -90, -70, -25 },
	{ 83, 36, -36, -83, -83, -36, 36, 83 },
	{ 80, 9, -70, -87, -25, 57, 90, 43 },
	{ 75, -18, -89, -50, 50, 89, 18, -75 },
	{ 70, -43, -87, 9, 90, 25, -80, -57 },
	{ 64, -64, -64, 64, 64, -64, -64, 64 },
	{ 57, -80, -25, 90, -9, -87, 43, 70 },
	{ 50, -89, 18, 75, -75, -18, 89, -50 },
	{ 43, -90, 57, 25, -87, 70, 9, -80 },
	{ 36, -83, 83, -36, -36, 83, -83, 36 },
	{ 25, -70, 90, -80, 43, 9, -57, 87 },
	{ 18, -50, 75, -89, 89, -75, 50, -18 },
	{ 9, -25, 43, -57, 70, -80, 87, -90 }
};

int dct_32x32_coeff_odd[16][16]=
{
	{ 90, 90, 88, 85, 82, 78, 73, 67, 61, 54, 46, 38, 31, 22, 13, 4 },
	{ 90, 82, 67, 46, 22, -4, -31, -54, -73, -85, -90, -88, -78, -61, -38, -13 },
	{ 88, 67, 31, -13, -54, -82, -90, -78, -46, -4, 38, 73, 90, 85, 61, 22 },
	{ 85, 46, -13, -67, -90, -73, -22, 38, 82, 88, 54, -4, -61, -90, -78, -31 },
	{ 82, 22, -54, -90, -61, 13, 78, 85, 31, -46, -90, -67, 4, 73, 88, 38 },
	{ 78, -4, -82, -73, 13, 85, 67, -22, -88, -61, 31, 90, 54, -38, -90, -46 },
	{ 73, -31, -90, -22, 78, 67, -38, -90, -13, 82, 61, -46, -88, -4, 85, 54 },
	{ 67, -54, -78, 38, 85, -22, -90, 4, 90, 13, -88, -31, 82, 46, -73, -61},
	{ 61, -73, -46, 82, 31, -88, -13, 90, -4, -90, 22, 85, -38, -78, 54, 67 },
	{ 54, -85, -4, 88, -46, -61, 82, 13, -90, 38, 67, -78, -22, 90, -31, -73 },
	{ 46, -90, 38, 54, -90, 31, 61, -88, 22, 67, -85, 13, 73, -82, 4, 78 },
	{ 38, -88, 73, -4, -67, 90, -46, -31, 85, -78, 13, 61, -90, 54, 22, -82 },
	{ 31, -78, 90, -61, 4, 54, -88, 82, -38, -22, 73, -90, 67, -13, -46, 85 },
	{ 22, -61, 85, -90, 73, -38, -4, 46, -78, 90, -82, 54, -13, -31, 67, -88 },
	{ 13, -38, 61, -78, 88, -90, 85, -73, 54, -31, 4, 22, -46, 67, -82, 90 },
	{ 4, -13, 22, -31, 38, -46, 54, -61, 67, -73, 78, -82, 85, -88, 90, -90 }
};


//===================================================================================
//============================ SWC version ==========================================
//===================================================================================
/************************************************************************/
/*                    IDCT/DST coeffs                                    */
/************************************************************************/
int t4_coeff[4][4] =
{
    { 64, 64, 64, 64 },
    { 83, 36, -36, -83 },
    { 64, -64, -64, 64 },
    { 36, -83, 83, -36 }
};

