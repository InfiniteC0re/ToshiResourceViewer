#include "pch.h"
#include "StreamedKeyLib.h"

//-----------------------------------------------------------------------------
// Enables memory debugging.
// Note: Should be the last include!
//-----------------------------------------------------------------------------
#include <Core/TMemoryDebugOn.h>

TOSHI_NAMESPACE_USING

namespace Resource
{

static TKeyframeLibraryManager s_oKeyFrameLibManager;
TKeyframeLibraryManager* g_pKeyFrameLibManager = &s_oKeyFrameLibManager;

StreamedKeyLib::StreamedKeyLib()
    : m_pTKeyFrameLib( TNULL )
    , m_pTRBDataCopy( TNULL )
{
}

StreamedKeyLib::~StreamedKeyLib()
{
	Destroy();
}

void StreamedKeyLib::Setup( const TPString8& strName )
{
	m_strName = strName;
}

TBOOL StreamedKeyLib::Create( TKeyframeLibrary::TRBHeader* pTRBHeader )
{
	if ( !pTRBHeader )
		return TFALSE;

	// Create a copy of the data
	m_pTRBDataCopy = new TKeyframeLibrary::TRBHeader();

	m_pTRBDataCopy->m_szName = new TCHAR[ T2String8::Length( pTRBHeader->m_szName ) + 1 ];
	T2String8::Copy( m_pTRBDataCopy->m_szName, pTRBHeader->m_szName );

	m_pTRBDataCopy->m_SomeVector       = pTRBHeader->m_SomeVector;
	m_pTRBDataCopy->m_iNumTranslations = pTRBHeader->m_iNumTranslations;
	m_pTRBDataCopy->m_iNumQuaternions  = pTRBHeader->m_iNumQuaternions;
	m_pTRBDataCopy->m_iNumScales       = pTRBHeader->m_iNumScales;
	m_pTRBDataCopy->m_iTranslationSize = pTRBHeader->m_iTranslationSize;
	m_pTRBDataCopy->m_iQuaternionSize  = pTRBHeader->m_iQuaternionSize;
	m_pTRBDataCopy->m_iScaleSize       = pTRBHeader->m_iScaleSize;

	m_pTRBDataCopy->m_pTranslations = new TAnimVector[ pTRBHeader->m_iNumTranslations ];
	TUtil::MemCopy( m_pTRBDataCopy->m_pTranslations, pTRBHeader->m_pTranslations, sizeof( TAnimVector ) * pTRBHeader->m_iNumTranslations );

	m_pTRBDataCopy->m_pQuaternions = new TAnimQuaternion[ pTRBHeader->m_iNumQuaternions ];
	TUtil::MemCopy( m_pTRBDataCopy->m_pQuaternions, pTRBHeader->m_pQuaternions, sizeof( TAnimQuaternion ) * pTRBHeader->m_iNumQuaternions );

	m_pTRBDataCopy->m_pScales = new TAnimScale[ pTRBHeader->m_iNumScales ];
	TUtil::MemCopy( m_pTRBDataCopy->m_pScales, pTRBHeader->m_pScales, sizeof( TAnimScale ) * pTRBHeader->m_iNumScales );

	m_pTKeyFrameLib = g_pKeyFrameLibManager->LoadLibrary( m_pTRBDataCopy );

	return m_pTKeyFrameLib != TNULL;
}

void StreamedKeyLib::Destroy()
{
	if ( m_pTKeyFrameLib )
	{
		g_pKeyFrameLibManager->UnloadLibrary( m_pTKeyFrameLib );
		m_pTKeyFrameLib = TNULL;
	}

	if ( m_pTRBDataCopy )
	{
		if ( m_pTRBDataCopy->m_szName )
			delete[] m_pTRBDataCopy->m_szName;

		if ( m_pTRBDataCopy->m_pTranslations )
			delete[] m_pTRBDataCopy->m_pTranslations;

		if ( m_pTRBDataCopy->m_pQuaternions )
			delete[] m_pTRBDataCopy->m_pQuaternions;

		if ( m_pTRBDataCopy->m_pScales )
			delete[] m_pTRBDataCopy->m_pScales;

		delete m_pTRBDataCopy;
	}
}

Resource::StreamedKeyLibMap g_mapStreamedKeyLibs;

void StreamedKeyLib_DestroyUnused()
{
	for ( auto it = g_mapStreamedKeyLibs.Begin(); it != g_mapStreamedKeyLibs.End(); )
	{
		auto next = it.Next();

		// Destroy the weak reference if no strong references are left
		if ( !it->second.IsValid() )
			g_mapStreamedKeyLibs.Remove( it );

		it = next;
	}
}

TINT StreamedKeyLib_GetTotalNumber()
{
	return g_mapStreamedKeyLibs.Size();
}

Resource::StreamedKeyLibMap::Iterator StreamedKeyLib_GetIteratorBegin()
{
	return g_mapStreamedKeyLibs.Begin();
}

Resource::StreamedKeyLibMap::Iterator StreamedKeyLib_GetIteratorEnd()
{
	return g_mapStreamedKeyLibs.End();
}

T2SharedPtr<Resource::StreamedKeyLib> StreamedKeyLib_Create( const TPString8& strName, TKeyframeLibrary::TRBHeader* pTRBHeader )
{
	// See if the keyframe lib is already created
	auto it = g_mapStreamedKeyLibs.Find( strName );

	if ( it != g_mapStreamedKeyLibs.End() )
	{
		if ( !it->second.IsValid() )
			Resource::StreamedKeyLib_DestroyUnused();
		else
		{
			if ( it->second->IsDummy() )
			{
				// Complete the keyframe lib with real data
				it->second->Setup( strName );
				it->second->Create( pTRBHeader );
			}

			return it->second;
		}
	}

	// Create new real keyframe lib
	auto pKeyLib = T2SharedPtr<Resource::StreamedKeyLib>::New();

	pKeyLib->Setup( strName );
	pKeyLib->Create( pTRBHeader );

	g_mapStreamedKeyLibs.Insert( strName, T2WeakPtr<Resource::StreamedKeyLib>( pKeyLib ) );

	return pKeyLib;
}

T2SharedPtr<Resource::StreamedKeyLib> StreamedKeyLib_FindOrCreateDummy( const TPString8& strName )
{
	// See if the keyframe lib is already created
	auto it = g_mapStreamedKeyLibs.Find( strName );

	if ( it != g_mapStreamedKeyLibs.End() )
	{
		if ( !it->second.IsValid() )
			Resource::StreamedKeyLib_DestroyUnused();
		else
			return it->second;
	}

	// Create new dummy keyframe lib
	auto pKeyLib = T2SharedPtr<Resource::StreamedKeyLib>::New();

	pKeyLib->Setup( strName );
	g_mapStreamedKeyLibs.Insert( strName, T2WeakPtr<Resource::StreamedKeyLib>( pKeyLib ) );

	return pKeyLib;
}

} // namespace Resource
