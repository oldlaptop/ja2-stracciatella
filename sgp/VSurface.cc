#include <stdexcept>

#include "Debug.h"
#include "HImage.h"
#include "MemMan.h"
#include "Shading.h"
#include "VObject_Blitters.h"
#include "VSurface.h"
#include "Video.h"


SGPVSurface::SGPVSurface(UINT16 const w, UINT16 const h, UINT8 const bpp) :
	p16BPPPalette()
{
	Assert(w > 0);
	Assert(h > 0);

	SDL_Surface* s;
	switch (bpp)
	{
		case 8:
			s = SDL_CreateRGBSurface(SDL_SWSURFACE, w, h, bpp, 0, 0, 0, 0);
			break;

		case 16:
		{
			SDL_PixelFormat const* f = SDL_GetVideoSurface()->format;
			s = SDL_CreateRGBSurface(SDL_SWSURFACE, w, h, bpp, f->Rmask, f->Gmask, f->Bmask, 0);
			break;
		}

		default:
			throw std::logic_error("Tried to create video surface with invalid bpp, must be 8 or 16.");
	}
	if (!s) throw std::runtime_error("Failed to create SDL surface");
	surface_ = s;
}


SGPVSurface::~SGPVSurface()
{
	if (p16BPPPalette) MemFree(p16BPPPalette);
}


void SGPVSurface::SetPalette(const SGPPaletteEntry* const src_pal)
{
	// Create palette object if not already done so
	if (!palette_) palette_.Allocate(256);
	SGPPaletteEntry* const p = palette_;
	for (UINT32 i = 0; i < 256; i++)
	{
		p[i] = src_pal[i];
	}

	if (p16BPPPalette != NULL) MemFree(p16BPPPalette);
	p16BPPPalette = Create16BPPPalette(src_pal);
}


void SGPVSurface::SetTransparency(const COLORVAL colour)
{
	Uint32 colour_key;
	switch (BPP())
	{
		case  8: colour_key = colour;                break;
		case 16: colour_key = Get16BPPColor(colour); break;

		default: abort(); // HACK000E
	}
	SDL_SetColorKey(surface_, SDL_SRCCOLORKEY, colour_key);
}


void SGPVSurface::Fill(const UINT16 colour)
{
	SDL_FillRect(surface_, NULL, colour);
}


static void InternalShadowVideoSurfaceRect(SGPVSurface* const dst, INT32 X1, INT32 Y1, INT32 X2, INT32 Y2, const UINT16* const filter_table)
{
	if (X1 < 0) X1 = 0;
	if (X2 < 0) return;

	if (Y2 < 0) return;
	if (Y1 < 0) Y1 = 0;

	if (X2 >= dst->Width())  X2 = dst->Width() - 1;
	if (Y2 >= dst->Height()) Y2 = dst->Height() - 1;

	if (X1 >= dst->Width())  return;
	if (Y1 >= dst->Height()) return;

	if (X2 - X1 <= 0) return;
	if (Y2 - Y1 <= 0) return;

	SGPRect area;
	area.iTop    = Y1;
	area.iBottom = Y2;
	area.iLeft   = X1;
	area.iRight  = X2;

	SGPVSurface::Lock ldst(dst);
	Blt16BPPBufferFilterRect(ldst.Buffer<UINT16>(), ldst.Pitch(), filter_table, &area);
}


void SGPVSurface::ShadowRect(INT32 const x1, INT32 const y1, INT32 const x2, INT32 const y2)
{
	InternalShadowVideoSurfaceRect(this, x1, y1, x2, y2, ShadeTable);
}


void SGPVSurface::ShadowRectUsingLowPercentTable(INT32 const x1, INT32 const y1, INT32 const x2, INT32 const y2)
{
	InternalShadowVideoSurfaceRect(this, x1, y1, x2, y2, IntensityTable);
}


static void DeletePrimaryVideoSurfaces(void);


typedef struct VSURFACE_NODE
{
	SGPVSurface*   hVSurface;
	VSURFACE_NODE* next;

#ifdef SGP_VIDEO_DEBUGGING
	char* pName;
	char* pCode;
#endif
} VSURFACE_NODE;

