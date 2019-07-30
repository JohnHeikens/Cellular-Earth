#pragma once
#include <windows.h>
#include <iostream>
#include <cmath>
#include "include.h"
//defines

//vars

//structs
enum biome { sea, beach, grassland, snow, biomecount };
enum disease { none,cold, seasickness, malaria, pest, diseasecount };
int severity[diseasecount]{ 0,1,1,3,5 };
int healability[diseasecount]{ 0,10,10,5,1 };
fp spreadability[diseasecount]{ 0,0.3,0,0.6,0.9 };
struct human
{
	int age;
	int	activehealth;//how much health points left
	int	x, y;//position
	int speedx, speedy;//speed
	int	lastx,lasty;//position before
	disease sickness;
	//genes
	int power;//how much hp per hit
	int health;//how much starting hp
	int strength;//max age
	int group;//which group
};
//functions
void ProcessInput();
bool DoEvents();
void ClearColor(color BackGroundColor);
void FillPixel(int x, int y, color color);
void FillPixelUnsafe(int x, int y, color color);
void FillTriangle3D(fp x1, fp y1, fp d1, fp x2, fp y2, fp d2, fp x3, fp y3, fp d3, fp minx, fp maxx, color FillWith);
void FillRectangle(int x, int y, int w, int h, color FillWith);
void FillCircle(fp x, fp y, fp w, fp h, color FillWith);