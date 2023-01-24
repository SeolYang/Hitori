#pragma once
#include <PCH.h>

namespace sy::render
{
	class Mesh;
	class Material;
}

namespace sy::component
{
	struct StaticMeshComponent : ecs::Component
	{
		Handle<render::Mesh> Mesh;
		Handle<render::Material> Material;
	};
}
