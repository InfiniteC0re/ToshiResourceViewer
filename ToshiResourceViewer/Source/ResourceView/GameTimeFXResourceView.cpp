#include "pch.h"
#include "GameTimeFXResourceView.h"

//-----------------------------------------------------------------------------
// Enables memory debugging.
// Note: Should be the last include!
//-----------------------------------------------------------------------------
#include <Core/TMemoryDebugOn.h>

TOSHI_NAMESPACE_USING

GameTimeFXResourceView::GameTimeFXResourceView()
{
	m_strName = "GameTimeFX";
}

GameTimeFXResourceView::~GameTimeFXResourceView()
{
	if ( m_pSettings ) delete[] m_pSettings;
}

TBOOL GameTimeFXResourceView::OnCreate( Toshi::T2StringView pchFilePath )
{
	TRBResourceView::OnCreate( pchFilePath );

	PTRB mergeTo( "D:\\_dev\\OpenBarnyard\\Game\\Data\\GameTimeFXSettings.trb" );

	auto fxSettingsOut   = mergeTo.GetSymbols()->Find<Settings_TRBHeader*>( mergeTo.GetSections(), "settings" ).get();
	auto fxLightScaleOut = mergeTo.GetSymbols()->Find<LightScale_TRBHeader>( mergeTo.GetSections(), "lightscale" ).get();

	PTRBSymbols* pSymbols = m_pTRB->GetSymbols();

	auto fxSettings   = pSymbols->Find<Settings_TRBHeader*>( m_pTRB->GetSections(), "settings" ).get();
	auto fxLightScale = pSymbols->Find<LightScale_TRBHeader>( m_pTRB->GetSections(), "lightscale" ).get();

	if ( fxSettings )
	{
		fxLightScaleOut->a = ConvertEndianess( fxLightScale->a );
		fxLightScaleOut->b = ConvertEndianess( fxLightScale->b );
		fxLightScaleOut->c = ConvertEndianess( fxLightScale->c );
		fxLightScaleOut->d = ConvertEndianess( fxLightScale->d );
		fxLightScaleOut->e = ConvertEndianess( fxLightScale->e );
		fxLightScaleOut->f = ConvertEndianess( fxLightScale->f );
		fxLightScaleOut->g = ConvertEndianess( fxLightScale->g );
		fxLightScaleOut->h = ConvertEndianess( fxLightScale->h );

		// Count FXs
		m_iNumFXs = 0;
		while ( fxSettings[ m_iNumFXs ] ) m_iNumFXs++;

		// Allocate FXs
		m_pSettings = new Settings_TRBHeader[ m_iNumFXs ];
		for ( TINT i = 0; i < m_iNumFXs; i++ )
		{
			fxSettingsOut[ i ]->m_vecLightDir            = ConvertEndianess( fxSettings[ i ]->m_vecLightDir );
			fxSettingsOut[ i ]->field1_0x10              = ConvertEndianess( fxSettings[ i ]->field1_0x10 );
			fxSettingsOut[ i ]->m_oShadowColour          = ConvertEndianess( fxSettings[ i ]->m_oShadowColour );
			fxSettingsOut[ i ]->m_oAmbientColour         = ConvertEndianess( fxSettings[ i ]->m_oAmbientColour );
			fxSettingsOut[ i ]->m_vecFogColor            = ConvertEndianess( fxSettings[ i ]->m_vecFogColor );
			fxSettingsOut[ i ]->field5_0x50              = ConvertEndianess( fxSettings[ i ]->field5_0x50 );
			fxSettingsOut[ i ]->field6_0x60              = ConvertEndianess( fxSettings[ i ]->field6_0x60 );
			fxSettingsOut[ i ]->field7_0x70              = ConvertEndianess( fxSettings[ i ]->field7_0x70 );
			fxSettingsOut[ i ]->field8_0x80              = ConvertEndianess( fxSettings[ i ]->field8_0x80 );
			fxSettingsOut[ i ]->field9_0x90              = ConvertEndianess( fxSettings[ i ]->field9_0x90 );
			fxSettingsOut[ i ]->field10_0xa0             = ConvertEndianess( fxSettings[ i ]->field10_0xa0 );
			fxSettingsOut[ i ]->field11_0xb0             = ConvertEndianess( fxSettings[ i ]->field11_0xb0 );
			fxSettingsOut[ i ]->field12_0xc0             = ConvertEndianess( fxSettings[ i ]->field12_0xc0 );
			fxSettingsOut[ i ]->field13_0xd0             = ConvertEndianess( fxSettings[ i ]->field13_0xd0 );
			fxSettingsOut[ i ]->field14_0xe0             = ConvertEndianess( fxSettings[ i ]->field14_0xe0 );
			fxSettingsOut[ i ]->field15_0xf0             = ConvertEndianess( fxSettings[ i ]->field15_0xf0 );
			fxSettingsOut[ i ]->field16_0x100            = ConvertEndianess( fxSettings[ i ]->field16_0x100 );
			fxSettingsOut[ i ]->field17_0x110            = ConvertEndianess( fxSettings[ i ]->field17_0x110 );
			//fxSettingsOut[ i ]->m_pchName                = strdup( fxSettings[ i ]->m_pchName );
			fxSettingsOut[ i ]->field19_0x118            = ConvertEndianess( fxSettings[ i ]->field19_0x118 );
			fxSettingsOut[ i ]->m_fFogStart              = ConvertEndianess( fxSettings[ i ]->m_fFogStart );
			fxSettingsOut[ i ]->m_fFogEnd                = ConvertEndianess( fxSettings[ i ]->m_fFogEnd );
			fxSettingsOut[ i ]->field22_0x124            = ConvertEndianess( fxSettings[ i ]->field22_0x124 );
			fxSettingsOut[ i ]->m_fShadowAmbientProgress = ConvertEndianess( fxSettings[ i ]->m_fShadowAmbientProgress );
			fxSettingsOut[ i ]->m_flInstanceShading      = ConvertEndianess( fxSettings[ i ]->m_flInstanceShading );
			fxSettingsOut[ i ]->field25_0x130            = ConvertEndianess( fxSettings[ i ]->field25_0x130 );
			fxSettingsOut[ i ]->field26_0x134            = ConvertEndianess( fxSettings[ i ]->field26_0x134 );
			fxSettingsOut[ i ]->field27_0x138            = ConvertEndianess( fxSettings[ i ]->field27_0x138 );
			fxSettingsOut[ i ]->field28_0x13c            = ConvertEndianess( fxSettings[ i ]->field28_0x13c );
			fxSettingsOut[ i ]->m_fStarsOpacity          = ConvertEndianess( fxSettings[ i ]->m_fStarsOpacity );
			fxSettingsOut[ i ]->field30_0x144            = ConvertEndianess( fxSettings[ i ]->field30_0x144 );
			fxSettingsOut[ i ]->field31_0x148            = ConvertEndianess( fxSettings[ i ]->field31_0x148 );
			fxSettingsOut[ i ]->field32_0x14c            = ConvertEndianess( fxSettings[ i ]->field32_0x14c );
			fxSettingsOut[ i ]->m_fDarkeningFactor       = ConvertEndianess( fxSettings[ i ]->m_fDarkeningFactor );
			fxSettingsOut[ i ]->m_fLightIntensity        = ConvertEndianess( fxSettings[ i ]->m_fLightIntensity );
			fxSettingsOut[ i ]->field35_0x158            = ConvertEndianess( fxSettings[ i ]->field35_0x158 );
			fxSettingsOut[ i ]->field36_0x15c            = ConvertEndianess( fxSettings[ i ]->field36_0x15c );
			fxSettingsOut[ i ]->field37_0x160            = ConvertEndianess( fxSettings[ i ]->field37_0x160 );
			fxSettingsOut[ i ]->field38_0x164            = ConvertEndianess( fxSettings[ i ]->field38_0x164 );
			fxSettingsOut[ i ]->field39_0x168            = ConvertEndianess( fxSettings[ i ]->field39_0x168 );
			fxSettingsOut[ i ]->field40_0x16c            = ConvertEndianess( fxSettings[ i ]->field40_0x16c );
			fxSettingsOut[ i ]->field41_0x170            = ConvertEndianess( fxSettings[ i ]->field41_0x170 );
			fxSettingsOut[ i ]->field42_0x174            = ConvertEndianess( fxSettings[ i ]->field42_0x174 );
		}

		mergeTo.WriteToFile( "./GameTimeFXSettings.trb" );

		return TTRUE;
	}

	return TFALSE;
}

