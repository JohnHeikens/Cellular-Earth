#include "main.h"
#include <time.h>
#include <list>

int width;//the width of the window
int height;//the height of the window

// Global Windows/Drawing variables
HBITMAP hbmp = NULL;
HWND hwnd = NULL;
HDC hdcMem = NULL;
// The window's DC
HDC wndDC = NULL;
HBITMAP hbmOld = NULL;
// Pointer to colors (will automatically have space allocated by CreateDIBSection
color* colors = nullptr;
fp* depthbuffer = nullptr;
Texture tex;

vec3 Position = vec3();
fp mouseX = 0, mouseY = 0;
POINT MousePos = POINT();
fp* heightmap;
biome* biomemap;
human** peoplemap;
const fp maxheight = 100, sealevel = maxheight * 0.6, beachlevel = maxheight * 0.65, grasslevel = maxheight * 0.8;
const fp peopleheight = 2;
const color biomecolor[biomecount]{
	color(0, 105, 148),
	color(194, 178, 128), 
	color(96, 128, 56), 
	color(255, 250, 250),
};
color* groupcolor;
color diseasecolor[diseasecount]
{
	color(0xff,0xff,0xff),
	color(0,0xff,0),
	color(0x80,0x80,0xff),
	color(0xff,0,0),
	color(0x80,0x40,0x40),
};
std::vector<human*> humans;
fp slope = -1;//the slope of the light
fp scale = 1;//zoom
const int timestep = 1;//one year a frame
void onFrame() {
	//slope = sin(time(NULL) * 0.01);
	slope += 0.001;
	//slope = -1;
	//process people
	std::list<human*> childs = std::list<human*>();
	for (human* h : humans)
	{
		h->lastx = h->x;//save last coordinates for collision
		h->lasty = h->y;
		h->speedx += rand() % 3 - 1;//-1,0,1
		h->speedy += rand() % 3 - 1;//-1,0,1
		if (h->speedx < 0)h->x--;
		else if (h->speedx > 0)h->x++;
		if (h->speedy < 0)h->y--;
		else if (h->speedy > 0)h->y++;

		//check if the way is free
		if (h->x < 0)//block borders
		{
			h->x = 0;
		}
		else if (h->x >= width) 
		{
			h->x = width - 1;
		}
		if (h->y < 0) 
		{
			h->y = 0;
		}
		else if (h->y >= height)
		{
			h->y = height - 1;
		}
		human* other = *(peoplemap + h->x + h->y * width);
		if (other)//bumps into other human
		{
			if (other->group != h->group)
			{//fight
				other->activehealth -= h->power;
				h->activehealth -= other->power;
			}
			//step back
			*(peoplemap + h->x + h->y * width) = NULL;
			h->x = h->lastx;
			h->y = h->lasty;
			other->x = other->lastx;
			other->y = other->lasty;
			*(peoplemap + other->x + other->y * width) = other;
		}
		else if (biomemap[h->x + h->y * width] == sea &&
			biomemap[h->lastx + h->lasty * width] &&
			rand() % 20 > 0)
		{
			//step back
			h->x = h->lastx;
			h->y = h->lasty;
		}
		//no one is at the place this human is
		const int pixelindex = h->x + h->y * width;
		human** ptr = peoplemap + pixelindex;
		*ptr = h;

		human* left = h->x ? *(ptr - 1) : NULL;
		human* right = h->x < width - 1 ? *(ptr + 1) : NULL;
		human* top = h->y ? *(ptr - width) : NULL;
		human* bottom = h->y < height - 1 ? *(ptr + width) : NULL;

		biome* biomeptr = biomemap + pixelindex;
		if (h->sickness) //cannot reproduce
		{
			h->activehealth -= severity[h->sickness];
			if (rand() % (h->age/healability[h->sickness] + 1) == 0)
				h->sickness = none;
			fp s = spreadability[h->sickness];
			if (s) {
				//spread
				if (left && h->sickness > left->sickness && RANDFP < s)//left has human
				{
					left->sickness = h->sickness;
				}
				if (right && h->sickness > right->sickness && RANDFP < s)//right has human
				{
					right->sickness = h->sickness;
				}
				if (top && h->sickness > top->sickness && RANDFP < s)//top has human
				{
					top->sickness = h->sickness;
				}
				if (bottom && h->sickness > bottom->sickness && RANDFP < s)//bottom has human
				{
					bottom->sickness = h->sickness;
				}
			}
		}
		else {
			if (rand() % 10 == 0)
			{//more chance of sickness
				switch (*biomeptr)
				{
				case sea:
					h->sickness = seasickness;
					break;
				case beach:
					if (rand() % 5 == 0) {
						h->sickness = malaria;
					}
					break;
				case grassland:
					if (rand() % 20 == 0)
					{
						h->sickness = pest;//1 out of 200 on grass
					}
					break;
				case snow:
					h->sickness = cold;
					break;
				default:
					break;
				}
			}
			//45 - 11 = 34 / 10 = 3 babies avg without sickness
			const fp minbabyage = 11;
			const fp maxbabyage = minbabyage + (h->strength - minbabyage) * 0.5f;
			if (h->age >= minbabyage && h->age <= maxbabyage && rand() % 10 == 0)//reproduce
			{
				human* baby = new human();
				//mutate dna
				baby->power = h->power + rand() % 3 - 1;//+1,0,-1
				baby->health = h->health + rand() % 3 - 1;//+1,0,-1
				baby->group = h->group;
				baby->strength = h->strength + rand() % 5 - 2;//+2,+1,0,-1,-2
				if (baby->power < 1)baby->power = 1;
				if (baby->health < 1)baby->health = 1;
				if (baby->strength < 1)baby->strength = 1;

				baby->age = 0;
				baby->x = h->x;
				baby->y = h->y;
				baby->speedx = 0;
				baby->speedy = 0;
				baby->sickness = none;
				baby->activehealth = baby->health;
				//seek room
				if (h->x > 0 && !left && *(biomeptr - 1))//left is free
				{
					baby->x--;
					childs.push_back(baby);
				}
				else if (h->x < width - 1 && !right && *(biomeptr + 1))//right is free
				{
					baby->x++;
					childs.push_back(baby);
				}
				else if (h->y > 0 && !top && *(biomeptr - width))//top is free
				{
					baby->y--;
					childs.push_back(baby);
				}
				else if (h->y < height - 1 && !bottom && *(biomeptr + width))//bottom is free
				{
					baby->y++;
					childs.push_back(baby);
				}
				else 
				{
					delete baby;
				}
			}
		}
		
		h->age += timestep;
	}
	humans.insert(humans.end(), childs.begin(), childs.end());
	childs.clear();
	//remove all gone
	auto oldi = &humans[0];//else same memory adresses
	auto newi = oldi;
	int newhumancount = humans.size();
	auto end = oldi + newhumancount;
	while (oldi < end)
	{
		if ((*oldi)->activehealth <= 0 || (*oldi)->age == (*oldi)->strength)//dead
		{
			//oldi = humans.erase(oldi);
			//erase position
			*(peoplemap + (*oldi)->x + (*oldi)->y * width) = NULL;
			delete* (oldi);
			newhumancount--;
		}
		else
		{
			*newi = *oldi;
			++newi;
		}
		++oldi;
	}
	humans.resize(newhumancount);
	//draw
	color* colorptr = colors;
	fp* heightptr = heightmap;
	biome* activebiome = biomemap;
	human** activehuman = peoplemap;
	for (int j = 0; j < height; j++)
	{
		fp maxdiagonalheight = 0;//the highest shadow
		for (int i = 0; i < width; i++,colorptr++, heightptr++, maxdiagonalheight += slope, activehuman++, activebiome++)
		{
			human* h = *activehuman;
			fp heighestpos = max(*heightptr, sealevel);
			if (h == NULL) //no human in this place
			{
				*colorptr = biomecolor[*activebiome];
			}
			else {
				*colorptr = groupcolor[h->group];//show group
				//*colorptr = diseasecolor[h->sickness];//show disease
				//*colorptr = color(((fp)h->age / h->strength) * 255);//show age
				heighestpos += peopleheight;
				*activehuman = NULL;
			}
			fp lightlevel = heighestpos - maxdiagonalheight + .5;
			//between 0 and 1
			if(lightlevel < 1)
			{
				if (lightlevel < 0.3)
				{
					lightlevel = 0.3;
				}
				*colorptr = *colorptr * lightlevel;
			}
			if (maxdiagonalheight < heighestpos)
			{
				maxdiagonalheight = heighestpos;
			}
		}
	}

	POINT p;
	if (GetCursorPos(&p))
	{
		//cursor position now in p.x and p.y
		if (ScreenToClient(hwnd, &p))
		{
			MousePos = p;
			//p.x and p.y are now relative to hwnd's client area
		}
	}
}
void ProcessInput()
{
	//source:
	//https://stackoverflow.com/questions/41606356/catch-ctrlalt-keys-in-wndproc
	if (GetKeyState(VK_UP) & 0x8000) //forward key is pressed
	{
		Position.y--;
	}
	if (GetKeyState(VK_DOWN) & 0x8000) //forward key is pressed
	{
		Position.y++;
	}
	if (GetKeyState(VK_LEFT) & 0x8000) //left key is pressed
	{
		Position.x--;
	}
	if (GetKeyState(VK_RIGHT) & 0x8000) //right key is pressed
	{
		Position.x++;
	}
}
void MakeSurface(HWND hwnd) {
	/* Use CreateDIBSection to make a HBITMAP which can be quickly
	 * blitted to a surface while giving 100% fast access to colors
	 * before blit.
	 */
	 // Desired bitmap properties
	BITMAPINFO bmi;
	bmi.bmiHeader.biSize = sizeof(BITMAPINFO);//sizeof(BITMAPINFO);
	bmi.bmiHeader.biWidth = width;
	bmi.bmiHeader.biHeight = -height; // Order colors from top to bottom
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32; // last byte not used, 32 bit for alignment
	bmi.bmiHeader.biCompression = BI_RGB;
	bmi.bmiHeader.biSizeImage = 0;// width* height * 4;
	bmi.bmiHeader.biXPelsPerMeter = 0;
	bmi.bmiHeader.biYPelsPerMeter = 0;
	bmi.bmiHeader.biClrUsed = 0;
	bmi.bmiHeader.biClrImportant = 0;
	bmi.bmiColors[0].rgbBlue = 0;
	bmi.bmiColors[0].rgbGreen = 0;
	bmi.bmiColors[0].rgbRed = 0;
	bmi.bmiColors[0].rgbReserved = 0;
	HDC hdc = GetDC(hwnd);
	colors = nullptr;
	// Create DIB section to always give direct access to colors
	hbmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, (void**)& colors, NULL, 0);
	
	DeleteDC(hdc);
	// Give plenty of time for main thread to finish setting up
	Sleep(50);//time??? without sleep, it finishes
	ShowWindow(hwnd, SW_SHOW);
	// Retrieve the window's DC
	wndDC = GetDC(hwnd);
	// Create DC with shared colors to variable 'colors'
	hdcMem = CreateCompatibleDC(wndDC);//HDC must be wndDC!! :)
	hbmOld = (HBITMAP)SelectObject(hdcMem, hbmp);

	depthbuffer = (fp*)calloc(width * height, sizeof(fp));
	bool b = false;
	const int octaves = 6;
	fp weights[]{ 1,0.5,0.25,0.13,0.06,0.03 };
	const bool realworld = true;
	if (realworld) 
	{
		int w, h, channels = 0;
		std::vector<char> data = readBMP("heightmap1280x640rgb.BMP", w, h, channels);
		heightmap = new fp[width * height];
		fp* heightptr = heightmap;
		fp* endptr = heightptr + width * height;
		byte* imgptr = (byte*)data.data();
		byte* jptr = imgptr + width * height * channels;
		for (int j = 0; j < h; j++) 
		{
			jptr -= width * channels;
			byte* iptr = jptr;
			for (int i = 0; i < w; i++,heightptr++,iptr+=channels)
			{
				*heightptr = map(*iptr, 0xc, 0xb0, sealevel, maxheight);
			}
		}
	}
	else {
		LayerNoiseSimplex* noise = new LayerNoiseSimplex((int)time(NULL), weights, octaves, 0, maxheight, 0.003);
		heightmap = noise->Evaluate(0, 0, width, height);
	}
	humans = std::vector<human*>();
	fp* heightptr = heightmap;
	fp* endptr = heightptr + width * height;
	const int density = 1000;
	const int groupcount = 20;
	human** activehuman = peoplemap = new human * [width * height];
	biome* activebiome = biomemap = new biome[width * height];
	while (heightptr < endptr)
	{
		if (*heightptr > beachlevel && *heightptr < grasslevel && rand() % density == 0)
		{
			int index = heightptr - heightmap;
			int health = rand() % 9 + 2;//2-10
			*activehuman = new human{ 0,health,index % width,index / width,0,0,0,0,none,
				rand() % 4 + 1,//1 to 4
				health,
				rand() % 101 + 20,//20 - 120
				rand() % groupcount };
			humans.push_back(*activehuman++);
		}
		else 
		{
			*activehuman++ = NULL;
		}
		*activebiome =
			*heightptr > sealevel ?
			*heightptr > beachlevel ?
			*heightptr > grasslevel ?
			snow :
			grassland :
			beach :
			sea;
		heightptr++;
		activebiome++;
	}
	groupcolor = new color[groupcount];
	for (int i = 0; i < groupcount; i++) 
	{
		groupcolor[i] = RANDCOLOR;
	}
}
LRESULT CALLBACK WndProc(
	HWND hwnd,
	UINT msg,
	WPARAM wParam,
	LPARAM lParam)
{
	switch (msg) {
	case WM_CREATE:
	{
		MakeSurface(hwnd);
	}
	break;
	case WM_MOUSEMOVE:
	{
		
	}
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hwnd, &ps);
		// Draw colors to window when window needs repainting
		BitBlt(hdc, 0, 0, width, height, hdcMem, 0, 0, SRCCOPY);
		EndPaint(hwnd, &ps);
	}
	break;
	case WM_CLOSE:
	{
		DestroyWindow(hwnd);
	}
	break;
	case WM_DESTROY:
	{
		SelectObject(hdcMem, hbmOld);
		DeleteDC(wndDC);
		PostQuitMessage(0);
	}
	break;
	default:
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return 0;
}
int WINAPI WinMain(
	HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPSTR lpCmdLine,
	int nShowCmd)
{
	//get global vars
	GetDesktopResolution(width, height);
	width = 1280;
	height = 640;
	Image image = Image::FromFile(std::string("up.BMP"));
	tex = Texture(image.colors, image.Width);

	WNDCLASSEX wc;
	//MSG msg;
	// Init wc
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.hbrBackground = CreateSolidBrush(0);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
	wc.hInstance = hInstance;
	wc.lpfnWndProc = WndProc;
	wc.lpszClassName = L"animation_class";
	wc.lpszMenuName = NULL;
	wc.style = 0;
	// Register wc
	if (!RegisterClassEx(&wc)) {
		MessageBox(NULL, L"Failed to register window class.", L"Error", MB_OK);
		return 0;
	}
	// Make window
	hwnd = CreateWindowEx(
		WS_EX_APPWINDOW,
		L"animation_class",
		L"Animation",
		WS_MINIMIZEBOX | WS_SYSMENU | WS_POPUP | WS_CAPTION,
		300, 200, width, height,
		NULL, NULL, hInstance, NULL);
	RECT rcClient, rcWindow;
	POINT ptDiff;
	// Get window and client sizes
	GetClientRect(hwnd, &rcClient);
	GetWindowRect(hwnd, &rcWindow);
	// Find offset between window size and client size
	ptDiff.x = (rcWindow.right - rcWindow.left) - rcClient.right;
	ptDiff.y = (rcWindow.bottom - rcWindow.top) - rcClient.bottom;
	// Resize client
	MoveWindow(hwnd, rcWindow.left, rcWindow.top, width + ptDiff.x, height + ptDiff.y, false);
	UpdateWindow(hwnd);

	while (DoEvents())
	{
		ProcessInput();//process events from user
		color c = color(rand() % 0x100, rand() % 0x100, rand() % 0x100);
		//FillRectangle(rand() % 500, rand() % 300, rand() % 500, rand() % 300, c);
		//next frame
		// Do stuff with colors
		onFrame();
		// Draw colors to window
		BitBlt(wndDC, 0, 0, width, height, hdcMem, 0, 0, SRCCOPY);
	}
	/*while (GetMessage(&msg, 0, 0, NULL) > 0)
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}*/
	return 0;
}
//process events
//return if to continue the program
bool DoEvents()
{
	MSG msg;
	BOOL result;

	while (::PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE))
	{
		result = ::GetMessage(&msg, NULL, 0, 0);
		if (result == 0) // WM_QUIT
		{
			::PostQuitMessage(msg.wParam);
			return false;
		}
		else if (result == -1)
		{
			return false;
			// Handle errors/exit application, etc.
		}
		else
		{
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
		}
	}
	return true;
}