static VSURFACE_NODE* gpVSurfaceHead = NULL;
static VSURFACE_NODE* gpVSurfaceTail = NULL;


SGPVSurface* g_back_buffer;
SGPVSurface* g_frame_buffer;
SGPVSurface* g_mouse_buffer;


static void SetPrimaryVideoSurfaces(void);


void InitializeVideoSurfaceManager(void)
{
	//Shouldn't be calling this if the video surface manager already exists.
	//Call shutdown first...
	Assert(gpVSurfaceHead == NULL);
	Assert(gpVSurfaceTail == NULL);
	gpVSurfaceHead = NULL;
	gpVSurfaceTail = NULL;

	// Create primary and backbuffer from globals
	SetPrimaryVideoSurfaces();
}


void ShutdownVideoSurfaceManager(void)
{
  DebugMsg(TOPIC_VIDEOSURFACE, DBG_LEVEL_0, "Shutting down the Video Surface manager");

	// Delete primary viedeo surfaces
	DeletePrimaryVideoSurfaces();

	while (gpVSurfaceHead != NULL)
	{
		VSURFACE_NODE* curr = gpVSurfaceHead;
		gpVSurfaceHead = gpVSurfaceHead->next;
		delete curr->hVSurface;
#ifdef SGP_VIDEO_DEBUGGING
		if (curr->pName != NULL) MemFree(curr->pName);
		if (curr->pCode != NULL) MemFree(curr->pCode);
#endif
		MemFree(curr);
	}
	gpVSurfaceHead = NULL;
	gpVSurfaceTail = NULL;
#ifdef SGP_VIDEO_DEBUGGING
	guiVSurfaceSize = 0;
#endif
}


static void AddStandardVideoSurface(SGPVSurface* const hVSurface)
{
	VSURFACE_NODE* const Node = MALLOC(VSURFACE_NODE);
	Node->hVSurface = hVSurface;

	Node->next      = NULL;
#ifdef SGP_VIDEO_DEBUGGING
	Node->pName     = NULL;
	Node->pCode     = NULL;
	++guiVSurfaceSize;
#endif

	if (gpVSurfaceTail == NULL)
	{
		gpVSurfaceHead = Node;
	}
	else
	{
		gpVSurfaceTail->next = Node;
	}
	gpVSurfaceTail = Node;
}


#undef AddVideoSurface
#undef AddVideoSurfaceFromFile


SGPVSurface* AddVideoSurface(UINT16 Width, UINT16 Height, UINT8 BitDepth)
{
	SGPVSurface* const vs = new SGPVSurface(Width, Height, BitDepth);
	AddStandardVideoSurface(vs);
	return vs;
}


SGPVSurface* AddVideoSurfaceFromFile(const char* const Filename)
{
	AutoSGPImage img(CreateImage(Filename, IMAGE_ALLIMAGEDATA));

	SGPVSurface* const vs = new SGPVSurface(img->usWidth, img->usHeight, img->ubBitDepth);

	UINT8 const dst_bpp = vs->BPP();
	UINT32      buffer_bpp;
	switch (dst_bpp)
	{
		case  8: buffer_bpp = BUFFER_8BPP;  break;
		case 16: buffer_bpp = BUFFER_16BPP; break;
		default: throw std::logic_error("Invalid bpp");
	}

	{ SGPVSurface::Lock l(vs);
		UINT8*  const dst   = l.Buffer<UINT8>();
		UINT16  const pitch = l.Pitch() / (dst_bpp / 8); // pitch in pixels
		SGPBox  const box   = { 0, 0, img->usWidth, img->usHeight };
		BOOLEAN const Ret   = CopyImageToBuffer(img, buffer_bpp, dst, pitch, vs->Height(), 0, 0, &box);
		if (!Ret)
		{
			DebugMsg(TOPIC_VIDEOSURFACE, DBG_LEVEL_2, "Error Occured Copying HIMAGE to video surface");
		}
	}

	if (img->ubBitDepth == 8) vs->SetPalette(img->pPalette);

	AddStandardVideoSurface(vs);
	return vs;
}