TBOOL GameTimeFXResourceView::CanSave()
{
	return TFALSE;
}

TBOOL GameTimeFXResourceView::OnSave( PTRB* pOutTRB )
{
	return TFALSE;
}

void GameTimeFXResourceView::OnDestroy()
{
}

void GameTimeFXResourceView::OnRender( TFLOAT flDeltaTime )
{
	if ( ImGui::BeginTabBar( "MyTabBar" ) )
	{
		if ( ImGui::BeginTabItem( "FX" ) )
		{
			for ( TINT i = 0; i < 0; i++ )
			{
				if ( ImGui::CollapsingHeader( m_pSettings[ i ].m_pchName ) )
				{

#define FIELD1( name )     \
	ImGui::Text( #name ); \
	ImGui::InputFloat( #name, &( m_pSettings[ i ].name ) )

#define FIELD4( name )     \
	ImGui::Text( #name ); \
	ImGui::InputFloat4( #name, ( (float*)&m_pSettings[ i ].name ) )

					FIELD4( m_vecLightDir );
					FIELD4( field1_0x10 );
					FIELD4( m_oShadowColour );
					FIELD4( m_oAmbientColour );
					FIELD4( m_vecFogColor );
					FIELD4( field5_0x50 );
					FIELD4( field6_0x60 );
					FIELD4( field7_0x70 );
					FIELD4( field8_0x80 );
					FIELD4( field9_0x90 );
					FIELD4( field10_0xa0 );
					FIELD4( field11_0xb0 );
					FIELD4( field12_0xc0 );
					FIELD4( field13_0xd0 );
					FIELD4( field14_0xe0 );
					FIELD4( field15_0xf0 );
					FIELD4( field16_0x100 );
					FIELD1( field17_0x110 );
					FIELD1( field19_0x118 );
					FIELD1( m_fFogStart );
					FIELD1( m_fFogEnd );
					FIELD1( field22_0x124 );
					FIELD1( m_fShadowAmbientProgress );
					FIELD1( m_flInstanceShading );
					FIELD1( field25_0x130 );
					FIELD1( field26_0x134 );
					FIELD1( field27_0x138 );
					FIELD1( field28_0x13c );
					FIELD1( m_fStarsOpacity );
					FIELD1( field30_0x144 );
					FIELD1( field31_0x148 );
					FIELD1( field32_0x14c );
					FIELD1( m_fDarkeningFactor );
					FIELD1( m_fLightIntensity );
					FIELD1( field35_0x158 );
					FIELD1( field36_0x15c );
					FIELD1( field37_0x160 );
					FIELD1( field38_0x164 );
					FIELD1( field39_0x168 );
					FIELD1( field40_0x16c );
					FIELD1( field41_0x170 );
					FIELD1( field42_0x174 );
				}
			}

			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}
}
