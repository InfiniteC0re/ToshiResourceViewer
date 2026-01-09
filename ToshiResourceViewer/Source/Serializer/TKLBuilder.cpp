#include "pch.h"
#include "TKLBuilder.h"
#include "Hash.h"

//-----------------------------------------------------------------------------
// Enables memory debugging.
// Note: Should be the last include!
//-----------------------------------------------------------------------------
#include <Core/TMemoryDebugOn.h>

TOSHI_NAMESPACE_USING

static constexpr TFLOAT flMaxError = 0.01f;

TINT TKLBuilder::AddTranslation( const Toshi::TAnimVector& rcTranslation )
{
	T2_FOREACH( m_vecTranslations, it )
	{
		TFLOAT flError = TMath::Abs( it->x - rcTranslation.x ) + TMath::Abs( it->y - rcTranslation.y ) + TMath::Abs( it->z - rcTranslation.z );
		if ( flError > flMaxError ) continue;

		return it.Index();
	}

	m_vecTranslations.PushBack( rcTranslation );
	return m_vecTranslations.Size() - 1;
}

TINT TKLBuilder::AddRotation( const Toshi::TAnimQuaternion& rcRotation )
{
	T2_FOREACH( m_vecRotations, it )
	{
		TFLOAT flError = TMath::Abs( it->x - rcRotation.x ) + TMath::Abs( it->y - rcRotation.y ) + TMath::Abs( it->z - rcRotation.z ) + TMath::Abs( it->w - rcRotation.w );
		if ( flError > flMaxError ) continue;

		return it.Index();
	}

	m_vecRotations.PushBack( rcRotation );
	return m_vecRotations.Size() - 1;
}

TINT TKLBuilder::AddScale( TAnimScale flScale )
{
	T2_FOREACH( m_vecScales, it )
	{
		if ( TMath::Abs( *it - flScale ) >= flMaxError ) continue;

		return it.Index();
	}

	m_vecScales.PushBack( flScale );
	return m_vecScales.Size() - 1;
}