void DrawTriangles(vec3* vertices, int* indices, color* colors, int TriangleCount, int VerticeCount, fp zoom, vec3 position, mat3x3 view, bool fill)
{
	vec3* vertices2D = nullptr;
	//vec3 direction = info.direction;
	//vec3 right = info.right;
	//Shader shader = todraw.shader;
	int W2 = width / 2, H2 = height / 2;
	fp Multiplier = max(W2, H2) * zoom;

	//https://en.wikipedia.org/wiki/Rotation_matrix
	view.MultiplyX(Multiplier);
	view.MultiplyZ(Multiplier);

	vertices2D = (vec3*)calloc(VerticeCount, sizeof(vec3));
	vec3* PointerVector = vertices2D;

	// vertices to 2D
	vec3* p2d = PointerVector;
	vec3* p3d = vertices;
	vec3* end = p3d + VerticeCount;
	while (p3d < end)
	{
		fp
			DX = p3d->x - position.x,
			DY = p3d->y - position.y,
			DZ = p3d->z - position.z;
		// Multiply With Multiplication Matrix
		fp maty = DX * view.XtoY + DY * view.YtoY + DZ * view.ZtoY;
		if (maty <= 0)
		{
			p2d->z = maty;
		}//checking wether distances are > 0 because you don't want to draw things behind
		else
		{

			fp matx = DX * view.XtoX + DY * view.YtoX + DZ * view.ZtoX;
			fp matz = DX * view.XtoZ + DY * view.YtoZ + DZ * view.ZtoZ;

			// Transformation To Screen
			p2d->x = (matx / maty) + W2;//convert to screen coordinates
			p2d->y = (-matz / maty) + H2;
			p2d->z = DX * DX + DY * DY + DZ * DZ;

		}
		p2d++;
		p3d++;
	}

	int pointcount = TriangleCount * 3;
	for (int i = 0; i < pointcount; i += 3)//iterate for each tri
	{
		// Get Values
		int
			ptr1 = *indices++,
			ptr2 = *indices++,
			ptr3 = *indices++;
		vec3* v1 = PointerVector + ptr1;
		vec3* v2 = PointerVector + ptr2;
		vec3* v3 = PointerVector + ptr3;
		color ActiveColor = *colors++;
		fp
			x1 = v1->x,//convert to screen coordinates
			x2 = v2->x,
			x3 = v3->x,//x3 = (outx3 / outy3) * Multiplier + W2,                                        x3 = (outx3 / outy3) * Multiplier + W2,
			y1 = v1->y,
			y2 = v2->y,
			y3 = v3->y,
			minx, maxx;

		fp distance1 = v1->z;
		fp distance2 = v2->z;
		fp distance3 = v3->z;
		if (distance1 <= 0 || distance2 <= 0 || distance3 <= 0) continue;
		// Calculate Triangle Properties
		if (x1 < x2)
		{
			minx = x1 < x3 ? x1 : x3;
			maxx = x2 > x3 ? x2 : x3;
		}
		else
		{
			minx = x2 < x3 ? x2 : x3;
			maxx = x1 > x3 ? x1 : x3;
		}
		if ((int)minx == (int)maxx || minx > width || maxx < 0) continue;
		//switch points so y1 <= y2 <= y3
		bool y1y2 = y1 > y2;
		bool y2y3 = y2 > y3;
		bool y1y3 = y1 > y3;
		if (y1y2)
		{
			if (y2y3)//total opposite
			{
				fp temp = y3;//321
				y3 = y1;
				y1 = temp;
				temp = x3;
				x3 = x1;
				x1 = temp;
				temp = distance3;
				distance3 = distance1;
				distance1 = temp;
			}
			else
			{
				if (y1y3)
				{//312
					fp temp = y1;
					y1 = y2;
					y2 = y3;
					y3 = temp;
					temp = x1;
					x1 = x2;
					x2 = x3;
					x3 = temp;
					temp = distance1;
					distance1 = distance2;
					distance2 = distance3;
					distance3 = temp;
				}
				else
				{

					fp temp = y2;//213
					y2 = y1;
					y1 = temp;
					temp = x2;
					x2 = x1;
					x1 = temp;
					temp = distance2;
					distance2 = distance1;
					distance1 = temp;
				}
			}
		}
		else if (y2y3)
		{
			if (y1y3)
			{//231
				fp temp = y1;
				y1 = y3;
				y3 = y2;
				y2 = temp;
				temp = x1;
				x1 = x3;
				x3 = x2;
				x2 = temp;
				temp = distance1;
				distance1 = distance3;
				distance3 = distance2;
				distance2 = temp;
			}
			else
			{//132
				fp temp = y2;
				y2 = y3;
				y3 = temp;
				temp = x2;
				x2 = x3;
				x3 = temp;
				temp = distance2;
				distance2 = distance3;
				distance3 = temp;
			}
		}
		if ((int)y1 == (int)y3 || y1 > height || y3 < 0) continue;

		//fp distance = udTriangle(v1, v2, v3, position);
		if (fill)
		{
			FillTriangle3D(x1, y1, distance1, x2, y2, distance2, x3, y3, distance3, minx, maxx, ActiveColor);
		}
		else
		{
			//Graphics.DrawTri(color, distance1, //DX1 * DX1 + DY1 * DY1 + DZ1 * DZ1,
			//	Pclr, x1, x2, x3, y1, y2, y3, distances, bounds, PixelWidth);
		}
	}

	//}
}

