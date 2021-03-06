//
// Copyright (c) 2019 The WebGLNativePorts Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// DeviceDawn.cpp: Implements accessing functions to the graphics API of Dawn.

#include <iostream>
#include <string>
#include <vector>

#include "../Aquarium.h"
#include "BufferDawn.h"
#include "ContextDawn.h"
#include "FishModelDawn.h"
#include "GenericModelDawn.h"
#include "InnerModelDawn.h"
#include "OutsideModelDawn.h"
#include "ProgramDawn.h"
#include "SeaweedModelDawn.h"
#include "TextureDawn.h"

#include <dawn/dawn.h>
#include <dawn/dawn_wsi.h>
#include <dawn/dawncpp.h>
#include <shaderc/shaderc.hpp>
#include "common/Constants.h"
#include "utils/BackendBinding.h"
#include "utils/ComboRenderPipelineDescriptor.h"
#include "../ASSERT.h"

#include <array>
#include <cstring>

void PrintDeviceError(const char* message, dawn::CallbackUserdata) {
    std::cout << "Device error: " << message << std::endl;
}

ContextDawn::ContextDawn()
    : mWindow(nullptr),
      pass(nullptr),
      device(nullptr),
      queue(nullptr),
      swapchain(nullptr),
      commandEncoder(nullptr),
      mBackbuffer(nullptr),
      mDepthStencilView(nullptr),
      mPipeline(nullptr),
      mBindGroup(nullptr),
      mPreferredSwapChainFormat(dawn::TextureFormat::R8G8B8A8Unorm),
      renderPassDescriptor(nullptr)
{
}

ContextDawn::~ContextDawn() {}

bool ContextDawn::createContext(std::string backend, bool enableMSAA)
{
    // TODO(yizhou): MSAA of Dawn is not supported yet.

    if (backend == "dawn_d3d12")
    {
        backendType = dawn_native::BackendType::D3D12;
    }
    else if (backend == "dawn_vulkan")
    {
        backendType = dawn_native::BackendType::Vulkan;
    }
    else if (backend == "dawn_metal")
    {
        backendType = dawn_native::BackendType::Metal;
    }
    else if (backend == "dawn_opengl")
    {
        backendType = dawn_native::BackendType::OpenGL;
    }

    // initialise GLFW
    if (!glfwInit())
    {
        std::cout << "Failed to initialise GLFW" << std::endl;
        return false;
    }

     utils::SetupGLFWWindowHintsForBackend(backendType);
    // set full screen
    //glfwWindowHint(GLFW_DECORATED, GL_FALSE);

    GLFWmonitor *pMonitor = glfwGetPrimaryMonitor();
    const GLFWvidmode *mode = glfwGetVideoMode(pMonitor);
    mClientWidth = mode->width;
    mClientHeight = mode->height;
    // If we show the window bar on the top, max width and height will be 1916 x 1053.
    // Use a window mode currently
    //mClientWidth  = 1024;
    //mClientHeight = 768;
    // Minus the height of title bar
    mClientHeight = 1060;

    mWindow = glfwCreateWindow(mClientWidth, mClientHeight, "Aquarium", NULL, NULL);
    if (mWindow == NULL)
    {
        std::cout << "Failed to open GLFW window." << std::endl;
        glfwTerminate();
        return false;
    }

	instance = std::make_unique<dawn_native::Instance>();
    utils::DiscoverAdapter(instance.get(), mWindow, backendType);

    // Get an adapter for the backend to use, and create the device.
    dawn_native::Adapter backendAdapter;
    {
        for (auto &adapter : instance->GetAdapters())
		{
            if (adapter.GetBackendType() == backendType)
			{
                backendAdapter = adapter;
                break;
			}
		}
    }

    dawnDevice backendDevice   = backendAdapter.CreateDevice();
    dawnProcTable backendProcs = dawn_native::GetProcs();

	utils::BackendBinding *binding = utils::CreateBinding(backendType, mWindow, backendDevice);
    if (binding == nullptr)
    {
        return false;
    }

    dawnSetProcs(&backendProcs);
    backendProcs.deviceSetErrorCallback(backendDevice, PrintDeviceError, 0);
    device =  dawn::Device::Acquire(backendDevice);

    queue = device.CreateQueue();
    dawn::SwapChainDescriptor swapChainDesc;
    swapChainDesc.implementation = binding->GetSwapChainImplementation();
    swapchain = device.CreateSwapChain(&swapChainDesc);

    mPreferredSwapChainFormat = static_cast<dawn::TextureFormat>(binding->GetPreferredSwapChainTextureFormat());
    swapchain.Configure(mPreferredSwapChainFormat,
        dawn::TextureUsageBit::OutputAttachment, mClientWidth, mClientHeight);

    mDepthStencilView = createDepthStencilView();

    return true;
}

