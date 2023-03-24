include "./vendor/premake/premake_customization/solution_items.lua"
include "Dependencies.lua"

workspace "Oxylus"
	architecture "x86_64"
	startproject "OxylusEditor"

	configurations
	{
		"Debug",
		"Release",
		"Dist"
	}

	solution_items
	{
		".editorconfig"
	}

	flags
	{
		"MultiProcessorCompile"
	}

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

include "Oxylus"
include "OxylusEditor"