void ClearDepthBuffer(fp MaxDistance)
{
	//fill the arrays
	std::fill(depthbuffer, depthbuffer + width * height, MaxDistance);
	color BackgroundColor = color(0xff, 0, 0);//red
}

void ClearColor(color BackGroundColor)
{
	//fill the screen with the background color
	std::fill(colors, colors + width * height, BackGroundColor);
}

inline void FillPixel(int x, int y, color color)
{
	if (x >= 0 && y >= 0 && x < width && y < height)* (colors + x + y * width) = color;
}

inline void FillPixelUnsafe(int x, int y, color color)
{
	*(colors + x + y * width) = color;
}

//a faster, safer method
void FillCircle(fp x, fp y, fp w, fp h, color FillWith)
{
	int MinX = (int)x;
	int MinY = (int)y;
	int MaxX = (int)(x + w);
	int MaxY = (int)(y + h);
	//crop
	if (MinX < 0)MinX = 0;
	if (MinY < 0)MinY = 0;
	if (MaxX > width)MaxX = width;
	if (MaxY > height)MaxY = height;

	fp midx = x + w * .5;
	fp midy = y + h * .5;
	fp multx = 1 / (midx - x);//multipliers
	fp multy = 1 / (midy - y);
	color* ptr = colors + MinX + MinY * width;
	for (int j = MinY; j < MaxY; j++)
	{
		color* NextPtr = ptr + width;
		fp dy = (j - midy) * multy;
		fp dy2 = dy * dy;
		for (int i = MinX; i < MaxX; i++)
		{
			fp dx = (i - midx) * multx;
			if (dy2 + dx * dx < 1) 
			{
				*ptr = FillWith;
			}
			ptr++;
		}
		ptr = NextPtr;
	}
}

