#pragma once
#include <Render/TMaterial.h>
#include <Toshi/T2SharedPtr.h>

namespace Resource
{

class Material
	: public Toshi::TMaterial
{
public:
	TDECLARE_CLASS( Material, Toshi::TMaterial );
};

} // namespace Resource

