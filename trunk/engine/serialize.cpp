#include <stdio.h>
#include <stdlib.h>
#include "serialize.h"

void TextureHeader::Load( FILE* fp )
{
	fread( this, sizeof(TextureHeader), 1, fp );
}