void FillRectangle(fp x, fp y, fp w, fp h, color FillWith)
{
	int ix = (int)x;
	int iy = (int)y;
	FillRectangle(ix, iy, (int)(x + w) - ix, (int)(y + w) - iy, FillWith);
}

void FillRectangle(int x, int y, int w, int h, color FillWith)
{
	color* ptr = colors + x + y * width;
	color* EndPtrY = ptr + h * width;
	while (ptr < EndPtrY)
	{
		color* NextY = ptr + width;
		color* EndPtrX = ptr + w;
		while (ptr < EndPtrX)
		{
			*ptr++ = FillWith;
		}

		ptr = NextY;//ptr+=width of screen
	}


}
//y1 < y2 < y3
void FillTriangle(fp x1, fp y1, fp x2, fp y2, fp x3, fp y3, color FillWith)
{



}

//a c-sharp method from one of my old programs
void FillTriangle3D(fp x1, fp y1, fp d1, fp x2, fp y2, fp d2, fp x3, fp y3, fp d3, fp minx, fp maxx, color FillWith)
{
	fp btop = 0 > y1 ? 0 : y1,
		bbottom = height < y3 ? height : y3,
		//https://codeplea.com/triangular-interpolation
		x1min3 = x1 - x3,
		x2min1 = x2 - x1,
		x3min2 = x3 - x2,
		x3min1 = x3 - x1,
		y1min2 = y1 - y2,
		y1min3 = y1 - y3,
		y2min1 = y2 - y1,
		y2min3 = y2 - y3,
		y3min1 = y3 - y1,
		y3min2 = y3 - y2,
		det = y2min3 * x1min3 + x3min2 * -y3min1,

		//making a triangular distance formula
		w1 = (y2min3 * -x3 + x3min2 * -y3) / det,
		w2 = (y3min1 * -x3 + x1min3 * -y3) / det,
		v00 = w1 * d1 + w2 * d2 + (1 - w1 - w2) * d3;//at pixel x0 y0
	w1 += y2min3 / det;//w1 = (y2min3 * (1 - x3) + x3min2 * -y3) / det;
	w2 += y3min1 / det;
	fp v10 = w1 * d1 + w2 * d2 + (1 - w1 - w2) * d3,//at pixel x1 y0
		distx = v10 - v00;//per pixel to the right difference
	w1 = (y2min3 * -x3 + x3min2 * (1 - y3)) / det;
	w2 = (y3min1 * -x3 + x1min3 * (1 - y3)) / det;
	fp v01 = w1 * d1 + w2 * d2 + (1 - w1 - w2) * d3,//at pixel x0 y0
		disty = v01 - v00;//per pixel to the bottom difference
	bool
		l12w0 = x1 == x2,
		l23w0 = x2 == x3,
		l31w0 = x3 == x1;
	fp
		l12dydx = y2min1 / x2min1,
		l12yatx0 = y1 - l12dydx * x1,
		l23dydx = y3min2 / x3min2,
		l23yatx0 = y2 - l23dydx * x2,
		l31dydx = y3min1 / x3min1,
		l31yatx0 = y1 - l31dydx * x1;
	for (int j = (int)btop; j < bbottom; j++)
	{
		fp mostleft = maxx, mostright = minx;
		if (j >= y1 && j < y2)
		{
			mostleft = l12w0 ? x1 : (j - l12yatx0) / l12dydx;//(y-b)/a intersection point
		}
		if (j >= y2 && j < y3)
		{
			fp intersectx = l23w0 ? x2 : (j - l23yatx0) / l23dydx;//(y-b)/a
			if (intersectx < mostleft)
			{
				mostright = mostleft;
				mostleft = intersectx;
			}
			else
				mostright = intersectx;
		}
		if (j >= y1 && j < y3)
		{
			fp intersectx = l31w0 ? x3 : (j - l31yatx0) / l31dydx;//(y-b)/a
			if (intersectx < mostleft)
			{
				mostright = mostleft;
				mostleft = intersectx;
			}
			else mostright = intersectx;
		}

		int bleft = 0 > mostleft ? 0 : (int)mostleft;
		int bright = width < mostright ? width : (int)mostright;
		if (bleft < bright)
		{
			//int index = (j * w + (int)bleft) * 4;

			int EndPixelIndex = j * width + bright;
			//
			//{
			int PixelIndex = j * width + bleft;
			fp* ptr = depthbuffer + PixelIndex;
			color* colourptr = colors + PixelIndex;
			fp* endptr = depthbuffer + EndPixelIndex;
			fp* endptr2 = endptr - 2 - 1;
			fp distance = v00 + j * disty + bleft * distx;
			ptr--;
			while (ptr < endptr2)
			{
				distance += distx;
				if (*++ptr > distance)
				{
					*colourptr = FillWith;
					*ptr = distance;
				}
				colourptr++;
				distance += distx;
				if (*++ptr > distance)
				{
					*colourptr = FillWith;
					*ptr = distance;
				}
				colourptr++;
			}
			while (++ptr < endptr)
			{
				distance += distx;
				if (*ptr > distance)
				{
					*colourptr = FillWith;
					*ptr = distance;
				}
				colourptr++;
			}
		}
	}
}