Texture *ContextDawn::createTexture(std::string name, std::string url)
{
    Texture *texture = new TextureDawn(this, name, url);
    texture->loadTexture();
    return texture;
}

Texture *ContextDawn::createTexture(std::string name, const std::vector<std::string> &urls)
{
    Texture *texture = new TextureDawn(this, name, urls);
    texture->loadTexture();
    return texture;
}

dawn::Texture ContextDawn::createTexture(const dawn::TextureDescriptor & descriptor) const
{
    return device.CreateTexture(&descriptor);
}

dawn::Sampler ContextDawn::createSampler(const dawn::SamplerDescriptor & descriptor) const
{
    return device.CreateSampler(&descriptor);
}

dawn::Buffer ContextDawn::createBufferFromData(const void * pixels, int size, dawn::BufferUsageBit usage) const
{
    return utils::CreateBufferFromData(device, pixels, size, usage);
}

dawn::BufferCopyView ContextDawn::createBufferCopyView(const dawn::Buffer& buffer,
    uint32_t offset,
    uint32_t rowPitch,
    uint32_t imageHeight) const {

    return utils::CreateBufferCopyView(buffer, offset, rowPitch, imageHeight);
}

dawn::TextureCopyView ContextDawn::createTextureCopyView(dawn::Texture texture,
                                                         uint32_t level,
                                                         uint32_t slice,
                                                         dawn::Origin3D origin)
{

    return utils::CreateTextureCopyView(texture, level, slice, origin);
}

dawn::CommandBuffer ContextDawn::copyBufferToTexture(const dawn::BufferCopyView &bufferCopyView, const dawn::TextureCopyView &textureCopyView, const dawn::Extent3D& ext3D) const
{
    dawn::CommandEncoder encoder = device.CreateCommandEncoder();
    encoder.CopyBufferToTexture(&bufferCopyView, &textureCopyView, &ext3D);
    dawn::CommandBuffer copy = encoder.Finish();
    return copy;
}

void ContextDawn::submit(int numCommands, const dawn::CommandBuffer& commands) const
{
    queue.Submit(numCommands, &commands);
}

dawn::ShaderModule ContextDawn::createShaderModule(dawn::ShaderStage stage,
                                                   const std::string &str) const
{
    return utils::CreateShaderModule(device, stage, str.c_str());
}

dawn::BindGroupLayout ContextDawn::MakeBindGroupLayout(
    std::initializer_list<dawn::BindGroupLayoutBinding> bindingsInitializer) const {
    dawn::ShaderStageBit kNoStages{};

    return utils::MakeBindGroupLayout(device, bindingsInitializer);
}

dawn::PipelineLayout ContextDawn::MakeBasicPipelineLayout(
    std::vector<dawn::BindGroupLayout> bindingsInitializer) const {
    dawn::PipelineLayoutDescriptor descriptor;

    descriptor.bindGroupLayoutCount = static_cast<uint32_t>(bindingsInitializer.size());
    descriptor.bindGroupLayouts = bindingsInitializer.data();
    
    return device.CreatePipelineLayout(&descriptor);
}

