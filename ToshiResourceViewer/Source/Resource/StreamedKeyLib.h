#pragma once
#include "Serializer/TKLBuilder.h"

#include <Toshi/T2SharedPtr.h>
#include <Render/TAnimation.h>

namespace Resource
{

class StreamedKeyLib
{
public:
	StreamedKeyLib();
	~StreamedKeyLib();

	void  Setup( const Toshi::TPString8& strName );
	TBOOL Create( Toshi::TKeyframeLibrary::TRBHeader* pTRBHeader );
	TBOOL Create( const TKLBuilder& rcTKLBuilder );
	void  Destroy();

	TBOOL IsLoaded() const { return m_pTKeyFrameLib; }
	TBOOL IsDummy() const { return !m_pTKeyFrameLib; }

	const Toshi::TPString8&  GetName() const { return m_strName; }
	Toshi::TKeyframeLibrary* GetLibrary() const { return m_pTKeyFrameLib; }

	TINT GetNumTranslations() const { return m_pTRBDataCopy->m_iNumTranslations; }
	TINT GetNumQuaternions() const { return m_pTRBDataCopy->m_iNumQuaternions; }
	TINT GetNumScales() const { return m_pTRBDataCopy->m_iNumScales; }
	TINT GetTranslationSize() const { return m_pTRBDataCopy->m_iTranslationSize; }
	TINT GetQuaternionSize() const { return m_pTRBDataCopy->m_iQuaternionSize; }
	TINT GetScaleSize() const { return m_pTRBDataCopy->m_iScaleSize; }

	const Toshi::TKeyframeLibrary::TRBHeader* GetTRBHeader() const { return m_pTRBDataCopy; }

private:
	Toshi::TPString8         m_strName;
	Toshi::TKeyframeLibrary* m_pTKeyFrameLib;
	Toshi::TKeyframeLibrary::TRBHeader* m_pTRBDataCopy;
};

extern Toshi::TKeyframeLibraryManager* g_pKeyFrameLibManager;

using StreamedKeyLibMap = Toshi::T2Map<Toshi::TPString8, Toshi::T2WeakPtr<Resource::StreamedKeyLib>, Toshi::TPString8::Comparator>;

void                        StreamedKeyLib_DestroyUnused();
TINT                        StreamedKeyLib_GetTotalNumber();
StreamedKeyLibMap::Iterator StreamedKeyLib_GetIteratorBegin();
StreamedKeyLibMap::Iterator StreamedKeyLib_GetIteratorEnd();

Toshi::T2SharedPtr<StreamedKeyLib> StreamedKeyLib_Create( const Toshi::TPString8& strName, Toshi::TKeyframeLibrary::TRBHeader* pTRBHeader );
Toshi::T2SharedPtr<StreamedKeyLib> StreamedKeyLib_Create( const Toshi::TPString8& strName, const TKLBuilder& rcTKLBuilder );
Toshi::T2SharedPtr<StreamedKeyLib> StreamedKeyLib_FindOrCreateDummy( const Toshi::TPString8& strName );

} // namespace Resource
