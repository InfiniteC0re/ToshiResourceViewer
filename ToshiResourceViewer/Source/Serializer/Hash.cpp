#include "pch.h"
#include "Hash.h"

//-----------------------------------------------------------------------------
// Enables memory debugging.
// Note: Should be the last include!
//-----------------------------------------------------------------------------
#include <Core/TMemoryDebugOn.h>

TOSHI_NAMESPACE_USING

TUINT32 Hash_Vec2i( TINT a_iX, TINT a_iY )
{
	TINT buffer[] = { a_iX, a_iY };
	return TUtil::CRC32( buffer, sizeof( buffer ) );
}

TUINT32 Hash_Vec3f( TFLOAT a_fX, TFLOAT a_fY, TFLOAT a_fZ )
{
	TFLOAT buffer[] = { a_fX, a_fY, a_fZ };
	return TUtil::CRC32( buffer, sizeof( buffer ) );
}

TUINT32 Hash_Vec4f( TFLOAT a_fX, TFLOAT a_fY, TFLOAT a_fZ, TFLOAT a_fW )
{
	TFLOAT buffer[] = { a_fX, a_fY, a_fZ, a_fW };
	return TUtil::CRC32( buffer, sizeof( buffer ) );
}