static void SetPrimaryVideoSurfaces(void)
{
	// Delete surfaces if they exist
	DeletePrimaryVideoSurfaces();

#ifdef JA2
	g_back_buffer  = new SGPVSurface(GetBackBufferObject());
	g_mouse_buffer = new SGPVSurface(GetMouseBufferObject());
#endif
	g_frame_buffer = new SGPVSurface(GetFrameBufferObject());
}


static void DeletePrimaryVideoSurfaces(void)
{
	delete g_back_buffer;
	g_back_buffer = NULL;

	delete g_frame_buffer;
	g_frame_buffer = NULL;

	delete g_mouse_buffer;
	g_mouse_buffer = NULL;
}


void BltVideoSurfaceHalf(SGPVSurface* const dst, SGPVSurface* const src, const INT32 DestX, const INT32 DestY, const SGPRect* const SrcRect)
{
	SGPVSurface::Lock lsrc(src);
	SGPVSurface::Lock ldst(dst);
	UINT8*  const SrcBuf         = lsrc.Buffer<UINT8>();
	UINT32  const SrcPitchBYTES  = lsrc.Pitch();
	UINT16* const DestBuf        = ldst.Buffer<UINT16>();
	UINT32  const DestPitchBYTES = ldst.Pitch();
	if (SrcRect == NULL)
	{
		Blt8BPPDataTo16BPPBufferHalf(DestBuf, DestPitchBYTES, src, SrcBuf, SrcPitchBYTES, DestX, DestY);
	}
	else
	{
		Blt8BPPDataTo16BPPBufferHalfRect(DestBuf, DestPitchBYTES, src, SrcBuf, SrcPitchBYTES, DestX, DestY, SrcRect);
	}
}


void ColorFillVideoSurfaceArea(SGPVSurface* const dst, INT32 iDestX1, INT32 iDestY1, INT32 iDestX2, INT32 iDestY2, const UINT16 Color16BPP)
{
	SGPRect Clip;
	GetClippingRect(&Clip);

	if (iDestX1 < Clip.iLeft) iDestX1 = Clip.iLeft;
	if (iDestX1 > Clip.iRight) return;

	if (iDestX2 > Clip.iRight) iDestX2 = Clip.iRight;
	if (iDestX2 < Clip.iLeft) return;

	if (iDestY1 < Clip.iTop) iDestY1 = Clip.iTop;
	if (iDestY1 > Clip.iBottom) return;

	if (iDestY2 > Clip.iBottom) iDestY2 = Clip.iBottom;
	if (iDestY2 < Clip.iTop) return;

	if (iDestX2 <= iDestX1 || iDestY2 <= iDestY1) return;

	SDL_Rect Rect;
	Rect.x = iDestX1;
	Rect.y = iDestY1;
	Rect.w = iDestX2 - iDestX1;
	Rect.h = iDestY2 - iDestY1;
	SDL_FillRect(dst->surface_, &Rect, Color16BPP);
}


void DeleteVideoSurface(SGPVSurface* const vs)
{
	VSURFACE_NODE* prev = NULL;
	VSURFACE_NODE* curr = gpVSurfaceHead;
	while (curr != NULL)
	{
		if (curr->hVSurface == vs)
		{ //Found the node, so detach it and delete it.
			delete vs;

			if (curr == gpVSurfaceHead) gpVSurfaceHead = gpVSurfaceHead->next;
			if (curr == gpVSurfaceTail) gpVSurfaceTail = prev;
			if (prev != NULL) prev->next = curr->next;

#ifdef SGP_VIDEO_DEBUGGING
			if (curr->pName != NULL) MemFree(curr->pName);
			if (curr->pCode != NULL) MemFree(curr->pCode);
			--guiVSurfaceSize;
#endif

			MemFree(curr);
			return;
		}
		prev = curr;
		curr = curr->next;
	}
}