dawn::InputState ContextDawn::createInputState(std::initializer_list<Attribute> attributeInitilizer, std::initializer_list<Input> inputInitilizer) const
{
    dawn::InputStateBuilder inputStateBuilder = device.CreateInputStateBuilder();

    for (auto& attribute : attributeInitilizer)
    {
        dawn::VertexAttributeDescriptor attrib;
        attrib.shaderLocation = attribute.shaderLocation;
        attrib.inputSlot      = attribute.bindingSlot;
        attrib.offset         = attribute.offset;
        attrib.format         = attribute.format;
        inputStateBuilder.SetAttribute(&attrib);
    }

    for (auto &input : inputInitilizer)
    {
        dawn::VertexInputDescriptor in;
        in.inputSlot = input.bindingSlot;
        in.stride    = input.stride;
        in.stepMode  = input.stepMode;
        inputStateBuilder.SetInput(&in);
    }

    return inputStateBuilder.GetResult();
}

dawn::RenderPipeline ContextDawn::createRenderPipeline(dawn::PipelineLayout pipelineLayout, ProgramDawn * programDawn, dawn::InputState inputState, bool enableBlend) const
{
    const dawn::ShaderModule& vsModule = programDawn->getVSModule();
    const dawn::ShaderModule& fsModule = programDawn->getFSModule();

    dawn::PipelineStageDescriptor cVertexStage;
    cVertexStage.entryPoint = "main";
    cVertexStage.module = vsModule;

    dawn::PipelineStageDescriptor cFragmentStage;
    cFragmentStage.entryPoint = "main";
    cFragmentStage.module = fsModule;

    dawn::BlendDescriptor blendDescriptor;
    blendDescriptor.operation = dawn::BlendOperation::Add;
    if (enableBlend)
    {
        blendDescriptor.srcFactor = dawn::BlendFactor::SrcAlpha;
        blendDescriptor.dstFactor = dawn::BlendFactor::OneMinusSrcAlpha;
    } else
    {
        blendDescriptor.srcFactor = dawn::BlendFactor::One;
        blendDescriptor.dstFactor = dawn::BlendFactor::Zero;
    }

	dawn::ColorStateDescriptor ColorStateDescriptor;
    ColorStateDescriptor.colorBlend = blendDescriptor;
    ColorStateDescriptor.alphaBlend = blendDescriptor;
    ColorStateDescriptor.colorWriteMask = dawn::ColorWriteMask::All;

	// test
    utils::ComboRenderPipelineDescriptor descriptor(device);
    descriptor.layout                               = pipelineLayout;
    descriptor.cVertexStage.module                  = vsModule;
    descriptor.cFragmentStage.module                = fsModule;
    descriptor.inputState                           = inputState;
    descriptor.depthStencilState                    = &descriptor.cDepthStencilState;
    descriptor.cDepthStencilState.format            = dawn::TextureFormat::D32FloatS8Uint;
    descriptor.cColorStates[0]                      = &ColorStateDescriptor;
    descriptor.cColorStates[0]->format              = mPreferredSwapChainFormat;
    descriptor.cDepthStencilState.depthWriteEnabled = true;
    descriptor.cDepthStencilState.depthCompare      = dawn::CompareFunction::Less;
    descriptor.primitiveTopology                    = dawn::PrimitiveTopology::TriangleList;
    descriptor.indexFormat                          = dawn::IndexFormat::Uint16;
    descriptor.sampleCount                          = 1;

    dawn::RenderPipeline pipeline = device.CreateRenderPipeline(&descriptor);

    return pipeline;
}

dawn::TextureView ContextDawn::createDepthStencilView() const
{
    dawn::TextureDescriptor descriptor;
    descriptor.dimension = dawn::TextureDimension::e2D;
    descriptor.size.width = mClientWidth;
    descriptor.size.height = mClientHeight;
    descriptor.size.depth = 1;
    descriptor.arrayLayerCount = 1;
    descriptor.sampleCount = 1;
    descriptor.format = dawn::TextureFormat::D32FloatS8Uint;
    descriptor.mipLevelCount   = 1;
    descriptor.usage = dawn::TextureUsageBit::OutputAttachment;
    auto depthStencilTexture = device.CreateTexture(&descriptor);
    return depthStencilTexture.CreateDefaultTextureView();
}

