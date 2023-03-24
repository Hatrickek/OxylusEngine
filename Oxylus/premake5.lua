project "Oxylus"
	kind "StaticLib"
	language "C++"
	cppdialect "C++20"
	staticruntime "on"
	exceptionhandling "off"
	editandcontinue "Off"

	targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

	pchheader "src/oxpch.h"
	pchsource "src/oxpch.cpp"

	files
	{
		"src/**.h",
		"src/**.cpp",

		"vendor/glm/glm/**.hpp",
		"vendor/glm/glm/**.inl",

		"vendor/miniaudio/**.h",

		"vendor/ImGuizmo/ImGuizmo.h",
		"vendor/ImGuizmo/ImGuizmo.cpp",

		"vendor/ImGui/*.h",
		"vendor/ImGui/*.cpp",
		"vendor/ImGui/misc/cpp/*.cpp",
		"vendor/ImGui/misc/cpp/*.h",
		"vendor/ImGui/backends/imgui_impl_glfw.h",
		"vendor/ImGui/backends/imgui_impl_glfw.cpp",
		"vendor/ImGui/backends/imgui_impl_vulkan.h",
		"vendor/ImGui/backends/imgui_impl_vulkan.cpp",

		"vendor/rapidyaml/src/ryml.cpp",

		"vendor/tracy/public/TracyClient.cpp"
	}

	removefiles
	{
		
	}

	defines
	{
		"VULKAN_HPP_NO_EXCEPTIONS",
		"VULKAN_HPP_NO_SPACESHIP_OPERATOR",
		"VULKAN_HPP_NO_TO_STRING",
		"_CRT_SECURE_NO_WARNINGS",
		"GLFW_INCLUDE_NONE",
		"_SILENCE_ALL_CXX20_DEPRECATION_WARNINGS",
		"PX_PHYSX_STATIC_LIB",
		"SPDLOG_NO_EXCEPTIONS"
	}

	includedirs
	{
		"src",
		"vendor/",
		"%{IncludeDir.GLFW}",
		"%{IncludeDir.ImGui}",
		"%{IncludeDir.glm}",
		"%{IncludeDir.entt}",
		"%{IncludeDir.rapidyaml}",
		"%{IncludeDir.ImGuizmo}",
		"%{IncludeDir.VulkanSDK}",
		"%{IncludeDir.PhysX}",
		"%{IncludeDir.tinygltf}",
		"%{IncludeDir.ktx}",
		"%{IncludeDir.miniaudio}",
		"%{IncludeDir.tracy}",
	}

	links
	{
		"%{Library.GLFW}",
		"%{Library.ktx}",
		"%{Library.Vulkan}",
	}

	filter "files:vendor/ImGuizmo/**.cpp"
	flags { "NoPCH" }
	filter "files:vendor/ImGui/**.cpp"
	flags { "NoPCH" }
	filter "files:vendor/tracy/public/TracyClient.cpp"
	flags { "NoPCH" }
	filter "files:vendor/rapidyaml/src/ryml.cpp"
	flags { "NoPCH" }

	filter "system:windows"
		systemversion "latest"

		defines
		{
		}

	filter "configurations:Debug"
		defines {"OX_DEBUG", "_DEBUG", "TRACY_ENABLE"}
		runtime "Debug"
		symbols "on"

		local physxDebugDir =  "../Oxylus/vendor/PhysX/bin/debug/"

		links
		{
			"%{Library.ShaderC_Debug}",

			physxDebugDir .. "PhysX_static_64.lib",
			physxDebugDir .. "PhysXFoundation_static_64.lib",
			physxDebugDir .. "PhysXCommon_static_64.lib",
			physxDebugDir .. "PhysXExtensions_static_64.lib",
			physxDebugDir .. "PhysXPvdSDK_static_64.lib",
			physxDebugDir .. "PhysXVehicle_static_64.lib",
			physxDebugDir .. "PhysXVehicle2_static_64.lib",
			physxDebugDir .. "PhysXCharacterKinematic_static_64.lib",
		}

	local physxReleaseDir =  "../Oxylus/vendor/PhysX/bin/release/"

	filter "configurations:Release"
		defines {"OX_RELEASE", "NDEBUG", "TRACY_ENABLE"}
		runtime "Release"
		optimize "on"


		links
		{
			"%{Library.ShaderC_Release}",

			physxReleaseDir .. "PhysX_static_64.lib",
			physxReleaseDir .. "PhysXFoundation_static_64.lib",
			physxReleaseDir .. "PhysXCommon_static_64.lib",
			physxReleaseDir .. "PhysXExtensions_static_64.lib",
			physxReleaseDir .. "PhysXPvdSDK_static_64.lib",
			physxReleaseDir .. "PhysXVehicle_static_64.lib",
			physxReleaseDir .. "PhysXVehicle2_static_64.lib",
			physxReleaseDir .. "PhysXCharacterKinematic_static_64.lib",
		}

	filter "configurations:Dist"
		defines {"OX_DIST", "NDEBUG"}
		runtime "Release"
		optimize "on"

		links
		{
			"%{Library.ShaderC_Release}",

			physxReleaseDir .. "PhysX_static_64.lib",
			physxReleaseDir .. "PhysXFoundation_static_64.lib",
			physxReleaseDir .. "PhysXCommon_static_64.lib",
			physxReleaseDir .. "PhysXExtensions_static_64.lib",
			physxReleaseDir .. "PhysXPvdSDK_static_64.lib",
			physxReleaseDir .. "PhysXVehicle_static_64.lib",
			physxReleaseDir .. "PhysXVehicle2_static_64.lib",
			physxReleaseDir .. "PhysXCharacterKinematic_static_64.lib",
		}
