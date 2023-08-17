#include "Texture.hpp"

uint32_t Texture::fullResourceIndex() const { return gpuTexture.resourceIndex; }
/*
    WARNING: ATM ALL SINGLE MIPS ONLY GET STORAGE RESOURCES!
    IF SINGLE MIPS ARE NEEDED FOR SAMPLED RESOURCES, USE fullResouceIndex() INSTEAD!
*/
uint32_t Texture::mipResourceIndex(uint32_t level) const { return gpuTexture._mipResourceIndices[level]; }

// TODO: own textureView abstraction
VkImageView Texture::fullResourceView() const { return gpuTexture.fullImageView; }
VkImageView Texture::mipResourceView(uint32_t level) const { return gpuTexture._mipImageViews[level]; }