dawn::Buffer ContextDawn::createBuffer(uint32_t size, dawn::BufferUsageBit bit) const
{
    dawn::BufferDescriptor descriptor;
    descriptor.size = size;
    descriptor.usage = bit;

    dawn::Buffer buffer = device.CreateBuffer(&descriptor);
    return buffer;
}

void ContextDawn::setBufferData(const dawn::Buffer& buffer, uint32_t start, uint32_t size, const void* pixels) const
{
    buffer.SetSubData(start, size, reinterpret_cast<const uint8_t*>(pixels));
}

dawn::BindGroup ContextDawn::makeBindGroup(
    const dawn::BindGroupLayout &layout,
    std::initializer_list<utils::BindingInitializationHelper> bindingsInitializer) const
{
    return utils::MakeBindGroup(device, layout, bindingsInitializer);
}

void ContextDawn::initGeneralResources(Aquarium* aquarium)
{
    // initilize general uniform buffers
    groupLayoutGeneral = MakeBindGroupLayout({
        { 0, dawn::ShaderStageBit::Fragment, dawn::BindingType::UniformBuffer },
        { 1, dawn::ShaderStageBit::Fragment, dawn::BindingType::UniformBuffer },
    });

    lightBuffer = createBufferFromData(&aquarium->lightUniforms, sizeof(aquarium->lightUniforms), dawn::BufferUsageBit::TransferDst | dawn::BufferUsageBit::Uniform);
    fogBuffer = createBufferFromData(&aquarium->fogUniforms, sizeof(aquarium->fogUniforms), dawn::BufferUsageBit::TransferDst | dawn::BufferUsageBit::Uniform);

    bindGroupGeneral = makeBindGroup(groupLayoutGeneral, {
        { 0, lightBuffer, 0, sizeof(aquarium->lightUniforms) },
        { 1, fogBuffer , 0, sizeof(aquarium->fogUniforms) }
    });

    setBufferData(lightBuffer, 0, sizeof(LightUniforms), &aquarium->lightUniforms);
    setBufferData(fogBuffer, 0, sizeof(FogUniforms), &aquarium->fogUniforms);

    // initilize world uniform buffers
    groupLayoutWorld = MakeBindGroupLayout({ 
        { 0, dawn::ShaderStageBit::Vertex, dawn::BindingType::UniformBuffer },
    });

    lightWorldPositionBuffer = createBufferFromData(&aquarium->lightWorldPositionUniform, sizeof(aquarium->lightWorldPositionUniform), dawn::BufferUsageBit::TransferDst | dawn::BufferUsageBit::Uniform);
    
    bindGroupWorld = makeBindGroup(groupLayoutWorld, {
        {0, lightWorldPositionBuffer, 0, sizeof(aquarium->lightWorldPositionUniform) },
    });

    setBufferData(lightWorldPositionBuffer, 0, sizeof(LightWorldPositionUniform), &aquarium->lightWorldPositionUniform);
}

void ContextDawn::updateWorldlUniforms(Aquarium* aquarium)
{
    setBufferData(lightWorldPositionBuffer, 0, sizeof(LightWorldPositionUniform), &aquarium->lightWorldPositionUniform);
}

Buffer *ContextDawn::createBuffer(int numComponents, const std::vector<float> &buf, bool isIndex)
{
    Buffer *buffer = new BufferDawn(this, static_cast<int>(buf.size()), numComponents, buf, isIndex);
    return buffer;
}

Buffer *ContextDawn::createBuffer(int numComponents,
                                 const std::vector<unsigned short> &buf,
                                 bool isIndex)
{
    Buffer *buffer = new BufferDawn(this, static_cast<int>(buf.size()), numComponents, buf, isIndex);
    return buffer;
}

