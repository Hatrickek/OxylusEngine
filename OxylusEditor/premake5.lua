project "OxylusEditor"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++20"
	staticruntime "on"
	exceptionhandling "off"
	editandcontinue "Off"

	targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

	files
	{
		"src/**.h",
		"src/**.cpp"
	}

	includedirs
	{
		"src",
		"%{IncludeDir.Oxylus}",
		"%{IncludeDir.vendor}",
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
		"Oxylus"
	}

	filter "system:windows"
		systemversion "latest"

	filter "configurations:Debug"
		defines {"OX_DEBUG", "_DEBUG", "TRACY_ENABLE"}
		runtime "Debug"
		symbols "on"
		
	filter "configurations:Release"
		defines {"OX_RELEASE", "NDEBUG", "TRACY_ENABLE"}
		runtime "Release"
		optimize "on"

	filter "configurations:Dist"
		defines {"OX_DIST", "NDEBUG"}
		runtime "Release"
		optimize "on"