// Will drop down into user-defined blitter if 8->16 BPP blitting is being done
void BltVideoSurface(SGPVSurface* const dst, SGPVSurface* const src, const INT32 iDestX, const INT32 iDestY, const SGPRect* const SRect)
{
	Assert(dst);
	Assert(src);

	const UINT8 src_bpp = src->BPP();
	const UINT8 dst_bpp = dst->BPP();
	if (src_bpp == dst_bpp)
	{
		SDL_Rect* src_rect = 0;
		SDL_Rect  r;
		if (SRect)
		{
			r.x = SRect->iLeft;
			r.y = SRect->iTop;
			r.w = SRect->iRight  - SRect->iLeft;
			r.h = SRect->iBottom - SRect->iTop;
			src_rect = &r;
		}

		SDL_Rect dstrect;
		dstrect.x = iDestX;
		dstrect.y = iDestY;
		SDL_BlitSurface(src->surface_, src_rect, dst->surface_, &dstrect);
#if defined __GNUC__ && defined i386
		__asm__ __volatile__("cld"); // XXX HACK000D
#endif
	}
	else if (src_bpp < dst_bpp)
	{
		SGPRect const* src_rect = SRect;
		SGPRect        r;
		if (!src_rect)
		{
			// Check Sizes, SRC size MUST be <= DEST size
			if (dst->Height() < src->Height())
			{
				DebugMsg(TOPIC_VIDEOSURFACE, DBG_LEVEL_2, "Incompatible height size given in Video Surface blit");
				return;
			}
			if (dst->Width() < src->Width())
			{
				DebugMsg(TOPIC_VIDEOSURFACE, DBG_LEVEL_2, "Incompatible height size given in Video Surface blit");
				return;
			}

			r.iLeft   = 0;
			r.iTop    = 0;
			r.iRight  = src->Width();
			r.iBottom = src->Height();
			src_rect = &r;
		}

		SGPVSurface::Lock lsrc(src);
		SGPVSurface::Lock ldst(dst);
		UINT8*  const s_buf  = lsrc.Buffer<UINT8>();
		UINT32  const spitch = lsrc.Pitch();
		UINT16* const d_buf  = ldst.Buffer<UINT16>();
		UINT32  const dpitch = ldst.Pitch();
		Blt8BPPDataSubTo16BPPBuffer(d_buf, dpitch, src, s_buf, spitch, iDestX, iDestY, src_rect);
	}
	else
	{
		DebugMsg(TOPIC_VIDEOSURFACE, DBG_LEVEL_2, "Incompatible BPP values with src and dest Video Surfaces for blitting");
	}
}


void BltStretchVideoSurface(SGPVSurface* const dst, const SGPVSurface* const src, SGPRect* const SrcRect, SGPRect* const DestRect)
{
	if (dst->BPP() != 16 || src->BPP() != 16) return;

	SDL_Surface const* const ssurface = src->surface_;
	SDL_Surface*       const dsurface = dst->surface_;

	const UINT32  s_pitch = ssurface->pitch >> 1;
	const UINT32  d_pitch = dsurface->pitch >> 1;
	const UINT16* os      = (const UINT16*)ssurface->pixels + s_pitch * SrcRect->iTop  + SrcRect->iLeft;
	UINT16*       d       =       (UINT16*)dsurface->pixels + d_pitch * DestRect->iTop + DestRect->iLeft;

	const UINT width  = DestRect->iRight  - DestRect->iLeft;
	const UINT height = DestRect->iBottom - DestRect->iTop;
	const UINT dx = SrcRect->iRight  - SrcRect->iLeft;
	const UINT dy = SrcRect->iBottom - SrcRect->iTop;
	UINT py = 0;
	if (ssurface->flags & SDL_SRCCOLORKEY)
	{
		const UINT16 key = ssurface->format->colorkey;
		for (UINT iy = 0; iy < height; ++iy)
		{
			const UINT16* s = os;
			UINT px = 0;
			for (UINT ix = 0; ix < width; ++ix)
			{
				if (*s != key) *d = *s;
				++d;
				px += dx;
				for (; px >= width; px -= width) ++s;
			}
			d += d_pitch - width;
			py += dy;
			for (; py >= height; py -= height) os += s_pitch;
		}
	}
	else
	{
		for (UINT iy = 0; iy < height; ++iy)
		{
			const UINT16* s = os;
			UINT px = 0;
			for (UINT ix = 0; ix < width; ++ix)
			{
				*d++ = *s;
				px += dx;
				for (; px >= width; px -= width) ++s;
			}
			d += d_pitch - width;
			py += dy;
			for (; py >= height; py -= height) os += s_pitch;
		}
	}
}


