// DeviceDawn.cpp: Implements accessing functions to the graphics API of Dawn.

#include <iostream>
#include <string>
#include <vector>

#include "../Aquarium.h"
#include "ContextDawn.h"

#include "common/Assert.h"
#include "common/Platform.h"

#include <dawn/dawn.h>
#include <dawn/dawn_wsi.h>
#include <dawn/dawncpp.h>
#include <dawn_native/DawnNative.h>
#include "utils/BackendBinding.h"
#include "utils/ComboRenderPipelineDescriptor.h"
#include "utils/DawnHelpers.h"
#include "utils/SystemUtils.h"
#include "common/Constants.h"
#include "shaderc/shaderc.h"

#include "BufferDawn.h"
#include "TextureDawn.h"
#include "ProgramDawn.h"
#include "FishModelDawn.h"
#include "GenericModelDawn.h"
#include "InnerModelDawn.h"
#include "OutsideModelDawn.h"
#include "SeaweedModelDawn.h"

#include <cstring>

void PrintDeviceError(const char* message, dawn::CallbackUserdata) {
    std::cout << "Device error: " << message << std::endl;
}

ContextDawn::ContextDawn() {}

ContextDawn::~ContextDawn() {}

bool ContextDawn::createContext()
{
    // TODO(yizhou) : initilize dawn dynamic backend
    utils::BackendType backendType = utils::BackendType::D3D12;
    utils::BackendBinding* binding = utils::CreateBinding(backendType);
    if (binding == nullptr) {
        return false;
    }

    // initialise GLFW
    if (!glfwInit())
    {
        std::cout << "Failed to initialise GLFW" << std::endl;
        return false;
    }

    binding->SetupGLFWWindowHints();
    // set full screen
    //glfwWindowHint(GLFW_DECORATED, GL_FALSE);

    GLFWmonitor *pMonitor = glfwGetPrimaryMonitor();
    const GLFWvidmode *mode = glfwGetVideoMode(pMonitor);
    //mClientWidth = mode->width;
    //mClientHeight = mode->height;
    // If we show the window bar on the top, max width and height will be 1916 x 1053.
    // Use a window mode currently
    mClientWidth = 1024;
    mClientHeight = 768;

    mWindow = glfwCreateWindow(mClientWidth, mClientHeight, "Aquarium", NULL, NULL);
    if (mWindow == NULL)
    {
        std::cout << "Failed to open GLFW window." << std::endl;
        glfwTerminate();
        return false;
    }

    binding->SetWindow(mWindow);

    dawnDevice backendDevice = binding->CreateDevice();
    dawnProcTable backendProcs = dawn_native::GetProcs();

    dawnSetProcs(&backendProcs);
    backendProcs.deviceSetErrorCallback(backendDevice, PrintDeviceError, 0);
    device =  dawn::Device::Acquire(backendDevice);

    queue = device.CreateQueue();
    swapchain = device.CreateSwapChainBuilder()
        .SetImplementation(binding->GetSwapChainImplementation())
        .GetResult();

    preferredSwapChainFormat = static_cast<dawn::TextureFormat>(binding->GetPreferredSwapChainTextureFormat());
    swapchain.Configure(preferredSwapChainFormat,
        dawn::TextureUsageBit::OutputAttachment, mClientWidth, mClientHeight);

    depthStencilView = createDepthStencilView();

    dawn::BlendDescriptor blendDescriptor;
    blendDescriptor.operation = dawn::BlendOperation::Add;
    blendDescriptor.srcFactor = dawn::BlendFactor::SrcAlpha;
    blendDescriptor.dstFactor = dawn::BlendFactor::OneMinusSrcAlpha;

    blendState = device.CreateBlendStateBuilder()
        .SetBlendEnabled(true)
        .SetColorBlend(&blendDescriptor)
        .SetAlphaBlend(&blendDescriptor)
        .GetResult();

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

void ContextDawn::generateTexture(unsigned int * texture)
{
}

void ContextDawn::bindTexture(unsigned int target, unsigned int texture)
{
}

void ContextDawn::deleteTexture(unsigned int * texture)
{
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
    dawn::BufferDescriptor descriptor;
    descriptor.size = size;
    descriptor.usage = usage | dawn::BufferUsageBit::TransferDst;

    dawn::Buffer buffer = device.CreateBuffer(&descriptor);
    buffer.SetSubData(0, size, reinterpret_cast<const uint8_t*>(pixels));
    return buffer;
}

dawn::BufferCopyView ContextDawn::createBufferCopyView(const dawn::Buffer& buffer,
    uint32_t offset,
    uint32_t rowPitch,
    uint32_t imageHeight) const {
    dawn::BufferCopyView bufferCopyView;
    bufferCopyView.buffer = buffer;
    bufferCopyView.offset = offset;
    bufferCopyView.rowPitch = rowPitch;
    bufferCopyView.imageHeight = imageHeight;

    return bufferCopyView;
}

dawn::TextureCopyView ContextDawn::CreateTextureCopyView(const dawn::Texture& texture,
    uint32_t level,
    uint32_t slice,
    dawn::Origin3D origin,
    dawn::TextureAspect aspect) const {
    dawn::TextureCopyView textureCopyView;
    textureCopyView.texture = texture;
    textureCopyView.level = level;
    textureCopyView.slice = slice;
    textureCopyView.origin = origin;

    return textureCopyView;
}

dawn::CommandBuffer ContextDawn::copyBufferToTexture(const dawn::BufferCopyView &bufferCopyView, const dawn::TextureCopyView &textureCopyView, const dawn::Extent3D& ext3D) const
{
    return  device.CreateCommandBufferBuilder()
        .CopyBufferToTexture(&bufferCopyView, &textureCopyView, &ext3D)
        .GetResult();
}

void ContextDawn::submit(int numCommands, const dawn::CommandBuffer& commands) const
{
    queue.Submit(numCommands, &commands);
}

dawn::TextureCopyView ContextDawn::createTextureCopyView(dawn::Texture texture, uint32_t level, uint32_t slice, dawn::Origin3D origin) const
{
    dawn::TextureCopyView textureCopyView;
    textureCopyView.texture = texture;
    textureCopyView.level = level;
    textureCopyView.slice = slice;
    textureCopyView.origin = origin;

    return textureCopyView;
}

shaderc_shader_kind ContextDawn::ShadercShaderKind(dawn::ShaderStage stage) const {
    switch (stage) {
    case dawn::ShaderStage::Vertex:
        return shaderc_glsl_vertex_shader;
    case dawn::ShaderStage::Fragment:
        return shaderc_glsl_fragment_shader;
    case dawn::ShaderStage::Compute:
        return shaderc_glsl_compute_shader;
    default:
        UNREACHABLE();
    }
}

dawn::ShaderModule ContextDawn::CreateShaderModuleFromResult(
    const dawn::Device& device,
    const shaderc::SpvCompilationResult& result) const {
    // result.cend and result.cbegin return pointers to uint32_t.
    const uint32_t* resultBegin = result.cbegin();
    const uint32_t* resultEnd = result.cend();
    // So this size is in units of sizeof(uint32_t).
    ptrdiff_t resultSize = resultEnd - resultBegin;
    // SetSource takes data as uint32_t*.

    dawn::ShaderModuleDescriptor descriptor;
    descriptor.codeSize = static_cast<uint32_t>(resultSize);
    descriptor.code = result.cbegin();
    return device.CreateShaderModule(&descriptor);
}

dawn::ShaderModule ContextDawn::createShaderModule(dawn::ShaderStage stage, const std::string & str, const std::string & shaderId) const
{
    shaderc_shader_kind kind = ShadercShaderKind(stage);

    shaderc::Compiler compiler;
    auto result = compiler.CompileGlslToSpv(str.c_str(), str.length(), kind, shaderId.c_str());
    if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
        std::cerr << result.GetErrorMessage();
        return {};
    }
    return CreateShaderModuleFromResult(device, result);
}

