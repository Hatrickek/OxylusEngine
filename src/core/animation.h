#pragma once

#include <vector>
#include <map>
#include <glm/glm.hpp>
#include <assimp/scene.h>
#include <functional>
#include <core/bone.h>
#include <core/animdata.h>
#include <core/model.h>
#include <assimp/Importer.hpp>
#include "log.h"

namespace FlatEngine {
	struct AssimpNodeData {
		glm::mat4 transformation;
		std::string name;
		int childrenCount;
		std::vector<AssimpNodeData> children;
	};

	class Animation {
	public:
		Animation(const std::string& animationPath, Model* model) {
			Assimp::Importer importer;
			const aiScene* scene = importer.ReadFile(animationPath, aiProcess_Triangulate | aiProcess_JoinIdenticalVertices);
			if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) // if is Not Zero
			{
				FE_LOG_ERROR("ERROR::ASSIMP:: {}", importer.GetErrorString());
				return;
			}
			assert(scene && scene->mRootNode);
			if(!scene->HasAnimations()){
				FE_LOG_WARN("ERROR::ASSIMP:: Number of animations: {}", scene->mNumAnimations);
			}
			else {
				aiAnimation* animation = scene->mAnimations[0];
				m_Duration = animation->mDuration;
				m_TicksPerSecond = animation->mTicksPerSecond;
				aiMatrix4x4 globalTransformation = scene->mRootNode->mTransformation;
				globalTransformation = globalTransformation.Inverse();
				ReadHeirarchyData(m_RootNode, scene->mRootNode);
				ReadMissingBones(animation, *model);	
			}
		}

		~Animation() {
		}

		Bone* FindBone(const std::string& name) {
			auto iter = std::find_if(m_Bones.begin(), m_Bones.end(),[&](const Bone& Bone)
			                         {
				                         return Bone.GetBoneName() == name;
			                         }
			);
			if (iter == m_Bones.end()) return nullptr;
			return &(*iter);
		}


		float GetTicksPerSecond() { return m_TicksPerSecond; }
		float GetDuration() { return m_Duration; }
		const AssimpNodeData& GetRootNode() { return m_RootNode; }

		const std::map<std::string, BoneInfo>& GetBoneIDMap() {
			return m_BoneInfoMap;
		}

	private:
		void ReadMissingBones(const aiAnimation* animation, Model& model) {
			int size = animation->mNumChannels;

			auto& boneInfoMap = model.GetBoneInfoMap(); //getting m_BoneInfoMap from Model class
			int& boneCount = model.GetBoneCount(); //getting the m_BoneCounter from Model class

			//reading channels(bones engaged in an animation and their keyframes)
			for (int i = 0; i < size; i++) {
				auto channel = animation->mChannels[i];
				std::string boneName = channel->mNodeName.data;

				if (!boneInfoMap.contains(boneName)) {
					boneInfoMap[boneName].id = boneCount;
					boneCount++;
				}
				m_Bones.push_back(Bone(channel->mNodeName.data,
				                       boneInfoMap[channel->mNodeName.data].id, channel));
			}

			m_BoneInfoMap = boneInfoMap;
		}

		void ReadHeirarchyData(AssimpNodeData& dest, const aiNode* src) {
			assert(src);

			dest.name = src->mName.data;
			dest.transformation = AssimpGLMHelpers::ConvertMatrixToGLMFormat(src->mTransformation);
			dest.childrenCount = src->mNumChildren;

			for (int i = 0; i < src->mNumChildren; i++) {
				AssimpNodeData newData;
				ReadHeirarchyData(newData, src->mChildren[i]);
				dest.children.push_back(newData);
			}
		}

		float m_Duration;
		int m_TicksPerSecond;
		std::vector<Bone> m_Bones;
		AssimpNodeData m_RootNode;
		std::map<std::string, BoneInfo> m_BoneInfoMap;
	};
	
}