Program *ContextDawn::createProgram(std::string vId, std::string fId)
{
    ProgramDawn* program = new ProgramDawn(this, vId, fId);
    program->loadProgram();

    return program;
}

void ContextDawn::setWindowTitle(const std::string &text)
{
    glfwSetWindowTitle(mWindow, text.c_str());
}

bool ContextDawn::ShouldQuit() {
    return glfwWindowShouldClose(mWindow);
}

void ContextDawn::KeyBoardQuit() {
    if (glfwGetKey(mWindow, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(mWindow, GL_TRUE);
}

// Submit commands of the frame
void ContextDawn::DoFlush() {

    pass.EndPass();
    dawn::CommandBuffer cmd = commandEncoder.Finish();
    queue.Submit(1, &cmd);

    swapchain.Present(mBackbuffer);

    glfwPollEvents();
}

void ContextDawn::Terminate() {
    glfwTerminate();
}

// Update backbuffer and renderPassDescriptor
void ContextDawn::preFrame()
{
    GetNextRenderPassDescriptor(&mBackbuffer, &renderPassDescriptor);
    commandEncoder =
        device.CreateCommandEncoder();
    pass           = commandEncoder.BeginRenderPass(renderPassDescriptor);
}

void ContextDawn::GetNextRenderPassDescriptor(
    dawn::Texture* backbuffer,
    dawn::RenderPassDescriptor* info) const{

    *backbuffer = swapchain.GetNextTexture();
    auto backbufferView = backbuffer->CreateDefaultTextureView();

    dawn::RenderPassColorAttachmentDescriptor colorAttachment;
    colorAttachment.attachment = backbufferView;
    colorAttachment.resolveTarget = nullptr;
    colorAttachment.clearColor = { 0.0f, 0.8f, 1.0f, 0.0f };
    colorAttachment.loadOp = dawn::LoadOp::Clear;
    colorAttachment.storeOp = dawn::StoreOp::Store;

    dawn::RenderPassDepthStencilAttachmentDescriptor depthStencilAttachment;
    depthStencilAttachment.attachment = mDepthStencilView;
    depthStencilAttachment.depthLoadOp = dawn::LoadOp::Clear;
    depthStencilAttachment.stencilLoadOp = dawn::LoadOp::Clear;
    depthStencilAttachment.clearDepth = 1.0f;
    depthStencilAttachment.clearStencil = 0;
    depthStencilAttachment.depthStoreOp = dawn::StoreOp::Store;
    depthStencilAttachment.stencilStoreOp = dawn::StoreOp::Store;

    dawn::RenderPassDescriptor descriptorClear = device.CreateRenderPassDescriptorBuilder()
        .SetColorAttachments(1, &colorAttachment)
        .SetDepthStencilAttachment(&depthStencilAttachment)
        .GetResult();

    *info = device.CreateRenderPassDescriptorBuilder()
         .SetColorAttachments(1, &colorAttachment)
         .SetDepthStencilAttachment(&depthStencilAttachment)
         .GetResult();
}

Model * ContextDawn::createModel(Aquarium* aquarium, MODELGROUP type, MODELNAME name, bool blend)
{
    Model *model;
    switch (type)
    {
    case MODELGROUP::FISH:
        model = new FishModelDawn(this, aquarium, type, name, blend);
        break;
    case MODELGROUP::GENERIC:
        model = new GenericModelDawn(this, aquarium, type, name, blend);
        break;
    case MODELGROUP::INNER:
        model = new InnerModelDawn(this, aquarium, type, name, blend);
        break;
    case MODELGROUP::SEAWEED:
        model = new SeaweedModelDawn(this, aquarium, type, name, blend);
        break;
    case MODELGROUP::OUTSIDE:
        model = new OutsideModelDawn(this, aquarium, type, name, blend);
        break;
    default:
        model = nullptr;
        std::cout << "can not create model type" << std::endl;
    }

    return model;
}