void BltVideoSurfaceOnce(SGPVSurface* const dst, const char* const filename, INT32 const x, INT32 const y)
{
	AutoSGPVSurface src(AddVideoSurfaceFromFile(filename));
	BltVideoSurface(dst, src, x, y, NULL);
}


#ifdef SGP_VIDEO_DEBUGGING

UINT32 guiVSurfaceSize = 0;


typedef struct DUMPINFO
{
	UINT32 Counter;
	char Name[256];
	char Code[256];
} DUMPINFO;


void DumpVSurfaceInfoIntoFile(const char* filename, BOOLEAN fAppend)
{
	if (!guiVSurfaceSize) return;

	FILE* fp = fopen(filename, fAppend ? "a" : "w");
	Assert(fp != NULL);

	//Allocate enough strings and counters for each node.
	DUMPINFO* const Info = MALLOCNZ(DUMPINFO, guiVSurfaceSize);

	//Loop through the list and record every unique filename and count them
	UINT32 uiUniqueID = 0;
	for (const VSURFACE_NODE* curr = gpVSurfaceHead; curr != NULL; curr = curr->next)
	{
		const char* Name = curr->pName;
		const char* Code = curr->pCode;
		BOOLEAN fFound = FALSE;
		for (UINT32 i = 0; i < uiUniqueID; i++)
		{
			if (strcasecmp(Name, Info[i].Name) == 0 && strcasecmp(Code, Info[i].Code) == 0)
			{ //same string
				fFound = TRUE;
				Info[i].Counter++;
				break;
			}
		}
		if (!fFound)
		{
			strcpy(Info[uiUniqueID].Name, Name);
			strcpy(Info[uiUniqueID].Code, Code);
			Info[uiUniqueID].Counter++;
			uiUniqueID++;
		}
	}

	//Now dump the info.
	fprintf(fp, "-----------------------------------------------\n");
	fprintf(fp, "%d unique vSurface names exist in %d VSurfaces\n", uiUniqueID, guiVSurfaceSize);
	fprintf(fp, "-----------------------------------------------\n\n");
	for (UINT32 i = 0; i < uiUniqueID; i++)
	{
		fprintf(fp, "%d occurrences of %s\n%s\n\n", Info[i].Counter, Info[i].Name, Info[i].Code);
	}
	fprintf(fp, "\n-----------------------------------------------\n\n");

	MemFree(Info);
	fclose(fp);
}


static void RecordVSurface(const char* Filename, UINT32 LineNum, const char* SourceFile)
{
	//record the filename of the vsurface (some are created via memory though)
	gpVSurfaceTail->pName = MALLOCN(char, strlen(Filename) + 1);
	strcpy(gpVSurfaceTail->pName, Filename);

	//record the code location of the calling creating function.
	char str[256];
	sprintf(str, "%s -- line(%d)", SourceFile, LineNum);
	gpVSurfaceTail->pCode = MALLOCN(char, strlen(str) + 1);
	strcpy(gpVSurfaceTail->pCode, str);
}


SGPVSurface* AddAndRecordVSurface(const UINT16 Width, const UINT16 Height, const UINT8 BitDepth, const UINT32 LineNum, const char* const SourceFile)
{
	SGPVSurface* const vs = AddVideoSurface(Width, Height, BitDepth);
	RecordVSurface("<EMPTY>", LineNum, SourceFile);
	return vs;
}


SGPVSurface* AddAndRecordVSurfaceFromFile(const char* const Filename, const UINT32 LineNum, const char* const SourceFile)
{
	SGPVSurface* const vs = AddVideoSurfaceFromFile(Filename);
	RecordVSurface(Filename, LineNum, SourceFile);
	return vs;
}

#endif
