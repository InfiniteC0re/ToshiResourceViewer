#pragma once
#include <Math/TVector3.h>
#include <Math/TQuaternion.h>
#include <ToshiTools/T2DynamicVector.h>

class TKLBuilder
{
public:
	TKLBuilder()  = default;
	~TKLBuilder() = default;

	TINT AddTranslation( const Toshi::TAnimVector& rcTranslation );
	TINT AddRotation( const Toshi::TAnimQuaternion& rcRotation );
	TINT AddScale( Toshi::TAnimScale flScale );

	const Toshi::T2DynamicVector<Toshi::TAnimVector>&     GetTranslations() const { return m_vecTranslations; }
	const Toshi::T2DynamicVector<Toshi::TAnimQuaternion>& GetRotations() const { return m_vecRotations; }
	const Toshi::T2DynamicVector<Toshi::TAnimScale>&      GetScales() const { return m_vecScales; }

	void                SetName( Toshi::T2StringView pchName ) { m_strName = pchName; }
	Toshi::T2StringView GetName() const { return m_strName.GetString(); }

private:
	Toshi::TString8                                m_strName;
	Toshi::T2DynamicVector<Toshi::TAnimVector>     m_vecTranslations;
	Toshi::T2DynamicVector<Toshi::TAnimQuaternion> m_vecRotations;
	Toshi::T2DynamicVector<Toshi::TAnimScale>      m_vecScales;
};