dawn::BindGroupLayout ContextDawn::MakeBindGroupLayout(
    std::initializer_list<dawn::BindGroupLayoutBinding> bindingsInitializer) const {
    dawn::ShaderStageBit kNoStages{};

    std::vector<dawn::BindGroupLayoutBinding> bindings;
    for (const dawn::BindGroupLayoutBinding& binding : bindingsInitializer) {
        if (binding.visibility != kNoStages) {
            bindings.push_back(binding);
        }
    }

    dawn::BindGroupLayoutDescriptor descriptor;
    descriptor.numBindings = static_cast<uint32_t>(bindings.size());
    descriptor.bindings = bindings.data();
    return device.CreateBindGroupLayout(&descriptor);
}

dawn::PipelineLayout ContextDawn::MakeBasicPipelineLayout(
    std::vector<dawn::BindGroupLayout> bindingsInitializer) const {
    dawn::PipelineLayoutDescriptor descriptor;

    descriptor.numBindGroupLayouts = bindingsInitializer.size();
    descriptor.bindGroupLayouts = bindingsInitializer.data();
    
    return device.CreatePipelineLayout(&descriptor);
}

dawn::InputState ContextDawn::createInputState(std::initializer_list<Attribute> attributeInitilizer, std::initializer_list<Input> inputInitilizer) const
{
    dawn::InputStateBuilder inputStateBuilder = device.CreateInputStateBuilder();

    const Input* input = inputInitilizer.begin();
    const Attribute* attribute = attributeInitilizer.begin();
    for (; input != inputInitilizer.end(); ++input, ++attribute)
    {
        inputStateBuilder.SetAttribute(attribute->shaderLocation, attribute->bindingSlot, attribute->format, attribute->offset);
        inputStateBuilder.SetInput(input->bindingSlot, input->stride, input->stepMode);
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

    dawn::AttachmentDescriptor cColorAttachments[kMaxColorAttachments];
    dawn::AttachmentDescriptor cDepthStencilAttachment;
    dawn::BlendState cBlendStates[kMaxColorAttachments];

    dawn::AttachmentsStateDescriptor cAttachmentsState;
    cAttachmentsState.numColorAttachments = 1;
    cAttachmentsState.colorAttachments = cColorAttachments;
    cAttachmentsState.depthStencilAttachment = &cDepthStencilAttachment;
    cAttachmentsState.hasDepthStencilAttachment = true;

    for (uint32_t i = 0; i < kMaxColorAttachments; ++i) {
        cColorAttachments[i].format = preferredSwapChainFormat;
    }

    cDepthStencilAttachment.format = dawn::TextureFormat::D32FloatS8Uint;

    for (uint32_t i = 0; i < kMaxColorAttachments; ++i) {
        cBlendStates[i] = device.CreateBlendStateBuilder().GetResult();
    }

    dawn::RenderPipelineDescriptor descriptor;
    descriptor.layout = pipelineLayout;
    descriptor.vertexStage = &cVertexStage;
    descriptor.fragmentStage = &cFragmentStage;
    descriptor.attachmentsState = &cAttachmentsState;
    descriptor.inputState = inputState;
    descriptor.depthStencilState = depthStencilState;
    descriptor.primitiveTopology = dawn::PrimitiveTopology::TriangleList;
    descriptor.indexFormat = dawn::IndexFormat::Uint16;
    descriptor.sampleCount = 1;
    descriptor.numBlendStates = 1;
    descriptor.blendStates = cBlendStates;

    if (enableBlend) {
        cBlendStates[0] = blendState;
    }

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
    descriptor.arraySize = 1;
    descriptor.sampleCount = 1;
    descriptor.format = dawn::TextureFormat::D32FloatS8Uint;
    descriptor.levelCount = 1;
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

ContextDawn::BindingInitializationHelper::BindingInitializationHelper(uint32_t binding,
    const dawn::Sampler& sampler)
    : binding(binding), sampler(sampler) {
}

ContextDawn::BindingInitializationHelper::BindingInitializationHelper(uint32_t binding,
    const dawn::TextureView& textureView)
    : binding(binding), textureView(textureView) {
}

ContextDawn::BindingInitializationHelper::BindingInitializationHelper(uint32_t binding,
    const dawn::Buffer& buffer,
    uint32_t offset,
    uint32_t size)
    : binding(binding), buffer(buffer), offset(offset), size(size) {
}

dawn::BindGroupBinding ContextDawn::BindingInitializationHelper::GetAsBinding() const {
    dawn::BindGroupBinding result;

    result.binding = binding;
    result.sampler = sampler;
    result.textureView = textureView;
    result.buffer = buffer;
    result.offset = offset;
    result.size = size;

    return result;
}

dawn::BindGroup ContextDawn::makeBindGroup(const dawn::BindGroupLayout & layout, std::initializer_list<BindingInitializationHelper> bindingsInitializer) const
{
    std::vector<dawn::BindGroupBinding> bindings;
    for (const BindingInitializationHelper& helper : bindingsInitializer) {
        bindings.push_back(helper.GetAsBinding());
    }

    dawn::BindGroupDescriptor descriptor;
    descriptor.layout = layout;
    descriptor.numBindings = bindings.size();
    descriptor.bindings = bindings.data();

    return device.CreateBindGroup(&descriptor);
}

void ContextDawn::initGeneralResources(Aquarium* aquarium)
{
    // initilize general uniform buffers
    groupLayoutGeneral = MakeBindGroupLayout({
        { 0, dawn::ShaderStageBit::Vertex, dawn::BindingType::UniformBuffer },
        { 1, dawn::ShaderStageBit::Fragment, dawn::BindingType::UniformBuffer },
        { 2, dawn::ShaderStageBit::Fragment, dawn::BindingType::UniformBuffer },
    });

    lightWorldPositionBuffer = utils::CreateBufferFromData(device, &aquarium->lightWorldPositionUniform, sizeof(aquarium->lightWorldPositionUniform), dawn::BufferUsageBit::TransferDst | dawn::BufferUsageBit::Uniform);


    lightWorldPositionBuffer = createBufferFromData(&aquarium->lightWorldPositionUniform, sizeof(aquarium->lightWorldPositionUniform), dawn::BufferUsageBit::TransferDst | dawn::BufferUsageBit::Uniform);
    viewBuffer = createBufferFromData(&aquarium->viewUniforms, sizeof(aquarium->viewUniforms), dawn::BufferUsageBit::TransferDst | dawn::BufferUsageBit::Uniform);
    lightBuffer = createBufferFromData(&aquarium->lightUniforms, sizeof(aquarium->lightUniforms), dawn::BufferUsageBit::TransferDst | dawn::BufferUsageBit::Uniform);
    fogBuffer = createBufferFromData(&aquarium->fogUniforms, sizeof(aquarium->fogUniforms), dawn::BufferUsageBit::TransferDst | dawn::BufferUsageBit::Uniform);

    bindGroupGeneral = makeBindGroup(groupLayoutGeneral, {
        {0, lightWorldPositionBuffer, 0, sizeof(aquarium->lightWorldPositionUniform) },
        {1, lightBuffer, 0, sizeof(aquarium->lightUniforms) },
        {2, fogBuffer , 0, sizeof(aquarium->fogUniforms) }
    });

    setBufferData(lightWorldPositionBuffer, 0, sizeof(LightWorldPositionUniform), &aquarium->lightWorldPositionUniform);
    setBufferData(lightBuffer, 0, sizeof(LightUniforms), &aquarium->lightUniforms);
    setBufferData(fogBuffer, 0, sizeof(FogUniforms), &aquarium->fogUniforms);

    // initilize world uniform buffers
    groupLayoutWorld = MakeBindGroupLayout({ { 0, dawn::ShaderStageBit::Vertex, dawn::BindingType::UniformBuffer } });
    
    bindGroupWorld = makeBindGroup(groupLayoutWorld, {
        {0, viewBuffer, 0, sizeof(aquarium->viewUniforms) },
    });

    depthStencilState = device.CreateDepthStencilStateBuilder()
        .SetDepthCompareFunction(dawn::CompareFunction::Less)
        .SetDepthWriteEnabled(true)
        .GetResult();
}

void ContextDawn::updateWorldlUniforms(Aquarium* aquarium)
{
    setBufferData(viewBuffer, 0, sizeof(ViewUniforms), &aquarium->viewUniforms);
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

void ContextDawn::generateBuffer(unsigned int * buf)
{
}

void ContextDawn::deleteBuffer(unsigned int * buf)
{
}

void ContextDawn::bindBuffer(unsigned int target, unsigned int buf)
{
}

void ContextDawn::uploadBuffer(unsigned int target, const std::vector<float>& buf)
{
}

Program *ContextDawn::createProgram(std::string vId, std::string fId)
{
    ProgramDawn* program = new ProgramDawn(this, vId, fId);
    program->loadProgram();

    return program;
}

void ContextDawn::generateProgram(unsigned int * program)
{
}

void ContextDawn::setProgram(unsigned int program)
{
}

void ContextDawn::deleteProgram(unsigned int * program)
{
}

bool ContextDawn::compileProgram(unsigned int programId, const string & VertexShaderCode, const string & FragmentShaderCode)
{
    return false;
}

void ContextDawn::setWindowTitle(const std::string &text) {}

bool ContextDawn::ShouldQuit() { return false; }

void ContextDawn::KeyBoardQuit() {}

// Submit commands of the frame
void ContextDawn::DoFlush() {

    //renderPass.EndPass();
    //dawn::CommandBuffer cmd = commandBuilder.GetResult();
    //queue.Submit(1, &cmd);

    //swapchain.Present(backbuffer);
    std::cout << "present" << std::endl;

    //glfwSwapBuffers(mWindow);
    glfwPollEvents();
}

void ContextDawn::Terminate() {}

// Update backbuffer and renderPassDescriptor
void ContextDawn::resetState() {
    //GetNextRenderPassDescriptor(device, swapchain, depthStencilView, &backbuffer, &renderPassDescriptor);
    //commandBuilder = device.CreateCommandBufferBuilder();
    //renderPass = commandBuilder.BeginRenderPass(renderPassDescriptor);
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
    depthStencilAttachment.attachment = depthStencilView;
    depthStencilAttachment.depthLoadOp = dawn::LoadOp::Clear;
    depthStencilAttachment.stencilLoadOp = dawn::LoadOp::Clear;
    depthStencilAttachment.clearDepth = 1.0f;
    depthStencilAttachment.clearStencil = 0;
    depthStencilAttachment.depthStoreOp = dawn::StoreOp::Store;
    depthStencilAttachment.stencilStoreOp = dawn::StoreOp::Store;

    *info = device.CreateRenderPassDescriptorBuilder()
        .SetColorAttachments(1, &colorAttachment)
        .SetDepthStencilAttachment(&depthStencilAttachment)
        .GetResult();
}

void ContextDawn::enableBlend(bool flag) const {}

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

void ContextDawn::setTexture(const TextureDawn * texture, int index, int unit) const
{
}

void ContextDawn::setAttribs(BufferDawn * bufferGL, int index) const
{
}

void ContextDawn::setIndices(BufferDawn * bufferGL) const
{
}

void ContextDawn::drawElements(BufferDawn * buffer) const
{
}
